########
OBJS = \
    metrics.o \
    energy_node_lib.o

ifeq ($(DCGMI), 1)
OBJS += \
				dcgmi_lib.o \
				dcgmi_lib/turing.o \
				dcgmi_lib/hopper.o \
				dcgmi_lib/ampere.o \
				dcgmi_lib/common.o

dcgmilib_DEPS = dcgmi_lib/common.h
endif


all: $(OBJS)

dcgmi_lib/common.o: dcgmi_lib/common.c dcgmi_lib/common.h
	$(CC) $(CC_FLAGS) $(LIB_CFLAGS) $(CFLAGS) -c dcgmi_lib/common.c

%.o: %.c %.h
	$(CC) $(CC_FLAGS) $(LIB_CFLAGS) $(CFLAGS) -c $<


dcgmi_lib/%.o: %.c %.h $(dcgmilib_DEPS)
	$(CC) $(CC_FLAGS) $(LIB_CFLAGS) $(CFLAGS) -c $<

######## OPTIONS 

install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
