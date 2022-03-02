CFLAGS  = -O2 -fPIC -I$(SRCDIR)
LDFLAGS = -ldl

ifeq ($(FEAT_MPI_API), 0)
LOAD_CFLAGS = -DMPI=0 $(CFLAGS)
else
LOAD_CFLAGS = -DMPI=1 $(MPI_CFLAGS) $(CFLAGS)
endif

######## FILES

load_OBJS = \
    loader.o \
    module_common.o \
    module_cuda.o \
    module_openmp.o \
    module_default.o

ifeq ($(FEAT_MPI_API), 1)
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
	$(CC) $(CC_FLAGS) $(LOAD_CFLAGS) -c $<

libearld.so: $(load_OBJS) $(load_DEPS)
	$(CC) $(CC_FLAGS) -shared -o $@ $^ $(LDFLAGS)

######## OPTIONS 

install: load.ginstall;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
