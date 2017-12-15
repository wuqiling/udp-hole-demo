//ortp lib
#include <ortp/ortp.h>

//basic
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

//
#include "udp_recv.h"

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
} t_udp_recv;

static t_udp_recv recver;

/**
 * setting init
 * input mapped ip and port
 */
int udpRecv_settingInit(char *recvIp, int recvPort)
{
    memset(&recver, 0, sizeof(t_udp_recv));
    recver.audioSession = NULL;
    recver.audio_ts = 0;
    recver.rtpAudioType = 34;
    recver.maxPacketSize = (10 << 10);

    strcpy(recver.rtpRemoteIp, recvIp);
    recver.rtpAudioPort = recvPort;
    recver.rtpAudioLocalPort = 3030;
    return 0;
}

/**
 * init recv
 */
int udpRecv_init()
{
    ortp_init();
    ortp_scheduler_init();
    ortp_set_log_level_mask(ORTP_DEBUG | ORTP_MESSAGE | ORTP_WARNING |
                            ORTP_ERROR);

    recver.audioSession = rtp_session_new(RTP_SESSION_RECVONLY);
    if (recver.audioSession == NULL)
    {
        printf("audioSession rtp_session_new err\n");
        return -1;
    }

    rtp_session_set_scheduling_mode(recver.audioSession, 1);
    rtp_session_set_blocking_mode(recver.audioSession, 1);
    rtp_session_set_local_addr(recver.audioSession, "0.0.0.0", recver.rtpAudioPort, -1);
    rtp_session_set_connected_mode(recver.audioSession, TRUE);
    rtp_session_set_symmetric_rtp(recver.audioSession, TRUE);
    rtp_session_enable_adaptive_jitter_compensation(recver.audioSession, TRUE);
    rtp_session_set_jitter_compensation(recver.audioSession, 40);
    rtp_session_set_payload_type(recver.audioSession, recver.rtpAudioType);
    return 0;
}

// 退出ortp
//根据 ortp 0.23.0 src/test/rtprecv.c 编写
int udpRecv_exit(void)
{
    //退出
    if (recver.audioSession != NULL)
        rtp_session_destroy(recver.audioSession);
    recver.audioSession = NULL;
    ortp_exit();
    ortp_global_stats_display();
    return 0;
}

/**
 * recv process
 */
int udpRecv_recv(void *recvBuf, int *recvLen)
{
    // data buf
    int maxBufSize = recver.maxPacketSize;
    unsigned char *pbuf = recvBuf;

    int recvSize = 0; //接收到的rtp 的数据大小
    int have_more = 1, stream_received = 0, waitCnt = 0;
    while (recvSize == 0)
    {
        have_more = 1;
        while (have_more)
        {
            memset(pbuf, 0, maxBufSize);
            recvSize = rtp_session_recv_with_ts(recver.audioSession, pbuf,
                                                maxBufSize, recver.audio_ts,
                                                &have_more);
            if (recvSize > 0)
                stream_received = 1;

            if (have_more > 0)
            {
                //TODO: 内存不足时修改此方案
                //缓冲区较大
                printf("recv pkt too large, size %d\n", recvSize);
                break;
            }
            /* this is to avoid to write to disk some silence before the first RTP
            * packet is returned*/
            if ((stream_received) && (recvSize > 0))
            {
                *recvLen = recvSize;
            } //if
        }     //while (have_more)
        if (recvSize == 0)
        {
            usleep(2);
            if ((++waitCnt) % 100 == 0)
            {
                printf("udpRecv_recv audio waiting ...\n");
            }
            if (waitCnt > 1000)
            {
                printf("udpRecv_recv audio waiting too long, exit\n");
                return -1;
            }
        }
        else
        {
            waitCnt = 0;
        }
        recver.audio_ts += RTP_TS_INC; // TODO: 确定时间增量
    }                                  //while (recvSize <= 0)
    *recvLen = recvSize;
    return 0;
}
