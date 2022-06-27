CC        = /usr/bin/gcc
CC        = /opt/rh/devtoolset-7/root/bin/gcc
CC_FLAGS  = -Wall -g
SRCDIR    = ../..
SRCDIR    = /home/xjaneas/ngit/ear_private/src
CFLAGS    = $(CC_FLAGS) -I$(SRCDIR)
LDFLAGS   = -lpthread
ISST_BASE = /home/jordi/Documents/git/linux/tools/power/x86/intel-speed-select
ISST_BASE = /hpc/home/xjaneas/git/linux/tools/power/x86/intel-speed-select

######## FILES

true_OBJS = \
	isst_driver.o

root_OBJS = \
	cpuprio.o \
	isst.o

isst_OBJS = \
	$(ISST_BASE)/isst-config.o \
	$(ISST_BASE)/isst-core.o \
	$(ISST_BASE)/isst-display.o

######## RULES

all: true_test

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

true_test: $(true_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

isst.o: isst.c
	$(CC) $(CFLAGS) -I$(ISST_BASE) -D_GNU_SOURCE -c $<

test: $(root_OBJS)
	$(CC) -o $@ $(root_OBJS) $(LDFLAGS)

######## SUBFOLDERS

%.components: ;
#	@ $(MAKE) -C 'archs' $* || exit

######## OPTIONS

install: install.components

%.install: ;

clean: rclean clean.components

depend: depend.components

depend-clean: depend-clean.components

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra