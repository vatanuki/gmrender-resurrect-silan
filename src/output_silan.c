#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/select.h>

#include "logging.h"
#include "upnp_control.h"
#include "upnp_connmgr.h"
#include "output_module.h"
#include "output_silan.h"
#include "swa_msg_api.h"
#include "swa_gpio_api.h"

static output_transition_cb_t trans_callback = NULL;
static int output_silan_send_cmd(char cmd, int param, const char *data);
static void output_silan_set_spdifin(void);
static void output_silan_set_amp_mute(int m);
static void output_silan_set_line_mute(int m);
static void output_silan_set_amp_power(int m);
extern char *strcasestr(const char *, const char *);

struct swa_ipc_msg{
	long type;
	struct swa_ipc_data ipc_data;
	char ipc[IPC_NUM];
};

struct spdifin_status_info{
	int lock;
	int rate;
	int rawdata;
	int set_padmux;
};

static struct {
	int state;
	int status;
	int input_fd;
	int gpio_fd;
	int send_id;
	int recv_id;
	char *uri;
	char *uri_next;
	int mute;
	int volume;
	int duration;
	int position;
	int padmux;
} silan_data;

#if 0
static int output_silan_gpio_read(int num){
	struct gpio_data_t gd = {num, -2};
	return ioctl(silan_data.gpio_fd, GPIO_IOCTL_READ, &gd) < 0 ? -3 : gd.value;
}
#endif

static int output_silan_gpio_write(int num, int value){
	struct gpio_data_t gd = {num, value};
	return ioctl(silan_data.gpio_fd, GPIO_IOCTL_WRITE, &gd);
}

static int output_silan_spdifin_info(struct spdifin_status_info *info){
	int fd = open(SILAN_FNAME_SPDIF_IN_INFO, O_RDONLY);
	fd_set set;
	struct timeval to;

	if(fd < 0){
		Log_error("silan_spdifin", "failed to open %s | %s", SILAN_FNAME_SPDIF_IN_INFO, strerror(errno));
		return -1;
	}

	to.tv_sec = 0;
	to.tv_usec = 500;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	if(select(fd + 1, &set, NULL, NULL, &to) > 0 && read(fd, info, sizeof(struct spdifin_status_info)) == sizeof(struct spdifin_status_info)){
#ifdef SILAN_DEBUG
		Log_info("silan_spdifin", "info: padmux=%d, lock=%d, rate=%d, rawdata=0x%08x", info->set_padmux, info->lock, info->rate, info->rawdata);
#endif
		close(fd);
		return 0;

	}else{
		Log_error("silan_spdifin", "failed to read %s | %s", SILAN_FNAME_SPDIF_IN_INFO, strerror(errno));
	}

	close(fd);
	return -1;
}

#if 0
static int output_silan_check_spdifin(int padmux, int count, int threshold){
	struct gpio_data_t gd = {GPIO_SPDIF_IN0 + padmux, 1};
	int value = 1;

	while(count-- > 0){
		ioctl(silan_data.gpio_fd, GPIO_IOCTL_READ, &gd);
		if(value != gd.value && threshold-- <= 0){
#ifdef SILAN_SIGNAL_LOGGING
			Log_info("silan_spdifin", "signal @ %d",count);
#endif
			return 1;
		}
		value = gd.value;
	}

#ifdef SILAN_SIGNAL_LOGGING
	Log_info("silan_spdifin", "no signal @ %d", threshold);
#endif
	return 0;
}
#endif

static void output_silan_volume_change(int dir){
	if(silan_data.volume > 50) dir*= 5;
	else if(silan_data.volume > 20) dir*= 4;
	else if(silan_data.volume > 10) dir*= 2;
	silan_data.volume+= dir;
	if(silan_data.volume < 0) silan_data.volume = 0;
	if(silan_data.volume > 100) silan_data.volume = 100;
	upnp_control_set_volume(silan_data.volume);
}

static int output_silan_loop(void){
	int cnt_stop = 0, cnt_unlock = 0, cnt_mute = 0;
	fd_set set;
	struct timeval to;
	struct input_event ev;
	struct spdifin_status_info info;

	while(1){
		to.tv_sec = 1;
		to.tv_usec = 0;

		if(silan_data.input_fd >= 0){
			FD_ZERO(&set);
			FD_SET(silan_data.input_fd, &set);
		}

		if(silan_data.input_fd >= 0 && select(silan_data.input_fd + 1, &set, NULL, NULL, &to) > 0 && read(silan_data.input_fd, &ev, sizeof(ev)) == sizeof(ev)){
#ifdef SILAN_IR_LOGGING
			Log_info("silan_ir", "type: %d, code: %d, value: %d", ev.type, ev.code, ev.value);
#endif

			if(ev.type != EV_KEY || ev.value != 1)
				continue;

			switch(ev.code){
				case KEY_RADIO:
					output_silan_set_spdifin();
#ifdef SILAN_IR_LOGGING
					Log_info("silan_ir", "set spdifin %d", silan_data.padmux);
#endif
					if(trans_callback){
#ifdef SILAN_IR_LOGGING
						Log_info("silan_ir", "callback: STOPPED due SPDIFIN");
#endif
						trans_callback(PLAY_STOPPED);
					}
					break;

				case KEY_VOLUMEUP:
					if(silan_data.volume < 100)
						output_silan_volume_change(+1);
#ifdef SILAN_IR_LOGGING
					Log_info("silan_ir", "set volume %d", silan_data.volume);
#endif
					break;

				case KEY_VOLUMEDOWN:
					if(silan_data.volume > 0)
						output_silan_volume_change(-1);
#ifdef SILAN_IR_LOGGING
					Log_info("silan_ir", "set volume %d", silan_data.volume);
#endif
					break;

				case KEY_MUTE:
					upnp_control_set_mute(!silan_data.mute);
#ifdef SILAN_IR_LOGGING
					Log_info("silan_ir", "set mute %d", silan_data.mute ? 0 : 1);
#endif
					break;
			}
		}else{
			if(silan_data.input_fd < 0)
				sleep(to.tv_sec);

			if(silan_data.status & SILAN_STATUS_SPDIFIN){
				if(!output_silan_spdifin_info(&info) && info.lock){
					cnt_unlock = 0;
					if(!(silan_data.status & SILAN_STATUS_AMP_POWER)){
						cnt_mute = 0;
						output_silan_set_amp_power(1);
					}

				}else if(cnt_unlock < SILAN_AUTO_POWER_OFF){
					cnt_unlock++;
					Log_info("silan_wd", "no SPDIF IN @ %d", cnt_unlock);

				}else if(cnt_unlock == SILAN_AUTO_POWER_OFF){
					cnt_unlock++;
					if(silan_data.status & SILAN_STATUS_AMP_POWER)
						output_silan_set_amp_power(0);
				}
			}else{
				if(silan_data.state == SLMP_STATE_PLAY){
					cnt_stop = 0;
					if(!(silan_data.status & SILAN_STATUS_AMP_POWER)){
						cnt_mute = 0;
						output_silan_set_amp_power(1);
					}

				}else if(cnt_stop < SILAN_AUTO_SPDIFIN){
					cnt_stop++;
					Log_info("silan_wd", "not playing @ %d", cnt_stop);

				}else if(cnt_stop == SILAN_AUTO_SPDIFIN){
					cnt_stop++;
					output_silan_set_spdifin();
				}
			}

			if((silan_data.status & (SILAN_STATUS_AMP_POWER|SILAN_STATUS_AMP_MUTE)) == (SILAN_STATUS_AMP_POWER|SILAN_STATUS_AMP_MUTE)){
				if(cnt_mute < SILAN_AMP_UNMUTE){
					cnt_mute++;
					Log_info("silan_wd", "AMP still muted @ %d", cnt_mute);
				}else if(cnt_mute == SILAN_AMP_UNMUTE){
					cnt_mute++;
					output_silan_set_amp_mute(0);
				}
			}
		}
	}

#if 0
	while(1){
		btn1 = output_silan_gpio_read(GPIO_BTN1);
		btn2 = output_silan_gpio_read(GPIO_BTN2);
		if(!btn1 || !btn2)
			Log_info("silan_loop", "buttons: %d, %d", btn1, btn2);
		usleep(100000);
	}
#endif

	return 0;
}

