CC     = /usr/bin/gcc -g
SRCDIR = /home/jordi/Documents/git/ear_private/src

export CC
export SRCDIR

######## RULES

all: all.components libpm.a earpmd simulatron

plugin_manager.o: plugin_manager.c
	$(CC) -I $(SRCDIR) -fPIC -c $^

libpm.a: plugin_manager.o
	$(AR) rvs $@ $^

earpmd: plugin_manager.c
	$(CC) -DMAIN=1 -I $(SRCDIR) -fPIC -o $@ $^ ../common/libcommon.a -lm -lpthread -ldl

simulatron: simulatron.c
	$(CC) -I $(SRCDIR) -fPIC -o $@ $^ libpm.a ../common/libcommon.a -lm -lpthread -ldl

######## OPTIONS

install: install.components;

%.install: %.install.components;

clean: clean.components rclean
	rm -f earpmd simulatron

######## SUBFOLDERS

%.components: ;
	@ $(MAKE) -C 'plugins' $* || exit

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra
