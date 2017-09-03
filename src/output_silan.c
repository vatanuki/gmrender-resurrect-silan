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

#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <ctype.h>

#include "logging.h"
#include "upnp_connmgr.h"
#include "output_module.h"
#include "output_silan.h"
#include "swa_msg_api.h"

static output_transition_cb_t play_trans_callback = NULL;

struct swa_ipc_msg{
	long type;
	struct swa_ipc_data ipc_data;
	char ipc[IPC_NUM];
};

static struct {
	int state;
	int status;
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

static int output_silan_send_cmd(char cmd, int param, const char *data){
	int bit, cnt;
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
	memset(&msg.ipc, 0, IPC_NUM);

#ifdef SILAN_IPC_LOGGING
	Log_info("silan_send_cmd", "cmd: %d, param: %d, data: %s\n", msg.ipc_data.cmd, msg.ipc_data.param, msg.ipc_data.data);
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
		default:
			bit = 0;
	}

	silan_data.status|= bit;
	if(msgsnd(silan_data.send_id, &msg, sizeof(msg), 0) < 0)
		return -1;

	cnt = 0;
	while((silan_data.status & bit) && cnt < SILAN_RECV_RETRY){
		cnt++;
#ifdef SILAN_IPC_LOGGING
		Log_info("silan_send_cmd", "waiting cmd (%d) data to be ready (%d)", cmd, cnt);
#endif
		usleep(SILAN_RECV_TOUT);
	}

	return bit && cnt >= SILAN_RECV_RETRY ? -1 : 0;
}

static int output_silan_loop(void){
	struct swa_ipc_msg msg;

	while(1){
		if(msgrcv(silan_data.recv_id, &msg, sizeof(msg), 0, 0) < 0){
			error(0, errno, "%s msgrcv failed", __FUNCTION__);
			sleep(500);
			continue;
		}

		if(msg.ipc_data.des == SWA_SIGNAL_APK){
#ifdef SILAN_IPC_LOGGING
			Log_info("silan_recv_cmd", "[SKIP] dst: %d, cmd: %d, param: %d, data: %s", msg.ipc_data.des, msg.ipc_data.cmd, msg.ipc_data.param, msg.ipc_data.data);
#endif
			continue;
		}

#ifdef SILAN_IPC_LOGGING
		Log_info("silan_recv_cmd", "dst: %d, cmd: %d, param: %d, data: %s", msg.ipc_data.des, msg.ipc_data.cmd, msg.ipc_data.param, msg.ipc_data.data);
#endif

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

				if(silan_data.state == SLMP_STATE_STOP && (silan_data.status & SILAN_DATA_PLAYING)){
					Log_info("silan", "End-of-stream");

					if(silan_data.uri_next){
						if(silan_data.uri)
							free(silan_data.uri);
						silan_data.uri = silan_data.uri_next;
						silan_data.uri_next = NULL;

						Log_info("silan", "play next uri %s", silan_data.uri);
						output_silan_send_cmd(CMD_SlmpPlayUri, 0, silan_data.uri);
						if(play_trans_callback)
							play_trans_callback(PLAY_STARTED_NEXT_STREAM);

					}else if(play_trans_callback){
						silan_data.status&= ~SILAN_DATA_PLAYING;
						play_trans_callback(PLAY_STOPPED);
						output_silan_send_cmd(CMD_SpdifIn, 0, NULL);
					}
				}

				break;
		}
	}

	return 0;
}

