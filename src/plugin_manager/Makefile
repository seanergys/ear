CC     = /usr/bin/gcc -g
SRCDIR = /home/jordi/Documents/git/ear_private/src

######## RULES

all: upm libupmet.so libupvis.so libuprapl.so libupraplal.so

upm: upm.c
	$(CC) -rdynamic -I $(SRCDIR) -fPIC -o $@ $^ ../common/utils/strscreen.c ../common/libcommon.a -lm -lpthread -ldl

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

clean: clean.components rclean
	rm -f upm

######## SUBFOLDERS

%.components: ;

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra
