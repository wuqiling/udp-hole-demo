#ifndef _UDP_SEND_H
#define _UDP_SEND_H

/**
 * setting init
 * input mapped ip and port
 */
int udpSend_settingInit(char *dstIp, int dstPort, int localPort);

/**
 * init send
 */
int udpSend_init();

/**
 * exit send
 */
int udpSend_exit(void);

/**
 * increase ts
 * use ts to indicate new frame data
 */
int udpSend_updateTS(void);

/**
 * send process
 */
int udpSend_send(void *sendBuf, int sendLen);

#endif // !_UDP_SEND_H