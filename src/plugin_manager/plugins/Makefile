######## RULES

all: metrics.so periodic_metrics.so conf.so dummy.so

metrics.so: metrics.c $(SRCDIR)/metrics/metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/metrics/libmetrics.a $(SRCDIR)/daemon/local_api/libeard.a $(SRCDIR)/common/libcommon.a -lm

periodic_metrics.so: periodic_metrics.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/report/report.o $(SRCDIR)/common/libcommon.a

conf.so: conf.c
	$(CC) -I $(SRCDIR) -fPIC -shared -o $@ $^ $(SRCDIR)/common/libcommon.a

dummy.so: dummy.c
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
