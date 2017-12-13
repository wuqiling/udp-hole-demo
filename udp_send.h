#ifndef _UDP_SEND_H
#define _UDP_SEND_H 

/**
 * setting init
 * input mapped ip and port
 */
int udpSend_settingInit(char *dstIp, int dstPort);

/**
 * init send
 */
int udpSend_init();

/**
 * exit send
 */
int udpSend_exit();

/**
 * send process
 */
int udpSend_send(void *sendBuf, int sendLen);

#endif // !_UDP_SEND_H