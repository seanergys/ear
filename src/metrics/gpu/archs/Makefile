CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

gpu_OBJS = \
    cupti.o \
    dummy.o \
    eard.o \
    nvml.o \
    oneapi.o

ifdef CUDA_BASE
CUDA_FLAGS = -I$(CUDA_BASE)/include -DCUDA_BASE=\"$(CUDA_BASE)\"
endif

ifdef CUPTI_BASE
CUPTI_FLAGS = -I$(CUPTI_BASE)/include -DCUPTI_BASE=\"$(CUPTI_BASE)\"
endif

ifdef ONEAPI_BASE
ONEAPI_FLAGS = -I$(ONEAPI_BASE)/include -DONEAPI_BASE=\"$(ONEAPI_BASE)\"
endif

######## RULES

all: $(gpu_OBJS)

nvml.o: nvml.c nvml.h
	$(CC) $(CFLAGS) $(CUDA_FLAGS) -c $<

cupti.o: cupti.c cupti.h
	$(CC) $(CFLAGS) $(CUDA_FLAGS) $(CUPTI_FLAGS) -c $<

oneapi.o: oneapi.c oneapi.h
	$(CC) $(CFLAGS) $(ONEAPI_FLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

install: ;

clean: rclean;

######## EXPORTS

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
