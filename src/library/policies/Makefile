ifeq ($(FEAT_GPU_OPT), 1)
GPU_OPT_FLAG = -DGPU_OPT=1
endif

######## FILES

include ./Makefile.files

stan_PERM = 0775
stan_PATH = $(DESTDIR)/lib/plugins/policies
stan_DEPS = \
    $(SRCDIR)/daemon/powercap/powercap_status.o \
    $(SRCDIR)/common/libcommon.a

eard_PERM = $(stan_PERM)
eard_PATH = $(DESTDIR)/lib/plugins/eard_policies
eard_DEPS = $(SRCDIR)/common/libcommon.a

########

.PRECIOUS: %.o

all: $(poli_OBJS) $(stan_OBJS) $(stan_BINS) $(eard_BINS)
 
%.o: %.c 
	$(CC) $(CC_FLAGS) $(CFLAGS) $(LIB_CFLAGS) $(GPU_OPT_FLAG) -c $<

%.so: %.o $(stan_OBJS) $(stan_DEPS) 
	$(CC) $(CC_FLAGS) -shared -o $@ $^ 

%_eard.so: %_eard.o $(eard_DEPS)
	$(CC) $(CC_FLAGS) -shared -o $@ $^

######## OPTIONS 

install: stan.ginstall eard.ginstall
	ln -sf ./min_energy_eard.so $(eard_PATH)/min_energy_no_models_eard.so 
	ln -sf ./min_time_eard.so $(eard_PATH)/min_time_no_models_eard.so 

clean: rclean;

######## DEPENDENCIES

include $(SRCDIR)/Makefile.depend
include $(SRCDIR)/Makefile.extra
