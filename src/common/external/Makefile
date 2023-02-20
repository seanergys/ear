CFLAGS = $(CC_FLAGS) -fPIC -I$(SRCDIR) $(CONSTANTS) $(DB_CFLAGS)

######## FILES

#type_OBJS = $(patsubst %.c, %.o, $(shell find -maxdepth 1 -type f -name '*.c'))


ext_LIB = libext.a
ext_OBJS = \
		   $(SRCDIR)/common/external/ear_external.o \
		   $(SRCDIR)/common/system/shared_areas.o
		   

######## RULES

all: $(ext_OBJS) $(ext_LIB)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

libext.a: $(ext_OBJS)
	$(AR) rvs $@ $(ext_OBJS) 

######## OPTIONS

install: 

%.install: ;

clean: rclean;



######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra

