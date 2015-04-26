TOPDIR=$(shell pwd)
INCDIR=$(TOPDIR)
INCLUDES= -I$(INCDIR)/comlink
CC=gcc
CFLAGS= -Wall $(INCLUDES)
LDFLAGS= -lpthread

launcher_src=launcher/job_launcher.c comlink/comlink.c
listener_src=listener/listener.c comlink/comlink.c

launcher_objs=$(foreach src,$(launcher_src),$(subst .c,.o,$(src)))
listener_objs=$(foreach src,$(listener_src),$(subst .c,.o,$(src)))

all: job_launcher listener_stub #comlink_lib

%.o:%.c
	@echo CC $<
	$(CC) -c $(CFLAGS) -o $@ $<

job_launcher: $(launcher_objs)
	@echo LD $@
	$(CC) $(LDFLAGS) -o $@ $(launcher_objs)

listener_stub: $(listener_objs)
	@echo LD $@
	$(CC) $(LDFLAGS) -o $@ $(listener_objs)

distclean: clean
	rm -rf cscope*

clean:
	rm -rf *.o launcher/*.o listener/*.o comlink/*.o \
	job_launcher listener_stub
