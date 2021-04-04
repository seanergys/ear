/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* Copyright © 2017-present BSC-Lenovo
* BSC Contact   mailto:ear-support@bsc.es
* Lenovo contact  mailto:hpchelp@lenovo.com
*
* This file is licensed under both the BSD-3 license for individual/non-commercial
* use and EPL-1.0 license for commercial use. Full text of both licenses can be
* found in COPYING.BSD and COPYING.EPL files.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <common/config.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <library/common/externs.h>
#include <library/policies/policy_api.h>
#include <library/policies/policy_state.h>
#include <library/common/verbose_lib.h>
#include <library/metrics/gpu.h>


static uint last_pc=0;

#ifdef EARL_RESEARCH
extern unsigned long ext_def_freq;
#define DEF_FREQ(f) (!ext_def_freq?f:ext_def_freq)
#else
#define DEF_FREQ(f) f
#endif

#if 0
#define debug(...) \
{ \
        dprintf(2, __VA_ARGS__); \
        dprintf(2, "\n"); \
}
#endif

#define LOW_UTIL_GPU_TH 25
#define MED_UTIL_GPU_TH 50
#define HIGH_UTIL_GPU_TH 75
#define LOW_GPU_FREQ_RED 0.5
#define MED_GPU_FREQ_RED 0.25
#define HIGH_GPU_FREQ_RED 0.1

static const ulong **gpuf_list;
static const uint *gpuf_list_items;
static	ulong *gfreqs;
state_t policy_init(polctx_t *c)
{
	int i,j;
	ulong g_freq = 0;
  char *gpu_freq=getenv(SCHED_EAR_GPU_DEF_FREQ);
  state_t ret;
  if ((gpu_freq!=NULL) && (c->app->user_type==AUTHORIZED)){
    g_freq=atol(gpu_freq);
  }
	gpu_lib_freq_list(&gpuf_list, &gpuf_list_items);
	for (i=0;i<c->num_gpus;i++){
		verbose_master(3,"Freqs in GPU[%d]=%u",i,gpuf_list_items[i]);
		for (j=0;j<gpuf_list_items[i];j++){
			verbose_master(3,"GPU[%d][%d]=%.2f",i,j,(float)gpuf_list[i][j]/1000000.0);
		}
	}
	gpu_lib_alloc_array(&gfreqs);
	for (i=0;i<c->num_gpus;i++){
		if (g_freq) gfreqs[i] = g_freq;
		else 				gfreqs[i] = gpuf_list[i][0];
		verbose_master(2,"Setting GPUgreq[%d] = %.2f",i,(float)gfreqs[i]/1000000.0);
	}
	gpu_lib_freq_limit_set(gfreqs);
	
	return EAR_SUCCESS;
}
state_t policy_apply(polctx_t *c,signature_t *my_sig, node_freqs_t *freqs,int *ready)
{
	ulong *new_freq=freqs->gpu_freq;
	ulong eff_f,f;
	uint i;
	ulong util,freq;
	int new_gfreq_pos;
	#if USE_GPUS
	debug("apply: num_gpus %d",my_sig->gpu_sig.num_gpus);
	for (i=0;i<my_sig->gpu_sig.num_gpus;i++){
		debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]",i,
		my_sig->gpu_sig.gpu_data[i].GPU_power,(float)my_sig->gpu_sig.gpu_data[i].GPU_freq/1000.0,(float)my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
		my_sig->gpu_sig.gpu_data[i].GPU_util,my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}

	for (i=0;i<my_sig->gpu_sig.num_gpus;i++){
		util = my_sig->gpu_sig.gpu_data[i].GPU_util;
		freq = my_sig->gpu_sig.gpu_data[i].GPU_freq;
		if (util > 0){		
			if (util < LOW_UTIL_GPU_TH){
				new_gfreq_pos = gpuf_list_items[i] * LOW_GPU_FREQ_RED;
				new_freq[i] = gpuf_list[i][new_gfreq_pos];
				verbose_master(2,"GPU[%d] Low GPU util: Util %lu Curr GPUf %lu New GPUF %lu",i,util,freq,new_freq[i]);
			}else if (util < MED_UTIL_GPU_TH){
				new_gfreq_pos = gpuf_list_items[i] * MED_GPU_FREQ_RED;
				new_freq[i] = gpuf_list[i][new_gfreq_pos];
				verbose_master(2,"GPU[%d] Med GPU util: Util %lu Curr GPUf %lu New GPUF %lu",i,util,freq,new_freq[i]);
			}else if (util < HIGH_UTIL_GPU_TH ){
				new_gfreq_pos = gpuf_list_items[i] * HIGH_GPU_FREQ_RED;
				new_freq[i] = gpuf_list[i][new_gfreq_pos];
				verbose_master(2,"GPU[%d] High GPU util: Util %lu Curr GPUf %lu New GPUF %lu",i,util,freq,new_freq[i]);
			}else{		
				new_freq[i] = gfreqs[i];
				verbose_master(2,"GPU[%d] Very High GPU util: Util %lu Curr GPUf %lu New GPUF %lu",i,util,freq,new_freq[i]);
			}
		}else{
			new_freq[i]=freq;
			verbose_master(2,"GPU[%d] No GPU util: Util %lu Curr GPUf %lu New GPUF %lu",i,util,freq,new_freq[i]);
		}
	}		
	
	#endif
	*ready = EAR_POLICY_READY;
	
	return EAR_SUCCESS;
}
state_t policy_ok(polctx_t *c, signature_t *my_sig,signature_t *prev_sig,int *ok)
{
	ulong eff_f;
	uint i;
	#if USE_GPUS
	debug("num_gpus %d",my_sig->gpu_sig.num_gpus);
	for (i=0;i<my_sig->gpu_sig.num_gpus;i++){
		debug("[GPU %d Power %.2lf Freq %.2f Mem_freq %.2f Util %lu Mem_util %lu]",i,
		my_sig->gpu_sig.gpu_data[i].GPU_power,(float)my_sig->gpu_sig.gpu_data[i].GPU_freq/1000.0,(float)my_sig->gpu_sig.gpu_data[i].GPU_mem_freq/1000.0,
		my_sig->gpu_sig.gpu_data[i].GPU_util,my_sig->gpu_sig.gpu_data[i].GPU_mem_util);
	}
	#endif
	*ok=1;

	return EAR_SUCCESS;
}


state_t  policy_get_default_freq(polctx_t *c, ulong *freq_set)
{
	return EAR_SUCCESS;
}

state_t policy_max_tries(polctx_t *c,int *intents)
{
	*intents=0;
	return EAR_SUCCESS;
}


