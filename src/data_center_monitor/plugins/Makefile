######## RULES

plug_PATH = $(DESTDIR)/lib/plugins/monitoring
plug_PERM = 0775
plug_BINS = conf.so eardcon.so metrics.so periodic_metrics.so

all: $(plug_BINS) dummy.so

%.so: %.c $(SRCDIR)/common/libcommon.a
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

eardcon.so: eardcon.c $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/common/libcommon.a
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

metrics.so: metrics.c $(SRCDIR)/metrics/metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/metrics/libmetrics.a $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/common/libcommon.a -lm

periodic_metrics.so: periodic_metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/report/report.o $(SRCDIR)/common/libcommon.a

dummy.so: dummy.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

######## OPTIONS

install: plug.ginstall;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
