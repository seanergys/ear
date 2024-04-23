CFLAGS      = -fPIC -I$(SRCDIR) $(CC_FLAGS) $(MPI_CFLAGS)

######## FILES

load_OBJS = \
    loader.o \
    module_common.o \
    module_cuda.o \
    module_openmp.o \
    module_oneapi.o \
    module_default.o \

ifeq ($(MPI_CFLAGS),)
load_OBJS += \
    module_mpi.o \
    module_mpic.o \
    module_mpif.o
endif

load_BINS = libearld.so
load_DEPS = $(SRCDIR)/common/libcommon.a
load_PATH = $(DESTDIR)/lib
load_PERM = 0775

######## RULES

all: $(load_BINS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

libearld.so: $(load_OBJS) $(load_DEPS)
	$(CC) $(CC_FLAGS) -shared -o $@ $^ -ldl

######## OPTIONS 

install: load.ginstall;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra