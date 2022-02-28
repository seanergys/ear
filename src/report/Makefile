CFLAGS           = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall -g $(CONSTANTS)
DB_MYSQL_LDFLAGS = $(DB_LDFLAGS) $(SRCDIR)/common/libcommon.a
DB_PSQL_LDFLAGS  = -lpq $(SRCDIR)/common/libcommon.a

######## FILES

repo_OBJS = \
	report.o
repo_BINS = \
	eard.so \
	eardbd.so \
	csv.so \
	csv_ts.so \
    heroes.so \
	dummy.so
repo_PERM = 0775
repo_PATH = $(DESTDIR)/lib/plugins/reports

ifeq ($(FEAT_DB_PSQL),1)
repo_BINS += \
	psql.so 
endif

ifeq ($(FEAT_DB_MYSQL),1)
repo_BINS += \
	mysql.so 
endif

######## RULES

all: $(repo_OBJS) $(repo_BINS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

eard.so: eard.c $(SRCDIR)/daemon/local_api/eard_api.c $(SRCDIR)/common/libcommon.a
	$(CC) $(CFLAGS) -shared -o eard.so  eard.c $(SRCDIR)/daemon/local_api/eard_api.c $(LDFLAGS) $(SRCDIR)/common/libcommon.a

eardbd.so: eardbd.c $(SRCDIR)/database_cache/eardbd_api.c $(SRCDIR)/common/libcommon.a
	$(CC) $(CFLAGS) -shared -o eardbd.so eardbd.c $(SRCDIR)/database_cache/eardbd_api.c $(LDFLAGS) $(SRCDIR)/common/libcommon.a

ifeq ($(FEAT_DB_MYSQL),1)
mysql.so: mysql.c
	$(CC) $(CFLAGS) -shared -o mysql.so mysql.c $(LDFLAGS) $(DB_MYSQL_LDFLAGS)
endif

ifeq ($(FEAT_DB_PSQL),1)
psql.so: psql.c
	$(CC) $(CFLAGS) -shared -o psql.so psql.c $(LDFLAGS) $(DB_PSQL_LDFLAGS)
endif

######## SUBFOLDERS

%.components: ;

######## OPTIONS

install: repo.ginstall

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
