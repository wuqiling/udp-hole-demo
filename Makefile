CC=gcc

obj=clientDemo.o serverDemo.o udp_send.o STUNExternalIP.o udp_recv.o

all:$(obj)

clientDemo.o:udp_client.c udp_send.o STUNExternalIP.o udp_recv.o
	$(CC) -o clientDemo.o udp_send.o STUNExternalIP.o udp_recv.o udp_client.c -lortp

serverDemo.o:udp_server.c
	$(CC) -o serverDemo.o udp_server.c

udp_send.o:udp_send.c udp_send.h
	$(CC) -o udp_send.o -c udp_send.c

udp_recv.o:udp_recv.c udp_recv.h
	$(CC) -o udp_recv.o -c udp_recv.c

STUNExternalIP.o:STUNExternalIP.c STUNExternalIP.h
	$(CC) -o STUNExternalIP.o -c STUNExternalIP.c

clean:
	rm -rf *.o