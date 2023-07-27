######## RULES

all: all.components edcmon

edcmon: edcmon.c
	$(CC) -I$(SRCDIR) -fPIC -rdynamic -o $@ $^ $(call whole,$(SRCDIR)/common/libusual.a) -lm -lpthread -ldl

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
