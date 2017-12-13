//
//  STUNExternalIP.c
//  STUNExternalIP
//
//  Created by FireWolf on 2016-11-16.
//  Revised by FireWolf on 2017-02-24.
//
//  Copyright © 2016-2017 FireWolf. All rights reserved.
//

#include "STUN_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LOCAL_PORT 3030
int local_port = LOCAL_PORT;

// MARK: === PRIVATE DATA STRUCTURE ===

// RFC 5389 Section 6 STUN Message Structure
struct STUNMessageHeader
{
    // Message Type (Binding Request / Response)
    unsigned short type;

    // Payload length of this message
    unsigned short length;

    // Magic Cookie
    unsigned int cookie;

    // Unique Transaction ID
    unsigned int identifier[3];
};

#define XOR_MAPPED_ADDRESS_TYPE 0x0020

// RFC 5389 Section 15 STUN Attributes
struct STUNAttributeHeader
{
    // Attibute Type
    unsigned short type;

    // Payload length of this attribute
    unsigned short length;
};

#define IPv4_ADDRESS_FAMILY 0x01;
#define IPv6_ADDRESS_FAMILY 0x02;

// RFC 5389 Section 15.2 XOR-MAPPED-ADDRESS
struct STUNXORMappedIPv4Address
{
    unsigned char reserved;

    unsigned char family;

    unsigned short port;

    unsigned int address;
};

///
/// Get the external IPv4 address
///
/// @param server A STUN server
/// @param address A non-null buffer to store the public IPv4 address
/// @param *port non-null buffer to store the public IPv4 port
/// @return 0 on success.
/// @warning This function returns
///          -1 if failed to bind the socket; 
///          -2 if failed to resolve the given STUN server;
///          -3 if failed to send the STUN request;
///          -4 if failed to read from the socket (and timed out; default = 5s);
///          -5 if failed to get the external address.
///          -6 if failed to malloc
///
int getPublicIPAddress(struct STUNServer server, char *address, int *port)
{
    // Create a UDP socket
    int socketd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // Local Address
    struct sockaddr_in *localAddress = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (localAddress == NULL)
    {
        printf("localAddress malloc err\n");
        return -6;
    }

    bzero(localAddress, sizeof(struct sockaddr_in));

    localAddress->sin_family = AF_INET;

    localAddress->sin_addr.s_addr = INADDR_ANY;

    localAddress->sin_port = htons(local_port);

    if (bind(socketd, (struct sockaddr *)localAddress, sizeof(struct sockaddr_in)) < 0)
    {
        free(localAddress);

        close(socketd);

        return -1;
    }

    // Remote Address
    // First resolve the STUN server address
    struct addrinfo *results = NULL;

    struct addrinfo *hints = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    if (hints == NULL)
    {
        printf("hints malloc err\n");
        return -6;
    }

    bzero(hints, sizeof(struct addrinfo));

    hints->ai_family = AF_INET;

    hints->ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server.address, NULL, hints, &results) != 0)
    {
        free(localAddress);

        free(hints);

        close(socketd);

        return -2;
    }

    struct in_addr stunaddr;

    // `results` is a linked list
    // Read the first node
    if (results != NULL)
    {
        stunaddr = ((struct sockaddr_in *)results->ai_addr)->sin_addr;
    }
    else
    {
        free(localAddress);

        free(hints);

        freeaddrinfo(results);

        close(socketd);

        return -2;
    }

    // Create the remote address
    struct sockaddr_in *remoteAddress = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (remoteAddress == NULL)
    {
        printf("remoteAddress malloc err\n");
        return -6;
    }

    bzero(remoteAddress, sizeof(struct sockaddr_in));

    remoteAddress->sin_family = AF_INET;

    remoteAddress->sin_addr = stunaddr;

    remoteAddress->sin_port = htons(server.port);

    // Construct a STUN request
    struct STUNMessageHeader *request = (struct STUNMessageHeader *)malloc(sizeof(struct STUNMessageHeader));
    if (request == NULL)
    {
        printf("request malloc err\n");
        return -6;
    }

    request->type = htons(0x0001);

    request->length = htons(0x0000);

    request->cookie = htonl(0x2112A442);

    for (int index = 0; index < 3; index++)
    {
        srand((unsigned int)time(0));

        request->identifier[index] = rand();
    }

    // Send the request
    if (sendto(socketd, request, sizeof(struct STUNMessageHeader), 0, (struct sockaddr *)remoteAddress, sizeof(struct sockaddr_in)) == -1)
    {
        free(localAddress);

        free(hints);

        freeaddrinfo(results);

        free(remoteAddress);

        free(request);

        close(socketd);

        return -3;
    }

    // Set the timeout
    struct timeval tv = {5, 0};

    setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    // Read the response
    char *buffer = (char *)malloc(sizeof(char) * 512);
    if (buffer==NULL)
    {
        printf("buffer malloc err\n");
        return -6;
    }

    bzero(buffer, 512);

    long length = read(socketd, buffer, 512);

    if (length < 0)
    {
        free(localAddress);

        free(hints);

        freeaddrinfo(results);

        free(remoteAddress);

        free(request);

        free(buffer);

        close(socketd);

        return -4;
    }

    char *pointer = buffer;

    struct STUNMessageHeader *response = (struct STUNMessageHeader *)buffer;

    if (response->type == htons(0x0101))
    {
        // Check the identifer
        for (int index = 0; index < 3; index++)
        {
            if (request->identifier[index] != response->identifier[index])
            {
                return -1;
            }
        }

        pointer += sizeof(struct STUNMessageHeader);

        while (pointer < buffer + length)
        {
            struct STUNAttributeHeader *header = (struct STUNAttributeHeader *)pointer;

            if (header->type == htons(XOR_MAPPED_ADDRESS_TYPE))
            {
                pointer += sizeof(struct STUNAttributeHeader);

                struct STUNXORMappedIPv4Address *xorAddress = (struct STUNXORMappedIPv4Address *)pointer;

                unsigned int numAddress = htonl(xorAddress->address) ^ 0x2112A442;

                // Parse the IP address
                snprintf(address, 20, "%d.%d.%d.%d",
                         (numAddress >> 24) & 0xFF,
                         (numAddress >> 16) & 0xFF,
                         (numAddress >> 8) & 0xFF,
                         numAddress & 0xFF);

                //Parse the IP port
                *port = ntohs(xorAddress->port) ^ 0x2112;

                free(localAddress);

                free(hints);

                freeaddrinfo(results);

                free(remoteAddress);

                free(request);

                free(buffer);

                close(socketd);

                return 0;
            }

            pointer += (sizeof(struct STUNAttributeHeader) + ntohs(header->length));
        }
    }

    free(localAddress);

    free(hints);

    freeaddrinfo(results);

    free(remoteAddress);

    free(request);

    free(buffer);

    close(socketd);

    return -5;
}

#if 0
//测试 udp 打洞
//要求nat 类型为 Full Cone NAT 或者 Restricted Cone
//
int main(int argc, const char *argv[])
{
    struct STUNServer servers[14] = {
        // {"stun.ekiga.net"    , 3478 }, {"stun.xten.com"     , 3478},
        {"stun.l.google.com", 19302},
        {"stun.l.google.com", 19305},
        {"stun1.l.google.com", 19302},
        {"stun1.l.google.com", 19305},
        {"stun2.l.google.com", 19302},
        {"stun2.l.google.com", 19305},
        {"stun3.l.google.com", 19302},
        {"stun3.l.google.com", 19305},
        {"stun4.l.google.com", 19302},
        {"stun4.l.google.com", 19305},
        {"stun.wtfismyip.com", 3478},
        {"stun.bcs2005.net", 3478},
        {"numb.viagenie.ca", 3478},
        {"173.194.202.127", 19302}};

    int Success = 0;
    char *address = malloc(sizeof(char) * 100);
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


    free(address);
    return 0;
}

#endif
