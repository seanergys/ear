CC                 = /usr/bin/gcc -g
EAR_BASE           = /home/jordi/Documents/git/ear_private
EAR_CFLAGS         = -I$(EAR_BASE)/src
EAR_LDFLAGS        = $(EAR_BASE)/src/common/libcommon.a -lpthread -ldl
ONEAPI_BASE        = /home/jordi/Documents/opt/level-zero-1.11.0
#ONEAPI_BASE        = /home/jordi/Documents/opt/level-zero-1.16.11
#ONEAPI_BASE        = /home/jordi/Documents/opt/level_zero
ONEAPI_CFLAGS      = -I$(ONEAPI_BASE)/include -DONEAPI_BASE=\"$(ONEAPI_BASE)\"

all: test_core test_sysman

test_core: oneapi.c
	$(CC) -DCORE=1 $(EAR_CFLAGS) $(ONEAPI_CFLAGS) -o $@ $< $(EAR_LDFLAGS)

test_sysman: oneapi.c
	$(CC) $(EAR_CFLAGS) $(ONEAPI_CFLAGS) -o $@ $< $(EAR_LDFLAGS)

clean:
	rm -f test_core test_sysman