static int output_silan_send_cmd(char cmd, int param, const char *data){
	int bit = 0, cnt;
	const char *cmd_str = NULL;
	struct swa_ipc_msg msg;

	msg.ipc_data.src = SWA_SIGNAL_UI;
	msg.ipc_data.des = SWA_SIGNAL_SLMP;
	msg.ipc_data.cmd = cmd;
	msg.ipc_data.param = param;

	if(data)
		strcpy(msg.ipc_data.data, data);
	else
		msg.ipc_data.data[0] = '\0';

	msg.type = msg.ipc_data.src;
	memset(msg.ipc, 0, IPC_NUM);

#ifdef SILAN_IPC_LOGGING
	switch(cmd){
		case CMD_SlmpGetMute:
			cmd_str = "CMD_SlmpGetMute";
			break;
		case CMD_SlmpGetVolume:
			cmd_str = "CMD_SlmpGetVolume";
			break;
		case CMD_SlmpGetTimePos:
			cmd_str = "CMD_SlmpGetTimePos";
			break;
		case CMD_DmrGetTotalTime:
			cmd_str = "CMD_DmrGetTotalTime";
			break;
		case CMD_SpdifIn:
			cmd_str = "CMD_SpdifIn";
			break;
		case CMD_SetPadMux:
			cmd_str = "CMD_SetPadMux";
			break;
		case CMD_SlmpPlayUri:
			cmd_str = "CMD_SlmpPlayUri";
			break;
		case CMD_SlmpPause:
			cmd_str = "CMD_SlmpPause";
			break;
		case CMD_SlmpStop:
			cmd_str = "CMD_SlmpStop";
			break;
		case CMD_SlmpSeek:
			cmd_str = "CMD_SlmpSeek";
			break;
		case CMD_SlmpMute:
			cmd_str = "CMD_SlmpMute";
			break;
		case CMD_SlmpSetVolume:
			cmd_str = "CMD_SlmpSetVolume";
			break;
		default:
			cmd_str = "CMD_UNKNOWN";
			break;
	}

	Log_info("silan-send", "%s (%d) param: %d, data: %s", cmd_str, msg.ipc_data.cmd, msg.ipc_data.param, msg.ipc_data.data);
#endif

	switch(cmd){
		case CMD_SlmpGetMute:
			bit = SILAN_DATA_MUTE;
			break;
		case CMD_SlmpGetVolume:
			bit = SILAN_DATA_VOLUME;
			break;
		case CMD_SlmpGetTimePos:
			bit = SILAN_DATA_POSITION;
			break;
		case CMD_DmrGetTotalTime:
			bit = SILAN_DATA_DURATION;
			break;
	}

	silan_data.status|= bit;
	if(msgsnd(silan_data.send_id, &msg, sizeof(msg), 0) < 0)
		return -1;

	cnt = 0;
	while((silan_data.status & bit) && cnt < SILAN_RECV_RETRY){
		cnt++;
#ifdef SILAN_IPC_LOGGING
		Log_info("silan-send", "waiting %s (%d) data to be ready (%d)", cmd_str, cmd, cnt);
#endif
		usleep(SILAN_RECV_TOUT);
	}

	return bit && cnt >= SILAN_RECV_RETRY ? -1 : 0;
}

