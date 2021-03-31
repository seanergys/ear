SRCDIR = ../../
CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

proc_OBJS = stat.o

######## RULES

all: $(proc_OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<
#	$(CC) $(CFLAGS) -o stat $@

######## OPTIONS

install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
