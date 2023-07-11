######## RULES

all: libupmet.so libupper.so libupconf.so libupdummy.so

libupmet.so: metrics.c $(SRCDIR)/metrics/metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/metrics/libmetrics.a $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/common/libcommon.a -lm

libupper.so: periodic_metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/report/report.o $(SRCDIR)/common/libcommon.a

libupconf.so: config.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/common/libcommon.a

libupdummy.so: dummy.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^

######## OPTIONS

install: install.components;

%.install: %.install.components;

clean: clean.components rclean;

######## SUBFOLDERS

%.components: ;

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra
