CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

proc_OBJS = stat.o

######## RULES

all: $(proc_OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

install: ;

clean: rclean;

depend: depend.components

depend-clean: depend-clean.components


######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra

