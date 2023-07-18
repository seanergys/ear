CC     = /opt/rh/devtoolset-7/root/bin/gcc -g
CC     = gcc -g
SRCDIR = /home/xjaneas/ngit/ear_private/src
SRCDIR = /home/jordi/Documents/git/ear_private/src

export CC
export SRCDIR

######## RULES

all: all.components libpm.a edcmon

plugin_manager.o: plugin_manager.c
	$(CC) -I $(SRCDIR) -fPIC -c $^

libpm.a: plugin_manager.o
	$(AR) rvs $@ $^

edcmon: edcmon.c libpm.a
	$(CC) -I$(SRCDIR) -fPIC -rdynamic -o $@ $^ ../common/libcommon.a -lm -lpthread -ldl

######## OPTIONS

install: install.components;

%.install: %.install.components;

clean: clean.components rclean
	rm -f earpmd edcmon

######## SUBFOLDERS

%.components: ;
	@ $(MAKE) -C 'plugins' $* || exit

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra
