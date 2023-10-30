######## RULES

plug_PATH = $(DESTDIR)/lib/plugins/monitoring
plug_PERM = 0775
plug_BINS = \
    conf.so \
    dummy.so \
    eardcon.so \
    fast.so \
    keyboard.so \
    management.so \
    management_viewer.so \
    metrics.so \
    metrics_viewer.so \
    periodic_metrics.so \
    serial_eater.so \
    serial_feeder.so \
    test_cpufreq.so \
    test_gpu.so

ifdef CUDA_BASE 
ifdef KERNELS
plug_BINS += kernel_cuda.so
endif
endif

ifdef OPENCL_BASE
plug_BINS += kernel_cl.so
endif

all: $(plug_BINS) dummy.so

%.so: %.c
	$(CC) -g -I $(SRCDIR) -fPIC -shared -o $@ $^

eardcon.so: eardcon.c $(SRCDIR)/daemon/local_api/libeard.a
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

management.so: management.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(call whole,$(SRCDIR)/management/libmgt.a)

metrics.so: metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(call whole,$(SRCDIR)/metrics/libmetrics.a) $(SRCDIR)/daemon/local_api/libeard.a -lm

periodic_metrics.so: periodic_metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/report/report.o

dummy.so: dummy.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

kernel_cuda.so: kernel_cuda.c kernel_cuda_ptx.cu
	$(CUDA_BASE)/bin/nvcc -I $(SRCDIR) --compiler-options '-fPIC' -shared -o $@ $^

kernel_cl.so: kernel_cl.c
	$(CC) -I$(SRCDIR) $(OPENCL_CFLAGS) -D CL_TARGET_OPENCL_VERSION=100 -shared -fPIC -o $@ $^ $(OPENCL_LDFLAGS)

######## OPTIONS

install: plug.ginstall;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
