//ortp lib
#include <ortp/ortp.h>

//basic
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

//
#include "udp_send.h"

#define RTP_TS_INC 3600 // 90000 / framerate = 90000 / 25

typedef struct
{
    RtpSession *audioSession; //ortp 音频会话
    char rtpRemoteIp[20];     //目的Ip
    int rtpAudioPort;         //目的音频端口
    int rtpAudioLocalPort;    //local audio port
    int rtpAudioType;         //音频编码格式
    volatile unsigned long audio_ts;   //时间戳
    int maxPacketSize;
} t_udp_send;

static t_udp_send sender;

/**
 * setting init
 * input mapped ip and port
 */
int udpSend_settingInit(char *dstIp, int dstPort, int localPort)
{
    memset(&sender, 0, sizeof(t_udp_send));
    sender.audioSession = NULL;
    sender.audio_ts = 0;
    sender.rtpAudioType = 34;
    sender.maxPacketSize = 1100;

    strcpy(sender.rtpRemoteIp, dstIp);
    sender.rtpAudioPort = dstPort;
    sender.rtpAudioLocalPort = localPort;
    return 0;
}

/**
 * init send
 */
int udpSend_init()
{
    ortp_init();
    ortp_scheduler_init();
    ortp_set_log_level_mask(ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR);

    sender.audioSession = rtp_session_new(RTP_SESSION_SENDONLY);
    if (sender.audioSession == NULL)
    {
        printf("Ortp Audio init error!\n");
        return -1;
    }
    rtp_session_set_scheduling_mode(sender.audioSession, 1);
    rtp_session_set_blocking_mode(sender.audioSession, 1);
    // rtp_session_set_connected_mode(*ppsession,TRUE);
    rtp_session_set_local_addr(sender.audioSession, "0.0.0.0", sender.rtpAudioLocalPort, -1);
    rtp_session_set_remote_addr(sender.audioSession, sender.rtpRemoteIp, sender.rtpAudioPort);
    rtp_session_set_payload_type(sender.audioSession, sender.rtpAudioType); //s->rtpAudioType

    char *ssrc = getenv("SSRC");
    if (ssrc != NULL)
    {
        printf("Audio using SSRC=%i.\n", atoi(ssrc));
        rtp_session_set_ssrc(sender.audioSession, atoi(ssrc));
    }
    return 0;
}

/**
 * exit send
 */
int udpSend_exit()
{
    if (sender.audioSession != NULL)
    {
        rtp_session_destroy(sender.audioSession);
        sender.audioSession = NULL;
    }

    ortp_exit();
    ortp_global_stats_display();
    return 0;
}

/**
 * send process
 */
int udpSend_send(void *sendBuf, int sendLen)
{
    unsigned char *pbuf = sendBuf; //buf to send
    if (sendLen <= sender.maxPacketSize)
    {
        rtp_session_send_with_ts(sender.audioSession, pbuf, sendLen, sender.audio_ts);
        return 0;
    }
    int pos = 0;
    while (sendLen - pos > 0)
    {
        if (sendLen - pos <= sender.maxPacketSize)
        {
            rtp_session_send_with_ts(sender.audioSession, pbuf + pos, sendLen - pos, sender.audio_ts);
            return 0;
        }
        rtp_session_send_with_ts(sender.audioSession, pbuf + pos, sender.maxPacketSize, sender.audio_ts);
        pos += sender.maxPacketSize;
        usleep(20);
    }
    sender.audio_ts += RTP_TS_INC;
    return 0;
}
