CC     = /usr/bin/gcc -g
SRCDIR = /home/jordi/Documents/git/ear_private/src

export CC
export SRCDIR

######## RULES

all: all.components plugin_manager

plugin_manager: plugin_manager.c
	$(CC) -rdynamic -I $(SRCDIR) -fPIC -o $@ $^ ../common/utils/strscreen.c ../common/libcommon.a -lm -lpthread -ldl

######## OPTIONS

install: install.components;

%.install: %.install.components;

clean: clean.components rclean
	rm -f plugin_manager

######## SUBFOLDERS

%.components:
	@ $(MAKE) -C 'plugins' $* || exit

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra
