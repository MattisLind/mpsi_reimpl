#-------------------------------------------
# INIT

ifeq ($(mode),standalone)
  GPIO_OBJ = ../util/gpioDummy.o
else
  GPIO_OBJ = -l wiringPi -lrt
endif

SRCS=main.c tpf.c misc.c t98_mpsi.c t98_65.c helpdoc.c
OBJS=$(SRCS:.c=.o)

UTIL_OBJS=../util/cp.o ../util/util.o ../util/attn.o

CFLAGS = -Wall -DDEBUG -g


#-------------------------------------------
# TOP-LEVEL TARGETS

all: tp9830 tp9865

install: all
	mv tp9830 ~/bin/
	mv tp9865 ~/bin/

clean: 
	rm  helpdoc.c $(OBJS) tp9830 tp9865

tar:
	tar cvzf tp9830.tgz helpdoc.txt $(SRCS) *.h Makefile


#-------------------------------------------

tp9830: $(OBJS)
	$(CC) $(CFLAGS) -o tp9830 t98_mpsi.o main.o tpf.o misc.o helpdoc.o ../mpsiServer/mpsi.o $(UTIL_OBJS) $(GPIO_OBJ)

tp9865: $(OBJS)
	$(CC) $(CFLAGS) -o tp9865 t98_65.o main.o tpf.o misc.o helpdoc.o $(UTIL_OBJS) $(GPIO_OBJ)

$(OBJS): tpf.h t98.h ../util/cp.h ../util/util.h

helpdoc.c: helpdoc.txt
	awk 'BEGIN {print "char* HelpSrc=\"\\"} {printf "%s\\n\\\n", $$0} END {print "\";"}' helpdoc.txt >helpdoc.c
