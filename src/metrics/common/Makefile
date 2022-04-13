CFLAGS = $(CC_FLAGS) -fPIC -I$(SRCDIR) $(CONSTANTS) 

######## FILES

comm_OBJS = \
    apis.o \
    cupti.o \
    pstate.o \
    hsmp.o \
    hwmon.o \
    likwid.o \
    msr.o \
    nvml.o \
    omsr.o \
    oneapi.o \
    pci.o \
    perf.o

ifdef LIKWID_BASE
LIKWID_FLAGS = -I$(LIKWID_BASE)/include -DLIKWID_BASE=\"$(LIKWID_BASE)\"
endif

######## RULES

all: $(comm_OBJS)

likwid.o: likwid.c likwid.h
	$(CC) $(CFLAGS) $(LIKWID_FLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## SUBFOLDERS

%.components:
	@ $(MAKE) -C 'cpu' $* || exit

######## OPTIONS

install: ;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra