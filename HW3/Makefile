CC=gcc
CFLAGS=-Wall -Wextra -O2
all: router sendpkt
router: router.c common.h
	$(CC) $(CFLAGS) router.c -o router
sendpkt: sendpkt.c common.h
	$(CC) $(CFLAGS) sendpkt.c -o sendpkt
clean:
	rm -f router sendpkt
