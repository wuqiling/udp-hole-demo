//
//  STUNExternalIP.h
//  STUNExternalIP
//
//  Created by FireWolf on 2016-11-16.
//  Revised by FireWolf on 2017-02-24.
//
//  Copyright © 2017 FireWolf. All rights reserved.
//
#ifndef _STUNEXTERNALIP_H
#define _STUNEXTERNALIP_H

struct STUNServer
{
    char *address;

    unsigned short port;
};

///
/// Get the external IPv4 address
///
/// @param server A valid STUN server
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
int getPublicIPAddress(struct STUNServer server, char *address, int *port);



#endif // !_STUN_CLIENT_H