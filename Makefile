TOPDIR=.
INCDIR=$(TOPDIR)
INCLUDES= -I$(INCDIR)
CC=gcc
CFLAGS= -Wall $(INCLUDES)
APP=job_launcher

all:
	$(CC) job_launcher.c $(CFLAGS) -lpthread -o $(APP)

clean:
	rm -rf $(APP) *.o
