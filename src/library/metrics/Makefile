########
OBJS = \
    metrics.o \
    energy_node_lib.o

ifeq ($(DCGMI), 1)
OBJS += \
				dcgmi_lib.o \
				dcgmi_lib/turing.o \
				dcgmi_lib/hopper.o \
				dcgmi_lib/common.o
endif

all: $(OBJS)

%.o: %.c %.h
	$(CC) $(CC_FLAGS) $(LIB_CFLAGS) $(CFLAGS) -c $<

######## OPTIONS 

install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
