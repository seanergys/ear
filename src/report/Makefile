CFLAGS = $(CC_FLAGS) -I$(SRCDIR) -fPIC -Wall -g
DB_MYSQL_LDFLAGS = $(DB_LDFLAGS) $(SRCDIR)/common/libcommon.a
DB_PSQL_LDFLAGS = -lpq $(SRCDIR)/common/libcommon.a

######## FILES

repo_OBJS = \
	report.o
repo_BINS = \
	eard.so \
	eardbd.so \
	csv.so \
	csv_ts.so \
	mysql.so \
	psql.so \
	dummy.so
repo_PERM = 0775
repo_PATH = $(DESTDIR)/lib/plugins/reports


######## RULES

all: $(repo_OBJS) $(repo_BINS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

mysql.so:mysql.c
	$(CC) $(CFLAGS) -shared -o mysql.so mysql.c $(LDFLAGS) $(DB_MYSQL_LDFLAGS)

psql.so:psql.c
	$(CC) $(CFLAGS) -shared -o psql.so psql.c $(LDFLAGS) $(DB_PSQL_LDFLAGS)


######## SUBFOLDERS

%.components: ;

######## OPTIONS

install: repo.ginstall

%.install: ;

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
