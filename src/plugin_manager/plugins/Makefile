

######## RULES

all: libupmet.so libupvis.so libuprapl.so libupraplal.so

libupmet.so: plugin_metrics.c $(SRCDIR)/metrics/metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/metrics/libmetrics.a $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/common/libcommon.a -lm

libupvis.so: plugin_visuals.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/common/libcommon.a

libuprapl.so: plugin_rapl.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/metrics/libmetrics.a $(SRCDIR)/common/libcommon.a

libupraplal.so: plugin_rapl_alerts.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/common/libcommon.a

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
