A simple UDP hole punching example originally taken from
http://www.rapapaing.com/blog/?p=24.
To use it you need to set SRV_IP in client_udp.c.

A drawback is that it always punches the one port
that the server is using to talk to the other client.
It would be better to test the ports above that port
for a higher success rate.


v0.0.1

#1 add function of stun

#2 add function of ortp-send

#3 add function of ortp-recv

test pass on ubuntu 16.04 x64


dependent:
ortp-0.23.0

build(only in linux now)

    make clean && make

usage:

server(must run in public ip)

    ./serverDemo.o


client(must run in diffrent nat now, this will be fixed)

recv data

    ./clientDemo.o recv
    
send data

    ./clientDemo.o