static void *output_silan_recv_cmd(void *argv){
	struct swa_ipc_msg msg;
	const char *cmd_str = NULL;
#ifdef SILAN_DEBUG
	int ipc_num_i;
	char ipc_num_str[IPC_NUM*2+1];
#endif

	while(1){
		if(msgrcv(silan_data.recv_id, &msg, sizeof(msg), 0, 0) < 0){
			error(0, errno, "%s msgrcv failed", __FUNCTION__);
			sleep(500);
			continue;
		}

#ifdef SILAN_IPC_LOGGING
		switch(msg.ipc_data.cmd){
			case CMD_SlmpGetMute:
				cmd_str = "CMD_SlmpGetMute";
				break;
			case CMD_SlmpGetVolume:
				cmd_str = "CMD_SlmpGetVolume";
				break;
			case CMD_SlmpSetTimePos:
				cmd_str = "CMD_SlmpSetTimePos";
				break;
			case CMD_SlmpSetTotalTime:
				cmd_str = "CMD_DmrSetTotalTime";
				break;
			case CMD_SlmpPlayState:
				cmd_str = "CMD_SlmpPlayState";
				break;
			case CMD_SlmpPcmInfo:
				cmd_str = "CMD_SlmpPcmInfo";
				break;
			case CMD_SpdInBegM:
				cmd_str = "CMD_SpdInBegM";
				break;
			default:
				cmd_str = "CMD_UNKNOWN";
				break;
		}

		if(msg.ipc_data.cmd == CMD_SlmpPlayState){
			switch(msg.ipc_data.param){
				case 0:
					cmd_str = "CMD_SlmpPlayState - STOP";
					break;
				case 1:
					cmd_str = "CMD_SlmpPlayState - PAUSE";
					break;
				case 2:
					cmd_str = "CMD_SlmpPlayState - PLAY";
					break;
				case 3:
					cmd_str = "CMD_SlmpPlayState - DMR STOP";
					break;
				case 4:
					cmd_str = "CMD_SlmpPlayState - DMR PAUSE";
					break;
				case 5:
					cmd_str = "CMD_SlmpPlayState - DMR PLAY";
					break;
				case 6:
					cmd_str = "CMD_SlmpPlayState - FINISH";
					break;
				case 8:
					cmd_str = "CMD_SlmpPlayState - NEXT";
					break;
				case 9:
					cmd_str = "CMD_SlmpPlayState - PREV";
					break;
			}
		}

#ifndef SILAN_DEBUG
		if(msg.ipc_data.des == SWA_SIGNAL_UI)
#endif
		Log_info("silan-recv", "%s (%d) dst: %d, param: %d, data: %s", cmd_str, msg.ipc_data.cmd, msg.ipc_data.des, msg.ipc_data.param, msg.ipc_data.data);
#ifdef SILAN_DEBUG
		for(ipc_num_i = 0; ipc_num_i < IPC_NUM; ipc_num_i++)
			snprintf(&ipc_num_str[ipc_num_i * 2], 3, "%02x", msg.ipc[ipc_num_i]);
		Log_info("silan-recv", "IPC NUM DEBUG: 0x%s", ipc_num_str);
#endif
#endif

		if(msg.ipc_data.des != SWA_SIGNAL_UI)
			continue;

		switch(msg.ipc_data.cmd){
			case CMD_SlmpGetMute:
				silan_data.mute = msg.ipc_data.param;
				silan_data.status&= ~SILAN_DATA_MUTE;
				break;

			case CMD_SlmpGetVolume:
				silan_data.volume = msg.ipc_data.param;
				silan_data.status&= ~SILAN_DATA_VOLUME;
				break;

			case CMD_SlmpSetTimePos:
				silan_data.position = msg.ipc_data.param;
				silan_data.status&= ~SILAN_DATA_POSITION;
				break;

			case CMD_SlmpSetTotalTime:
				silan_data.duration = msg.ipc_data.param;
				silan_data.status&= ~SILAN_DATA_DURATION;
				break;

			case CMD_SlmpPlayState:
				silan_data.state = msg.ipc_data.param;

				if(silan_data.state == SLMP_STATE_STOP && silan_data.duration && silan_data.position == silan_data.duration){
					Log_info("silan", "End-of-stream");

					if(silan_data.uri_next){
						if(silan_data.uri)
							free(silan_data.uri);
						silan_data.uri = silan_data.uri_next;
						silan_data.uri_next = NULL;

						Log_info("silan", "play next uri %s", silan_data.uri);
						output_silan_send_cmd(CMD_SlmpPlayUri, 0, silan_data.uri);
						if(trans_callback){
							Log_info("silan", "callback: NEXT STREAM");
							trans_callback(PLAY_STARTED_NEXT_STREAM);
						}

					}else if(trans_callback){
						Log_info("silan", "callback: STOPPED");
						trans_callback(PLAY_STOPPED);
					}
				}

				break;
		}
	}

	return 0;
}

static char *output_silan_read_file(const char *fname){
	FILE *fd;
	struct stat fs;
	char *buf = NULL;

	fd = fopen(fname, "r");
	if(fd < 0)
		return NULL;

	if(stat(fname, &fs) == 0 && fs.st_size > 0 && (buf=malloc(fs.st_size+1))){
		buf[fs.st_size] = '\0';
		if(fread(buf, fs.st_size, 1, fd) != 1){
			free(buf);
			buf = NULL;
		}
	}

	fclose(fd);

	return buf;
}

