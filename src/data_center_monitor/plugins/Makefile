######## RULES

plug_PATH = $(DESTDIR)/lib/plugins/monitoring
plug_PERM = 0775
plug_BINS = conf.so eardcon.so management.so metrics.so periodic_metrics.so

all: $(plug_BINS) dummy.so

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

######## OPTIONS

install: plug.ginstall;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
