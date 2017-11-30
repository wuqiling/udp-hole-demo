CC=gcc

obj=clientDemo.o serverDemo.o

all:$(obj)

clientDemo.o:udp_client.c
	$(CC) -o clientDemo.o udp_client.c

serverDemo.o:udp_server.c
	$(CC) -o serverDemo.o udp_server.c

clean:
	rm -rf *.o