CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall -g

######## FILES

repo_OBJS = \
	report.o
repo_BINS = \
	plug_dummy.so

######## RULES

all: $(repo_OBJS) $(repo_BINS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

######## SUBFOLDERS

%.components: ;

######## OPTIONS

install: ;

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