static int output_silan_init(void){
	char *buf, *pos;
	struct gpio_data_t gd;

	silan_data.gpio_fd = open(DLNA_GPIO_NAME, O_RDONLY);
	assert(silan_data.gpio_fd >= 0);

	gd.value = 0;
	gd.num = GPIO_BTN1; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_IN, &gd);
	gd.num = GPIO_BTN2; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_IN, &gd);

	gd.value = 1;
	gd.num = GPIO_MUTE; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_OUT, &gd);
	gd.num = GPIO_LED2; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_OUT, &gd);
	gd.num = GPIO_LED3; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_OUT, &gd);
	gd.num = GPIO_LED4; ioctl(silan_data.gpio_fd, GPIO_IOCTL_SET_OUT, &gd);

	silan_data.state = SLMP_STATE_STOP;
	silan_data.status = 0;
	silan_data.uri = NULL;
	silan_data.uri_next = NULL;
	silan_data.mute = 0;
	silan_data.volume = 0;
	silan_data.duration = 0;
	silan_data.position = 0;
	silan_data.padmux = 0;

	buf = output_silan_read_file(SILAN_FNAME_DB_SLMP);
	if(buf){
		pos = strcasestr(buf, SILAN_SLMP_KEY_PADMUX);
		if(pos != NULL){
			silan_data.padmux = atoi(pos + sizeof(SILAN_SLMP_KEY_PADMUX) - 1);
			Log_info("silan-init", "padmux=%d", silan_data.padmux);
		}
		free(buf);
	}

	buf = output_silan_read_file(SILAN_FNAME_DB_SYSTEM);
	if(buf){
		pos = strcasestr(buf, SILAN_SYSTEM_KEY_VOLUME);
		if(pos != NULL){
			silan_data.volume = atoi(pos + sizeof(SILAN_SYSTEM_KEY_VOLUME) - 1);
			Log_info("silan-init", "volume=%d", silan_data.volume);
		}
		pos = strcasestr(buf, SILAN_SYSTEM_KEY_MUTE);
		if(pos != NULL){
			silan_data.mute = atoi(pos + sizeof(SILAN_SYSTEM_KEY_MUTE) - 1);
			Log_info("silan-init", "mute=%d", silan_data.mute);
		}
		free(buf);
	}

	silan_data.input_fd = open(SILAN_FNAME_IR_INPUT, O_RDONLY);
	if(silan_data.input_fd < 0)
		Log_error("silan", "failed to open %s | %s", SILAN_FNAME_IR_INPUT, strerror(errno));

	silan_data.send_id = msgget(SWA_SIGNAL_SLMP, IPC_CREAT | 0666);
	assert(silan_data.send_id >= 0);

	silan_data.recv_id = msgget(SWA_MSG_DAEMON, IPC_CREAT | 0666);
	assert(silan_data.recv_id >= 0);

	pthread_t thread;
	pthread_create(&thread, NULL, output_silan_recv_cmd, NULL);

	output_silan_set_amp_power(1);
	output_silan_set_spdifin();

	register_mime_type("audio/*");

	Log_info("silan-init", "done");

	return 0;
}

static void output_silan_set_spdifin(void){
	if(silan_data.status & SILAN_STATUS_SPDIFIN)
		return;

	silan_data.status|= SILAN_STATUS_SPDIFIN;
	output_silan_send_cmd(CMD_SetPadMux, silan_data.padmux, NULL);
	output_silan_send_cmd(CMD_SpdifIn, 0, NULL);
}

static void output_silan_set_uri(const char *uri){
	Log_info("silan", "Set uri to '%s'", uri);

	if(silan_data.uri)
		free(silan_data.uri);
	silan_data.uri = (uri && *uri) ? strdup(uri) : NULL;
}

static void output_silan_set_next_uri(const char *uri) {
	Log_info("silan", "Set next uri to '%s'", uri);

	if(silan_data.uri_next)
		free(silan_data.uri_next);
	silan_data.uri_next = (uri && *uri) ? strdup(uri) : NULL;
}

static int output_silan_play(void) {
	int res;

	Log_info("silan", "play");

	if(silan_data.uri){
		silan_data.status&= ~SILAN_STATUS_SPDIFIN;
		silan_data.duration = 0;
		silan_data.position = 0;

		res = output_silan_send_cmd(CMD_SlmpPlayUri, 0, silan_data.uri);

		free(silan_data.uri);
		silan_data.uri = NULL;

		return res;
	}

	if(silan_data.state == SLMP_STATE_PAUSE)
		return output_silan_send_cmd(CMD_SlmpPause, 0, NULL);

	return -1;
}

