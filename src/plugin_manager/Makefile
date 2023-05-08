######## RULES

all: libpm.a earpmd

plugin_manager.o: plugin_manager.c
	$(CC) -I $(SRCDIR) -fPIC -c $^

libpm.a: plugin_manager.o
	$(AR) rvs $@ $^

earpmd: plugin_manager.c
	$(CC) -DMAIN=1 -I $(SRCDIR) -fPIC -o $@ $^ ../common/utils/strscreen.c ../common/libcommon.a -lm -lpthread -ldl

######## OPTIONS

install: install.components;

%.install: %.install.components;

clean: clean.components rclean
	rm -f earpmd

######## SUBFOLDERS

%.components: ;
#	@ $(MAKE) -C 'plugins' $* || exit

######## DEPENDENCIES

depend: depend.components

depend-clean: depend-clean.components

include $(SRCDIR)/Makefile.extra