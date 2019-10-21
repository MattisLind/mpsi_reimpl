#-------------------------------------------
# INIT

SRCS=main.c tp.c pr.c ptr.c mpsi.c
OBJS=$(SRCS:.c=.o)

CFLAGS = -Wall -DDEBUG -g


#-------------------------------------------
# TOP-LEVEL TARGETS

all: mpsiServer

install: all
	mv mpsiServer ~/bin/

clean: 
	rm $(OBJS) mpsiServer

tar:
	tar cvzf mpsiServer.tgz $(SRCS) *.h Makefile


#-------------------------------------------

mpsiServer: $(OBJS)
	$(CC) $(CFLAGS) -o mpsiServer $(OBJS) ../util/util.o ../util/attn.o -l wiringPi -lrt

$(OBJS): mpsi.h ../util/util.h ../util/attn.h
