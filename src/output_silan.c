#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
//#include <ctype.h>



#include "logging.h"
#include "upnp_connmgr.h"
#include "output_module.h"
#include "output_silan.h"
#include "swa_msg_api.h"

static int msgid = 0;
static char *silan_uri = NULL;
static char *silan_uri_next = NULL;
static int last_known_duration = 0;
static int last_known_position = 0;
static output_transition_cb_t play_trans_callback_ = NULL;

struct swa_ipc_msg{
	long type;
	struct swa_ipc_data ipc_data;
	char ipc[IPC_NUM];
};

static int output_silan_send_cmd(char cmd, int param, const char *data){
	int res;
	struct swa_ipc_msg msg;

	msg.ipc_data.src = SWA_SIGNAL_BTN;
	msg.ipc_data.des = SWA_SIGNAL_SLMP;
	msg.ipc_data.cmd = cmd;
	msg.ipc_data.param = param;

	if(data)
		strcpy(msg.ipc_data.data, data);
	else
		msg.ipc_data.data[0] = '\0';

	msg.type = msg.ipc_data.src;
	memset(&msg.ipc, 0, IPC_NUM);

	res = msgsnd(msgid, &msg, sizeof(msg), 0);

	Log_info("silan_send_cmd", "[%d] cmd: %d, param: %d, data: %s\n", res, msg.ipc_data.cmd, msg.ipc_data.param, msg.ipc_data.data);

	return res;
}
/*
static int output_silan_signal_dispose(struct swa_ipc_data *ipc_data, void *arg){
	switch(ipc_data->cmd){
		case CMD_SlmpSetTimePos:
			if(!*ipc_data->data){
				Log_info("silan-signal", "End-of-stream");

				if(silan_uri_next){
					if(silan_uri)
						free(silan_uri);
					silan_uri = silan_uri_next;
					silan_uri_next = NULL;

					Log_info("silan-signal", "play next uri %s", silan_uri);
					swa_slmp_play(msgid, silan_uri);
					if(play_trans_callback_)
						play_trans_callback_(PLAY_STARTED_NEXT_STREAM);

				}else if(play_trans_callback_){
					play_trans_callback_(PLAY_STOPPED);
					swa_slmp_spdifin(msgid);
				}
			}
			break;

		case CMD_SlmpPlayState:

//#define SLMP_STATE_STOP        0
//#define SLMP_STATE_PAUSE       1
//#define SLMP_STATE_PLAY        2

			if(ipc_data->param == SLMP_STATE_STOP){
			}

			break;
		default:
			Log_info("silan-signal", "cmd: %d, param: %d, data: %s", ipc_data->cmd, ipc_data->param, ipc_data->data);
	}
	return 0;
}
*/
static int output_silan_init(void){
	msgid = msgget(SWA_MSG_DAEMON, IPC_CREAT | 0666);
	assert(msgid >= 0);

	output_silan_send_cmd(CMD_SpdifIn, 1, NULL);
/*
	if(audio_sink != NULL) {
		GstElement *sink = NULL;
		Log_info("silan", "Setting audio sink to %s; device=%s\n",
			 audio_sink, audio_device ? audio_device : "");
		sink = gst_element_factory_make (audio_sink, "sink");
		if (sink == NULL) {
		  Log_error("silan", "Couldn't create sink '%s'",
			    audio_sink);
		} else {
		  if (audio_device != NULL) {
		    g_object_set (G_OBJECT(sink), "device", audio_device, NULL);
		  }
		  g_object_set (G_OBJECT (player_), "audio-sink", sink, NULL);
		}
	}
*/
//	swa_slmp_spdifin(msgid);

	register_mime_type("audio/*");
	Log_info("silan", "init done");

	return 0;
}

static void output_silan_set_uri(const char *uri, output_update_meta_cb_t meta_cb){
	Log_info("silan", "Set uri to '%s'", uri);
	if(silan_uri)
		free(silan_uri);
	silan_uri = (uri && *uri) ? strdup(uri) : NULL;
}

static void output_silan_set_next_uri(const char *uri) {
	Log_info("silan", "Set next uri to '%s'", uri);
	if(silan_uri_next)
		free(silan_uri_next);
	silan_uri_next = (uri && *uri) ? strdup(uri) : NULL;
}

static int output_silan_play(output_transition_cb_t callback) {
	Log_info("silan", "play");
/*
	if(!msgid || !silan_uri){
		Log_error("silan", "setting play state failed");
		return -1;
	}
*/
	play_trans_callback_ = callback;
	return output_silan_send_cmd(CMD_SlmpPlayUri, 0, silan_uri); //swa_slmp_play(msgid, silan_uri);
/*
	play_trans_callback_ = callback;
	if (get_current_player_state() != GST_STATE_PAUSED) {
		if (gst_element_set_state(player_, GST_STATE_READY) ==
		    GST_STATE_CHANGE_FAILURE) {
			Log_error("silan", "setting play state failed (1)");
			// Error, but continue; can't get worse :)
		}
		g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
	}
	if (gst_element_set_state(player_, GST_STATE_PLAYING) ==
	    GST_STATE_CHANGE_FAILURE) {
		Log_error("silan", "setting play state failed (2)");
		return -1;
	}
*/
	return 0;
}

static int output_silan_stop(void) {
	Log_info("silan", "stop");
	output_silan_send_cmd(CMD_SlmpStop, 0, NULL); //swa_slmp_stop(msgid);
	output_silan_send_cmd(CMD_SpdifIn, 1, NULL); //swa_slmp_spdifin(msgid);
	return 0;
}

static int output_silan_pause(void) {
	Log_info("silan", "pause");
	return output_silan_send_cmd(CMD_SlmpPause, 0, NULL); //swa_slmp_pause(msgid);
}

static int output_silan_seek(int position) {
	Log_info("silan", "seek position to %d seconds", position);
	return output_silan_send_cmd(CMD_SlmpSeek, position, NULL); //swa_slmp_seek(msgid, position);
}

static int output_silan_get_position(int *track_dur, int *track_pos) {
	*track_dur = last_known_duration;
	*track_pos = last_known_position;

/*
	if (get_current_player_state() != GST_STATE_PLAYING) {
		return rc;  // playbin2 only returns valid values then.
	}
*/
/*
	*track_dur = swa_slmp_gettotaltime(msgid);
	if(*track_dur < 0){
		Log_error("silan", "Failed to get track duration");
		return -1;
	}

	*track_pos = swa_slmp_gettimepos(msgid);
	if(*track_pos < 0){
		Log_error("silan", "Failed to get track position");
		return -1;
	}
*/
	Log_info("silan", "track position is %d / %d seconds", *track_pos, *track_dur);

	last_known_duration = *track_dur;
	last_known_position = *track_pos;

	return 0;
}





























#if 0
// This is crazy. I want C++ :)
struct MetaModify {
	struct SongMetaData *meta;
	int any_change;
};

static gboolean my_bus_callback(GstBus * bus, GstMessage * msg,
				gpointer data)
{
	//GMainLoop *loop = (GMainLoop *) data;
	GstMessageType msgType;
	const GstObject *msgSrc;
	const gchar *msgSrcName;

	msgType = GST_MESSAGE_TYPE(msg);
	msgSrc = GST_MESSAGE_SRC(msg);
	msgSrcName = GST_OBJECT_NAME(msgSrc);

	switch (msgType) {
	case GST_MESSAGE_EOS:
		Log_info("silan", "%s: End-of-stream", msgSrcName);
		if (gs_next_uri_ != NULL) {
			// If playbin does not support gapless (old
			// versions didn't), this will trigger.
			free(gsuri_);
			gsuri_ = gs_next_uri_;
			gs_next_uri_ = NULL;
			gst_element_set_state(player_, GST_STATE_READY);
			g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
			gst_element_set_state(player_, GST_STATE_PLAYING);
			if (play_trans_callback_) {
				play_trans_callback_(PLAY_STARTED_NEXT_STREAM);
			}
		} else if (play_trans_callback_) {
			play_trans_callback_(PLAY_STOPPED);
		}
		break;

	case GST_MESSAGE_ERROR: {
		gchar *debug;
		GError *err;

		gst_message_parse_error(msg, &err, &debug);

		Log_error("silan", "%s: Error: %s (Debug: %s)",
			  msgSrcName, err->message, debug);
		g_error_free(err);
		g_free(debug);

		break;
	}
	case GST_MESSAGE_STATE_CHANGED: {
		GstState oldstate, newstate, pending;
		gst_message_parse_state_changed(msg, &oldstate, &newstate,
						&pending);
		/*
		g_print("silan: %s: State change: '%s' -> '%s', "
			"PENDING: '%s'\n", msgSrcName,
			gststate_get_name(oldstate),
			gststate_get_name(newstate),
			gststate_get_name(pending));
		*/
		break;
	}

	case GST_MESSAGE_TAG: {
		GstTagList *tags = NULL;
    
		if (meta_update_callback_ != NULL) {
			gst_message_parse_tag(msg, &tags);
			/*g_print("silan: Got tags from element %s\n",
				GST_OBJECT_NAME (msg->src));
			*/
			struct MetaModify modify;
			modify.meta = &song_meta_;
			modify.any_change = 0;
			gst_tag_list_foreach(tags, &MetaModify_add_tag, &modify);
			gst_tag_list_free(tags);
			if (modify.any_change) {
				meta_update_callback_(&song_meta_);
			}
		}
		break;
	}

	case GST_MESSAGE_BUFFERING:
		/* not caring about these right now */
		break;
	default:
		/*
		g_print("silan: %s: unhandled message type %d (%s)\n",
		        msgSrcName, msgType, gst_message_type_get_name(msgType));
		*/
		break;
	}

	return TRUE;
}
#endif
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

static int output_silan_get_volume(int *v) {
//	*v = swa_slmp_getvolume(msgid);
	Log_info("silan", "Query volume: %d", *v);
	return 0;
}

static int output_silan_set_volume(int v) {
	Log_info("silan", "Set volume to %d", v);
	return output_silan_send_cmd(CMD_SlmpSetVolume, v, NULL); //swa_slmp_volume(msgid, v);
}

static int output_silan_get_mute(int *m) {
//	*m = swa_slmp_getmute(msgid);
	return 0;
}

static int output_silan_set_mute(int m) {
	Log_info("silan", "Set mute to %s", m ? "on" : "off");
	return output_silan_send_cmd(CMD_SlmpMute, m, NULL); //swa_slmp_mute(msgid);
}
/*
static void prepare_next_stream(GstElement *obj, gpointer userdata) {
	Log_info("silan", "about-to-finish cb: setting uri %s",
		 gs_next_uri_);
	free(gsuri_);
	gsuri_ = gs_next_uri_;
	gs_next_uri_ = NULL;
	if (gsuri_ != NULL) {
		g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
		if (play_trans_callback_) {
			// TODO(hzeller): can we figure out when we _actually_
			// start playing this ? there are probably a couple
			// of seconds between now and actual start.
			play_trans_callback_(PLAY_STARTED_NEXT_STREAM);
		}
	}
}
*/

#if 0
static int output_silan_loop(void){
	struct swa_event *event;
	struct event_handle *evt_handle;
	struct event_ipc_data *evt_data;
	button_t *button;

	evt_handle = (struct event_handle *)malloc(sizeof(struct event_handle));
	assert(evt_handle != 0);
	init_event_method(evt_handle);

	evt_handle->bt = (struct swa_button *)malloc(sizeof(struct swa_button));
	assert(evt_handle->bt != 0);
	init_bt_method(evt_handle->bt);
	//strcpy(evt_handle->bt->bt_name, "silan_keypad");
	strcpy(evt_handle->bt->bt_name, "sl_ad-keys");
	evt_handle->bt->method->get_fd(evt_handle->bt);

	evt_handle->ipc = (struct swa_ipc *)malloc(sizeof(struct swa_ipc));
	assert(evt_handle->ipc != 0);
	init_ipc_method(evt_handle->ipc);
	evt_handle->ipc->method->get_fd(evt_handle->ipc);

	event = (struct swa_event *)malloc(sizeof(struct swa_event));
	assert(event != 0);

	swa_event_init();
	create_timer();

	Log_info("silan-loop", "init done");

/*
    memset(&swa_data, 0, sizeof(SWA_DATA));
    //swa_data.volume = 8;
    swa_data.volume = swa_volume_read()/10;
    //swa_data.volume = get_dac_volume()/10;
*/
	while(1){
		evt_handle->method->wait_for_event(evt_handle, event);
		switch(event->type){
			case IPC:
				evt_data = (struct event_ipc_data *)(event->data);
				Log_info("silan-loop", "ipc cmd: %d, param: %d, data: %s", evt_data->cmd, evt_data->param, evt_data->data);
				break;

			case TIMER:
				evt_data = (struct event_ipc_data *)(event->data);
				Log_info("silan-loop", "timer %s", evt_data->data);
				break;

			case BUTTON:
				button = (button_t *)(event->data);
				Log_info("silan-loop", "button code: %d, value: %d", button->code, button->value);
				break;

			default:
				break;
		}
	}

	return 0;
}
#endif

struct output_module silan_output = {
	.shortname = "slmp",
	.description = "silan multimedia framework",
	.init        = output_silan_init,
	.set_uri     = output_silan_set_uri,
	.set_next_uri= output_silan_set_next_uri,
	.play        = output_silan_play,
	.stop        = output_silan_stop,
	.pause       = output_silan_pause,
	.seek        = output_silan_seek,
	.get_position = output_silan_get_position,
	.get_volume  = output_silan_get_volume,
	.set_volume  = output_silan_set_volume,
	.get_mute  = output_silan_get_mute,
	.set_mute  = output_silan_set_mute,
};