static int output_silan_stop(void) {
	Log_info("silan", "stop");
	return output_silan_send_cmd(CMD_SlmpStop, 0, NULL);
}

static int output_silan_pause(void) {
	Log_info("silan", "pause");
	return output_silan_send_cmd(CMD_SlmpPause, 0, NULL);
}

static int output_silan_seek(int position) {
	Log_info("silan", "seek position to %d seconds", position);
	return output_silan_send_cmd(CMD_SlmpSeek, position, NULL);
}

static void output_silan_set_transport_callback(output_transition_cb_t callback){
	trans_callback = callback;
}

static int output_silan_get_position(int *track_dur, int *track_pos) {
	if(output_silan_send_cmd(CMD_SlmpGetTimePos, 0, NULL) < 0)
		Log_error("silan", "CMD_SlmpGetTimePos");

	*track_dur = silan_data.duration;
	*track_pos = silan_data.position;

	Log_info("silan", "track position is %d / %d seconds", *track_pos, *track_dur);
	return 0;
}

static int output_silan_get_volume(int *v) {
#if 0
	if(output_silan_send_cmd(CMD_SlmpGetVolume, 0, NULL) < 0){
		Log_error("silan", "CMD_SlmpGetVolume");
		return -1;
	}
#endif

	*v = silan_data.volume;

	Log_info("silan", "Get volume: %d", *v);
	return 0;
}

static int output_silan_set_volume(int v) {
	Log_info("silan", "Set volume to %d", v);

	silan_data.mute = 0;
	silan_data.volume = v;
	return output_silan_send_cmd(CMD_SlmpSetVolume, v, NULL);
}

static int output_silan_get_mute(int *m) {
#if 0
	if(output_silan_send_cmd(CMD_SlmpGetMute, 0, NULL) < 0){
		Log_error("silan", "CMD_SlmpGetMute");
		return -1;
	}
#endif

	*m = silan_data.mute;
	Log_info("silan", "Get mute: %s", *m ? "on" : "off");
	return 0;
}

static int output_silan_set_mute(int m) {
	int res = 0;
	Log_info("silan", "Set mute to %s", m ? "on" : "off");
	if(!m != !silan_data.mute)
		res = output_silan_send_cmd(CMD_SlmpMute, m, NULL);
	silan_data.mute = m;
	return res;
}

static void output_silan_set_amp_mute(int m) {
	if(!m != !(silan_data.status & SILAN_STATUS_AMP_MUTE))
		output_silan_gpio_write(GPIO_AMP_MUTE, m);
	if(m)
		silan_data.status|= SILAN_STATUS_AMP_MUTE;
	else
		silan_data.status&= ~SILAN_STATUS_AMP_MUTE;
	Log_info("silan", "Set AMP mute to %s", m ? "on" : "off");
}

static void output_silan_set_amp_power(int m) {
	if(!m != !(silan_data.status & SILAN_STATUS_AMP_POWER)){
		output_silan_set_amp_mute(1);
		output_silan_gpio_write(GPIO_AMP_POWER, !m);
	}
	if(m)
		silan_data.status|= SILAN_STATUS_AMP_POWER;
	else
		silan_data.status&= ~SILAN_STATUS_AMP_POWER;
	Log_info("silan", "Set AMP Power to %s", m ? "on" : "off");
}
#if 0
static void output_silan_set_line_mute(int m) {
	Log_info("silan", "Set Line mute to %s", m ? "on" : "off");
	output_silan_gpio_write(GPIO_MUTE, !m);
}
#endif
struct output_module silan_output = {
	.shortname    = "slmp",
	.description  = "silan multimedia framework",
	.init         = output_silan_init,
	.set_uri      = output_silan_set_uri,
	.set_next_uri = output_silan_set_next_uri,
	.play         = output_silan_play,
	.stop         = output_silan_stop,
	.pause        = output_silan_pause,
	.seek         = output_silan_seek,
	.set_transport_callback = output_silan_set_transport_callback,
	.get_position = output_silan_get_position,
	.get_volume   = output_silan_get_volume,
	.set_volume   = output_silan_set_volume,
	.get_mute     = output_silan_get_mute,
	.set_mute     = output_silan_set_mute,
	.loop         = output_silan_loop,
};
