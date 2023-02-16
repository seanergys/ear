CFLAGS = $(CC_FLAGS) -fPIC -I$(SRCDIR) $(CONSTANTS) $(DB_CFLAGS)

######## FILES

#type_OBJS = $(patsubst %.c, %.o, $(shell find -maxdepth 1 -type f -name '*.c'))


ext_OBJS = $(SRCDIR)/common/external/ear_external.o

######## RULES

all: $(ext_OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

######## OPTIONS

#install: install.components

#%.install: %.install.components;

#clean: clean.components rclean;

