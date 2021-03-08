SRCDIR   = ../..
CC       = /hpc/base/ctt/packages/compiler/gnu/9.1.0/bin/gcc
CC_FLAGS = -Wall -fPIC -O2 -I $(SRCDIR)

DEPS = \
    $(SRCDIR)/database_cache/eardbd_api.o \
    $(SRCDIR)/common/libcommon.a

######## RULES

all: $(DEPS)
	$(CC) $(CC_FLAGS) -o test test.c $(DEPS)

######## OPTIONS

install: ;

clean: rclean;
	rm test

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
