#ifndef _UDP_RECV_H
#define _UDP_RECV_H

/**
 * setting init
 * input mapped ip and port
 */
int udpRecv_settingInit(char *recvIp, int recvPort);

/**
 * init recv
 */
int udpRecv_init();

/**
 * 退出ortp
 * 根据 ortp 0.23.0 src/test/rtprecv.c 编写
 */
int udpRecv_exit(void);

/**
 * recv process
 */
int udpRecv_recv(void *recvBuf, int *recvLen);

#endif // !_UDP_RECV_H