######## RULES

plug_PATH = $(DESTDIR)/lib/plugins/monitoring
plug_PERM = 0775
plug_BINS = \
    conf.so \
    eardcon.so \
    keyboard.so \
    management.so \
    metrics.so \
    metrics_viewer.so \
    periodic_metrics.so \
    test_cpufreq.so \
    test_gpu.so

all: $(plug_BINS) dummy.so cuda_kernel.so

%.so: %.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

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

cuda_kernel.so: cuda_kernel.c
	nvcc -I $(SRCDIR) --compiler-options '-fPIC' -shared -o cuda_kernel.so cuda_kernel.c cuda_ptx.cu
	#nvcc -I $(SRCDIR) --compiler-options '-fPIC' -c cuda_ptx.cu
	#$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ cuda_ptx.o -lcudart 

######## OPTIONS

install: plug.ginstall;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
