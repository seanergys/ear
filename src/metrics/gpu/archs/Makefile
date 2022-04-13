CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

gpu_OBJS = \
    eard.o \
    nvml.o \
    dummy.o

ifdef CUDA_BASE
CUDA_FLAGS = -I$(CUDA_BASE)/include -DCUDA_BASE=\"$(CUDA_BASE)\"
endif

ifdef CUPTI_BASE
CUPTI_FLAGS = -I$(CUPTI_BASE)/include -DCUPTI_BASE=\"$(CUPTI_BASE)\"
endif

######## RULES

all: $(gpu_OBJS)

nvml.o: nvml.c nvml.h
	$(CC) $(CFLAGS) $(CUDA_FLAGS) -c $<

cupti.o: cupti.c cupti.h
	$(CC) $(CFLAGS) $(CUDA_FLAGS) $(CUPTI_FLAGS) -c $<
	$(CC) --shared -o cupti.so cupti.o $(SRCDIR)/common/libcommon.a -ldl
	$(CC) $(CFLAGS) -o test test.c -L $(CUDA_BASE)/lib64/stubs -lcuda

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

install: ;

clean: rclean;

######## EXPORTS

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
