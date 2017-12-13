// UDP hole punching example, client code
// Base UDP code stolen from http://www.abc.se/~m6695/udp.html
// By Oscar Rodriguez
// This code is public domain, but you're a complete lunatic
// if you plan to use this code in any real program.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "udp_send.h"
#include "udp_recv.h"
#include "STUN_client.h"

#define BUFLEN 512
#define NPACK 10
#define PORT 3030
#define PEER_LEN 3

// This is our server's IP address. In case you're wondering, this one is an RFC 5737 address.
#define SRV_IP "119.23.42.146"

//use for test only
#define LOCAL_IP "111.230.243.211"

// A small struct to hold a UDP endpoint. We'll use this to hold each peer's endpoint.
struct client
{
    int host;
    short port;
};

// Just a function to kill the program when something goes wrong.
void diep(char *s)
{
    perror(s);
    exit(1);
}

//stun
static volatile int endPoint_valid = 0;
static volatile int mappedIp = 0;
static volatile int mappedPort = 0;

//udp punch
static struct client peers[PEER_LEN]; // 10 peers. Notice that we're not doing any bound checking.

int udp_punch()
{
    struct sockaddr_in si_me, si_other;
    int s, i, f, j, k, slen = sizeof(si_other);
    struct client buf;
    struct client server;
    int n = 0;

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    // Our own endpoint data
    memset((char *)&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT); // This is not really necessary, we can also use 0 (any port)
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    // The server's endpoint data
    memset((char *)&si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    if (inet_aton(SRV_IP, &si_other.sin_addr) == 0)
        diep("aton");

    // Store the server's endpoint data so we can easily discriminate between server and peer datagrams.
    server.host = si_other.sin_addr.s_addr;
    server.port = si_other.sin_port;

#define UDP_PUNCH_RECV_BUFF (10 << 10)
    char *recvBuf = (char *)malloc(sizeof(char) * UDP_PUNCH_RECV_BUFF);
    if (recvBuf == NULL)
    {
        diep("recvBuf malloc");
    }

    // Send a simple datagram to the server to let it know of our public UDP endpoint.
    // Not only the server, but other clients will send their data through this endpoint.
    // The datagram payload is irrelevant, but if we wanted to support multiple
    // clients behind the same NAT, we'd send our won private UDP endpoint information
    // as well.
    if (sendto(s, "hi", 2, 0, (struct sockaddr *)(&si_other), slen) == -1)
        diep("sendto");

    // Right here, our NAT should have a session entry between our host and the server.
    // We can only hope our NAT maps the same public endpoint (both host and port) when we
    // send datagrams to other clients using our same private endpoint.
    while (1)
    {
        // Receive data from the socket. Notice that we use the same socket for server and
        // peer communications. We discriminate by using the remote host endpoint data, but
        // remember that IP addresses are easily spoofed (actually, that's what the NAT is
        // doing), so remember to do some kind of validation in here.
        memset(recvBuf, 0, UDP_PUNCH_RECV_BUFF);
        int ret = 0;
        if ((ret = recvfrom(s, recvBuf, UDP_PUNCH_RECV_BUFF, 0, (struct sockaddr *)(&si_other), &slen)) == -1)
            diep("recvfrom");
        printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        if (server.host == si_other.sin_addr.s_addr && server.port == (short)(si_other.sin_port))
        {
            // The datagram came from the server. The server code is set to send us a
            // datagram for each peer, in which the payload contains the peer's UDP
            // endpoint data. We're receiving binary data here, sent using the server's
            // byte ordering. We should make sure we agree on the endianness in any
            // serious code.
            f = 0;
            // Now we just have to add the reported peer into our peer list
            memset((char *)&buf, 0, sizeof(struct client));
            memcpy((char *)&buf, recvBuf, sizeof(struct client));
            for (i = 0; i < n && f == 0; i++)
            {
                if (peers[i].host == buf.host && peers[i].port == buf.port)
                {
                    f = 1;
                }
            }
            // Only add it if we didn't have it before.
            if (n + 1 < PEER_LEN)
            {
                if (f == 0)
                {
                    if (mappedIp == buf.host)
                    {
                        //my own ip from server
                        struct in_addr inp;
                        inp.s_addr = buf.host;
                        printf("udp get own ip %s:%d\n", inet_ntoa(inp), ntohs(buf.port));
                        continue;
                    }
                    peers[n].host = buf.host;
                    peers[n].port = buf.port;
                    n++;

                    si_other.sin_addr.s_addr = buf.host;
                    si_other.sin_port = buf.port;
                    printf("Added peer %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
                    printf("Now we have %d peers\n", n);
                }
            }
            else
            {
                printf("peer_len max %d !!!\n", n);
                continue; //ignore the new peer
            }
            // And here is where the actual hole punching happens. We are going to send
            // a bunch of datagrams to each peer. Since we're using the same socket we
            // previously used to send data to the server, our local endpoint is the same
            // as before.
            // If the NAT maps our local endpoint to the same public endpoint
            // regardless of the remote endpoint, after the first datagram we send, we
            // have an open session (the hole punch) between our local endpoint and the
            // peer's public endpoint. The first datagram will probably not go through
            // the peer's NAT, but since UDP is stateless, there is no way for our NAT
            // to know that the datagram we sent got dropped by the peer's NAT (well,
            // our NAT may get an ICMP Destination Unreachable, but most NATs are
            // configured to simply discard them) but when the peer sends us a datagram,
            // it will pass through the hole punch into our local endpoint.
            for (k = 0; k < 10; k++)
            {
                // Send 10 datagrams.
                for (i = 0; i < n; i++)
                {
                    si_other.sin_addr.s_addr = peers[i].host;
                    si_other.sin_port = peers[i].port;
                    // Once again, the payload is irrelevant. Feel free to send your VoIP
                    // data in here.
                    printf("#%2d send data to %s:%d\n", k, inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
                    if (k == 9)
                    {
                        if (sendto(s, "ok", 2, 0, (struct sockaddr *)(&si_other), slen) == -1)
                            diep("sendto()");
                    }
                    else
                    {
                        if (sendto(s, "hi", 2, 0, (struct sockaddr *)(&si_other), slen) == -1)
                            diep("sendto()");
                    }
                }
            }
        }
        else
        {
            // The datagram came from a peer
            for (i = 0; i < n; i++)
            {
                // Identify which peer it came from
                if (peers[i].host == buf.host && peers[i].port == (short)(buf.port))
                {
                    // And do something useful with the received payload
                    printf("Received from peer %d!\n", i);
                    break;
                }
            }

            if (recvBuf[0] == 'o' && recvBuf[1] == 'k')
            {
                printf("get ok responce !!!\n");
                endPoint_valid = 1;
                break; //退出循环准备收发数据
            }

            // It is possible to get data from an unregistered peer. These are some reasons
            // I quickly came up with, in which this can happen:
            // 1. The server's datagram notifying us with the peer's address got lost,
            //    or it hasn't arrived yet (likely)
            // 2. A malicious (or clueless) user is sending you data on this endpoint (maybe
            //    unlikely)
            // 3. The peer's public endpoint changed either because the session timed out,
            //    or because its NAT did not assign the same public endpoint when sending
            //    datagrams to different remote endpoints. If this happens, and we're able
            //    to detect this situation, we could change our peer's endpoint data to
            //    the correct one. If we manage to pull this off correctly, even if at most
            //    one client has a NAT that doesn't support hole punching, we can communicate
            //    directly between both peers.
        }
    }

    // Actually, we never reach this point...
    close(s);
    return 0;
}

//使用 stun 获得本机 NAT ip
int getMyMappedAddr(int *mappedIp, int *mappedPort)
{
    struct STUNServer servers[14] = {
        {"stun1.l.google.com", 19302},
        {"stun1.l.google.com", 19305},
        {"stun2.l.google.com", 19302},
        {"stun2.l.google.com", 19305},
        {"stun3.l.google.com", 19302},
        {"stun3.l.google.com", 19305},
        {"stun4.l.google.com", 19302},
        {"stun4.l.google.com", 19305},
        {"stun.l.google.com", 19302},
        {"stun.l.google.com", 19305},
        {"stun.wtfismyip.com", 3478},
        {"stun.bcs2005.net", 3478},
        {"numb.viagenie.ca", 3478},
        {"173.194.202.127", 19302}};

    int Success = 0;
    char *address = malloc(sizeof(char) * 100);
    if (address == NULL)
    {
        printf("address malloc err\n");
        return -1;
    }
    int port = 0;
    for (int index = 0; index < 14 && Success == 0; index++)
    {
        bzero(address, 100);
        port = 0;

        struct STUNServer server = servers[index];

        int retVal = getPublicIPAddress(server, address, &port);

        if (retVal != 0)
        {
            printf("%s: Failed. Error: %d\n", server.address, retVal);
        }
        else
        {
            printf("%s: Public IP: %s : %d\n", server.address, address, port);
            Success = 1;
        }
    }
    if (Success == 1)
    {
        struct in_addr inp;
        inet_aton(address, &inp);
        //TODO: 获得 ip 端口后，进行打洞数据传输, 两个客户端配合公网服务器获得对方ip(之后用 sip 服务器 取代该方式)
        *mappedIp = inp.s_addr;
        *mappedPort = port;
        free(address);
        return 0;
    }
    free(address);
    return -1;
}

FILE *fp = NULL;
char fpname[128] = "testSend.data";
FILE *fpFormat = NULL;
char fpFormaName[128] = "testSendForamt.data";
/**
 * 发送数据到映射的端口
 */
static int udpOrtp_send()
{
    if (NULL == (fp = fopen(fpname, "rb")))
    {
        printf("fopen %s err\n", fpname);
        return -1;
    }
    if (NULL == (fpFormat = fopen(fpFormaName, "rb")))
    {
        printf("fopen %s err\n", fpFormaName);
        return -1;
    }
    int len = 0;
    int bufsize = (10 << 10);
    char *buf = (char *)malloc(bufsize);
    if (NULL == buf)
    {
        printf("err in malloc\n");
        return -1;
    }
    while (1)
    {
        if (0 == fscanf(fpFormat, "%d", &len))
        {
            printf("read end or err\n");
            break;
        }
        memset(buf, 0, bufsize);
        if (len > bufsize || len != fread(buf, 1, len, fp)) //读取编码的声音
        {
            printf("read end or err\n");
            break;
        }
        if (0 > udpSend_send(buf, len))
        {
            printf("rtp_sendAudio err\n");
            fclose(fp);
            fclose(fpFormat);
            free(buf);
            return -1;
        }
    }
    fclose(fp);
    fclose(fpFormat);
    fp = NULL;
    fpFormat = NULL;
    if (buf != NULL)
    {
        free(buf);
        buf = NULL;
    }
    return 0;
}

FILE *fp_recv = NULL;
char fpname_recv[128] = "testRecv.data";
FILE *fpFormat_recv = NULL;
char fpFormaName_recv[128] = "testRecvForamt.data";
/**
 * 发送数据到映射的端口
 */
static int udpOrtp_recv()
{
    if (NULL == (fp_recv = fopen(fpname_recv, "wb")))
    {
        printf("fopen %s err\n", fpname_recv);
        return -1;
    }
    if (NULL == (fpFormat_recv = fopen(fpFormaName_recv, "wb")))
    {
        printf("fopen %s err\n", fpFormaName_recv);
        return -1;
    }
    int len = 0;
    int bufsize = (10 << 10);
    unsigned char *buf = (unsigned char *)malloc(bufsize);
    if (NULL == buf)
    {
        printf("err in malloc\n");
        return -1;
    }
    while (1)
    {
        if (udpRecv_recv(buf, &len) < 0)
        {
            printf("rtp_sendAudio err\n");
            fclose(fp);
            fclose(fpFormat);
            free(buf);
            return -1;
        }
        printf("recv data len %d\n", len);
        //TODO: write to file
    }
    fclose(fp_recv);
    fclose(fpFormat_recv);
    fp_recv = NULL;
    fpFormat_recv = NULL;
    if (buf != NULL)
    {
        free(buf);
        buf = NULL;
    }
    return 0;
}

//test udp punch and rtp send
int main(int argc, char **argv)
{
    //step 1
    //使用stun 获得当前网络 NAT 映射公网地址
    printf("step 1. 使用stun 获得当前网络 NAT 映射公网地址\n");
    if (0 != getMyMappedAddr((int *)&mappedIp, (int *)&mappedPort))
    {
        printf("getMyMappedAddr failed\n");
        return -1;
    }

    //step 2
    //使用 udp + 外网 主机 获得要通信的远端主机 映射ip
    printf("step 2. 使用 udp + 外网 主机 获得要通信的远端主机 映射ip\n");
    udp_punch();

    //step 3
    //send &recv data over rtp
    if (argc == 2 && strcmp(argv[1], "recv") == 0)
    {
        //recv
        printf("step 3. recv data over rtp\n");
        udpRecv_settingInit("0.0.0.0", 3030);
        if (udpRecv_init() < 0)
        {
            printf("udpRecv_init err\n");
            return -1;
        }
        if (0 > udpOrtp_recv())
        {
            printf("udpOrtp_send err\n");
            return -1;
        }
        udpRecv_exit();
    }
    else
    {
        //send
        printf("step 3. send data over rtp\n");
        struct in_addr inp;
        inp.s_addr = peers[0].host;
        udpSend_settingInit(inet_ntoa(inp), ntohs(peers[0].host));
        if (udpSend_init() < 0)
        {
            printf("udpSend_init err\n");
            return -1;
        }
        udpOrtp_send();
        udpSend_exit();
    }
    return 0;
}