static int output_silan_init(void){
	silan_data.send_id = msgget(SWA_SIGNAL_SLMP, IPC_CREAT | 0666);
	assert(silan_data.send_id >= 0);

	silan_data.recv_id = msgget(SWA_MSG_DAEMON, IPC_CREAT | 0666);
	assert(silan_data.recv_id >= 0);

	pthread_t thread;
	pthread_create(&thread, NULL, output_silan_loop, NULL);

	silan_data.state = SLMP_STATE_STOP;
	silan_data.status = 0;
	silan_data.uri = NULL;
	silan_data.uri_next = NULL;
	silan_data.mute = 0;
	silan_data.volume = 0;
	silan_data.duration = 0;
	silan_data.position = 0;
	silan_data.padmux = 1;

	//output_silan_send_cmd(CMD_SpdifIn, 0, NULL);
	//output_silan_send_cmd(CMD_SetPadMux, silan_data.padmux, NULL);

	register_mime_type("audio/*");

	Log_info("silan", "init done");

	return 0;
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

static int output_silan_play(output_transition_cb_t callback) {
	int res;

	Log_info("silan", "play");

	play_trans_callback = callback;
	silan_data.status|= SILAN_DATA_PLAYING;

	if(silan_data.uri){
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

	silan_data.status&= ~SILAN_DATA_PLAYING;
	output_silan_send_cmd(CMD_SlmpStop, 0, NULL);
	output_silan_send_cmd(CMD_SpdifIn, 0, NULL);
	//output_silan_send_cmd(CMD_SetPadMux, silan_data.padmux, NULL);

	return 0;
}

static int output_silan_pause(void) {
	Log_info("silan", "pause");
	return output_silan_send_cmd(CMD_SlmpPause, 0, NULL);
}

static int output_silan_seek(int position) {
	Log_info("silan", "seek position to %d seconds", position);
	return output_silan_send_cmd(CMD_SlmpSeek, position, NULL);
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
	if(output_silan_send_cmd(CMD_SlmpGetVolume, 0, NULL) < 0){
		Log_error("silan", "CMD_SlmpGetVolume");
		return -1;
	}

	*v = silan_data.volume;

	Log_info("silan", "Get volume: %d", *v);
	return 0;
}

static int output_silan_set_volume(int v) {
	Log_info("silan", "Set volume to %d", v);

	silan_data.mute = 0;
	return output_silan_send_cmd(CMD_SlmpSetVolume, v, NULL);
}

static int output_silan_get_mute(int *m) {
	if(output_silan_send_cmd(CMD_SlmpGetMute, 0, NULL) < 0){
		Log_error("silan", "CMD_SlmpGetMute");
		return -1;
	}

	*m = silan_data.mute;
	Log_info("silan", "Get mute: %s", *m ? "on" : "off");
	return 0;
}

static int output_silan_set_mute(int m) {
	int res = 0;
	Log_info("silan", "Set mute to %s", m ? "on" : "off");
	if(!!m != !!silan_data.mute)
		res = output_silan_send_cmd(CMD_SlmpMute, m, NULL);
	silan_data.mute = m;
	return res;
}




























/*
static gchar *audio_sink = NULL;
static gchar *audio_device = NULL;
static gchar *videosink = NULL;
static double initial_db = 0.0;
*/
/* Options specific to output_silan */
/*
static GOptionEntry option_entries[] = {
        { "gstout-audiosink", 0, 0, G_OPTION_ARG_STRING, &audio_sink,
          "silan audio sink to use "
	  "(autoaudiosink, alsasink, osssink, esdsink, ...)",
	  NULL },
        { "gstout-audiodevice", 0, 0, G_OPTION_ARG_STRING, &audio_device,
          "silan device for the given audiosink. ",
	  NULL },
        { "gstout-videosink", 0, 0, G_OPTION_ARG_STRING, &videosink,
          "silan video sink to use "
	  "(autovideosink, xvimagesink, ximagesink, ...)",
	  NULL },
        { "gstout-initial-volume-db", 0, 0, G_OPTION_ARG_DOUBLE, &initial_db,
          "silan inital volume in decibel (e.g. 0.0 = max; -6 = 1/2 max) ",
	  NULL },
        { NULL }
};
*/

/*static int output_silan_add_options(GOptionContext *ctx)
{
	GOptionGroup *option_group;
	option_group = g_option_group_new("gstout", "silan Output Options",
	                                  "Show silan Output Options",
	                                  NULL, NULL);
	g_option_group_add_entries(option_group, option_entries);

	g_option_context_add_group (ctx, option_group);
	
	g_option_context_add_group (ctx, gst_init_get_option_group ());

	return 0;
}
*/

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
	.get_position = output_silan_get_position,
	.get_volume   = output_silan_get_volume,
	.set_volume   = output_silan_set_volume,
	.get_mute     = output_silan_get_mute,
	.set_mute     = output_silan_set_mute,
};
