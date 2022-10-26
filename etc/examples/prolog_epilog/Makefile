SRC = /home/xjcorbalan/gitlab/regale/ear/src
BINARIES = ejob


plug_OBJS = \
        $(SRC)/daemon/remote_api/eard_rapi.o \
        $(SRC)/daemon/remote_api/eard_rapi_internals.o \
        $(SRC)/common/libcommon.a

all: $(BINARIES)

ejob:ejob.c $(SRC)/daemon/remote_api/eard_rapi.h $(SRC)/daemon/remote_api/eard_rapi.o $(SRC)/common/libcommon.a
	icc -o ejob ejob.c -I$(SRC) $(plug_OBJS)

clean:
	rm -rf $(BINARIES)
