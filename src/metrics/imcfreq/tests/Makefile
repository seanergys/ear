SRCDIR   = ../../..
CC       = /hpc/base/ctt/packages/compiler/gnu/9.1.0/bin/gcc
CC_FLAGS = -Wall -fPIC -O2 -I ../../.. -g

DEPS_GROUP = \
    $(SRCDIR)/management/libmgt.a \
    $(SRCDIR)/metrics/libmetrics.a
DEPS = \
    $(SRCDIR)/daemon/local_api/eard_api.o \
    $(SRCDIR)/common/libcommon.a

######## RULES

all: $(arch_OBJS)
	$(CC) $(CC_FLAGS) -o test test.c -Wl,--start-group $(DEPS_GROUP) -Wl,--end-group $(DEPS) -lpthread -lm

######## OPTIONS

install: ;

clean: rclean;
	rm test

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
