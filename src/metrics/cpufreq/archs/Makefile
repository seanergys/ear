CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall

######## FILES

freq_OBJS = \
    eard.o \
    easybus.o \
    dummy.o \
    intel63.o

######## RULES

all: $(freq_OBJS)

easybus.o: easybus.c easybus.h
	$(CC) $(CFLAGS) $(EASYBUS_CPPFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

install: ;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
