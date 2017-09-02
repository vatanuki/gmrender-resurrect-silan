#ifndef _SWA_DBUS_API_H_
#define _SWA_DBUS_API_H_

#define BUF_LEN 1024
#define IPC_NUM 8
#define SWA_MSG_DAEMON         1111

#define SWA_SIGNAL_SLMP        1
#define SWA_SIGNAL_UI          2
#define SWA_SIGNAL_DMR         3
#define SWA_SIGNAL_DMS         4
#define SWA_SIGNAL_WIFI        5
#define SWA_SIGNAL_VOLD        6
#define SWA_SIGNAL_AIRPLAY     7
#define SWA_SIGNAL_APK         8
#define SWA_SIGNAL_BTN         9

#define SWA_SLMP_SIGNAL_SINK       1
#define SWA_SLMP_SIGNAL_SOURCE     1
#define SWA_UI_SIGNAL_SINK         2
#define SWA_UI_SIGNAL_SOURCE       2
#define SWA_DMR_SIGNAL_SINK        3
#define SWA_DMR_SIGNAL_SOURCE      3
#define SWA_DMS_SIGNAL_SINK        4
#define SWA_DMS_SIGNAL_SOURCE      4
#define SWA_WIFI_SIGNAL_SINK       5
#define SWA_WIFI_SIGNAL_SOURCE     5
#define SWA_VOLD_SIGNAL_SINK       6
#define SWA_VOLD_SIGNAL_SOURCE     6
#define SWA_AIRPLAY_SIGNAL_SINK    7
#define SWA_AIRPLAY_SIGNAL_SOURCE  7
#define SWA_APK_SIGNAL_SINK        8
#define SWA_APK_SIGNAL_SOURCE      8
#define SWA_BTN_SIGNAL_SINK        9
#define SWA_BTN_SIGNAL_SOURCE      9
#define SWA_DMR1_SIGNAL_SINK       10
#define SWA_DMR1_SIGNAL_SOURCE     10
#define SWA_SDK_SIGNAL_SINK        11
#define SWA_SDK_SIGNAL_SOURCE      11


//slmp and dmr play state
#define SLMP_STATE_STOP        0
#define SLMP_STATE_PAUSE       1
#define SLMP_STATE_PLAY        2
#define DMR_STATE_STOP         3
#define DMR_STATE_PAUSE        4
#define DMR_STATE_PLAY         5
#define SLMP_STATE_FINISH      6

#define SLMP_STATE_NEXT        8
#define SLMP_STATE_PREV        9

#define SLMP_STATE_NEXT        8
#define SLMP_STATE_PREV        9

//slmp error state
#define SLMP_ERROR_NOERROR     0
#define SLMP_ERROR_FILE        1
#define SLMP_ERROR_AUDIO       2

//bypass mode
#define LINEIN_BYPASS          0
#define MICIN_BYPASS           1

//about dmc,dmr,dms
#define CMD_GetDmrList         1
#define CMD_GetDmsList         2
#define CMD_SetDmr             3
#define CMD_SetDms             4
#define CMD_GetDmsUri          5
#define CMD_SetDmsUri          6
#define CMD_DmrSetUri          7
#define CMD_DmrMute            8
#define CMD_DmrSetVolume       9
#define CMD_DmrGetVolume       10
#define CMD_DmrPause           11
#define CMD_DmrGetTimePos      12
#define CMD_DmrSetTimePos      13
#define CMD_DmrSeek            14
#define CMD_DmrGetTotalTime    15
#define CMD_DmrStop            16
#define CMD_DmrPlay            17
#define CMD_DmrDcTitle         18
#define CMD_DmsGetWifiMode     19
#define CMD_DmrNextSong        20
#define CMD_DmrReload          21
#define CMD_DmrGetUuid         22

// APK UI CMD
#define CMD_ApkConfig          24
#define CMD_ApkAudioSet        25
#define CMD_ApkAudioGet        26

//about slmp player
#define CMD_SlmpPcmInfo         28
#define CMD_SlmpSetChVol        29
#define CMD_SlmpPlayUri         30
#define CMD_SlmpMute            31
#define CMD_SlmpSetVolume       32
#define CMD_SlmpGetVolume       33
#define CMD_SlmpPause           34
#define CMD_SlmpStop            35
#define CMD_SlmpGetTimePos      36
#define CMD_SlmpSetTimePos      37
#define CMD_SlmpSeek            38
#define CMD_SlmpSetTotalTime    39
#define CMD_SlmpGetTotalTime    40
#define CMD_SlmpPlayState       41
#define CMD_SlmpOpenAoDevice    42
#define CMD_SlmpCloseAoDevice   43
#define CMD_SlmpErrorState      44
#define CMD_SlmpPostProc        45
#define CMD_SlmpGetMute         46
#define CMD_SlmpSeekable        47
#define CMD_SlmpGetCurUrl       48

//about hotplug
#define CMD_DiskInserted        50
#define CMD_DiskRemoved         51

//about wifi
#define CMD_WifiChangeMode        55
#define CMD_WifiOn                56
#define CMD_WifiOff               57
#define CMD_WifiAddNetwork        58
#define CMD_WifiRemoveNetwork     59
#define CMD_WifiSelectNetwork     60
#define CMD_WifiScanOk            61
#define CMD_WifiGetStatus         62
#define CMD_WifiStartWps          63
#define CMD_WifiSignalChange      64
#define CMD_WifiConnectOk         65
#define CMD_WifiGetIPOk           66
#define CMD_WifiInvalidKey        67
#define CMD_WifiScan			  68
#define CMD_WifiSetMode			  69
#define CMD_WifiSetApParam        70
#define CMD_WifiSimpleConfig      71

//about airplay
#define CMD_AirplayStop           80
#define CMD_AirplaySetVolume      81
#define CMD_AirplayStatus         82
#define CMD_AirplayOpenDevice     83
#define CMD_AirplayCloseDevice    84
#define CMD_AirplayUpdateInfo     85 /* update track info */
#define CMD_AirplayUpdatePos      86 /* update track position*/  
#define CMD_AirplayPostProc       87
#define CMD_AirplayGetVolume      88
#define CMD_AirplayGetStatus      89
#define CMD_AirplayGetTotalTime   90
#define CMD_AirplayGetTrackinfo   91
#define CMD_AirplayNextitem		  92

//about db
#define CMD_DbSearchOK            95
#define CMD_DbSearchFail		  96

#define CMD_BUTTON                98
#define CMD_Timer                 99

#define CMD_InbufDataSize         101
#define CMD_OutbufDataSize        102
#define CMD_SpdInBegM             103

#define CMD_AudioInMode           199
#define CMD_AudioOutMode          200
#define CMD_LineIn                201
#define CMD_LineOut               202
#define CMD_SpdifIn               203
#define CMD_SpdifOut              204
#define CMD_LineInMix             205
#define CMD_SpdifInMix            206
#define CMD_Recording             207
#define CMD_StopRecord            208
#define CMD_SetPadMux             209
#define CMD_RecordStatus          210
#define CMD_BluetoothIn           211

#define CMD_SetReqChannels        229
#define CMD_SetMic                230
#define CMD_VcMode                231
#define CMD_SetEqFlt              232
#define CMD_LoadEq                233
#define CMD_UnloadEq              234
#define CMD_SetScale              235
#define CMD_SetBypassMicinVol     236
#define CMD_SetBypassLineinVol    237
#define CMD_Bypass                238

struct swa_ipc_data{
	int src;
	int des;
    char cmd;
    int param;
    char data[BUF_LEN];
};

typedef int (*signal_callback)(struct swa_ipc_data *ipc_data, void *arg);

int swa_ipc_msg_recv(int native_name, int dest_name, signal_callback scallback, void *arg);

int swa_ipc_msg_send(int msgid, int native_name, int dest_name, struct swa_ipc_data *ipc_data);

int swa_ipc_msg_send_then_read(int msgid, int native_name, int dest_name, struct swa_ipc_data *ipc_data);

int swa_ipc_msg_send_ack(int msgid, int native_name, int dest_name, struct swa_ipc_data *ipc_data);

int swa_ipc_msg_broadcast(int msgid, int native_name, struct swa_ipc_data *ipc_data);

int swa_msg_init(int msgid);

#endif
