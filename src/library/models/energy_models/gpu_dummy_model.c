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
* EAR is an open source software, and it is licensed under both the BSD-3 license
* and EPL-1.0 license. Full text of both licenses can be found in COPYING.BSD
* and COPYING.EPL files.
*/

#define SHOW_DEBUGS 1

#include <stdlib.h>
#if !SHOW_DEBUGS
#define NDEBUG
#endif
#include <assert.h>

#include <common/config.h>
#include <common/states.h>
#include <common/output/debug.h>
#include <common/types/signature.h>
#include <common/types/coefficient.h>
#include <common/hardware/architecture.h>

#include <management/gpu/gpu.h>
#include <library/models/energy_models/common.h>
#include <library/metrics/metrics.h>


/* These coefficiency type must be specific per model */
/* Do we need a gpu_coefficient_t or can we use the same ? */
static coefficient_t **coefficients;
static coefficient_t *coefficients_sm;
static int num_coeffs;
static uint num_gpu_pstates;
static uint num_gpus = 0;
static uint gpu_model_init = 0;
static const ulong **gpuf_list;
static const uint *gpuf_list_items;


#define gpu_valid_pstate(p) (p < num_gpu_pstates)

/** Returns whether the pair <from_ps, to_ps> is between the configured pstate list. */
static int valid_range(ulong from_ps, ulong to_ps);

/** Computes the projection of the \ref signature loop time by using \ref coeff.
 * \pre All pointers must be initialized. Also freq_dst and signature's CPI must be non-zero. */
static double project_time(coefficient_t *coeff, double FP64, double FP16, double FP32, double TENSOR, double CTIME, double DRAM);



/** Computes the projection of the \ref signature DC node power by using \ref coeffs.
 * \pre Input arguments must be initialized. */
static double project_power(coefficient_t *coeff,double FP64, double FP16, double FP32, double TENSOR, double CPOWER, double DRAM);


/** Returns whether there exists a coefficients entry from \ref from_ps to \ref to_ps. */
static uint projection_available(ulong from_ps, ulong to_ps);


/** This function loads any information needed by the energy model.
 * It is called when the model just has been loaded. */
state_t energy_model_init(char *ear_etc_path, char *ear_tmp_path, architecture_t *arch_desc)
{
  uint i, ref;
  char * hack_file = ear_getenv(HACK_EARL_GPU_COEFF_FILE);

	debug("Using gpu_model");

	/* In CPU , architecture includes CPU list of pstates. Should we extend architecture_t to include GPU data ? */
	VERB_SET_EN(1);

	/* At this point, we assume GPU mgt has been already initialized */
	num_gpus        = (uint)metrics_gpus_get(MGT_GPU)->devs_count;
	gpuf_list       = (const ulong **) metrics_gpus_get(MGT_GPU)->avail_list;
       	gpuf_list_items = (const uint *)   metrics_gpus_get(MGT_GPU)->avail_count;
	num_gpu_pstates = gpuf_list_items[0];


	debug("GPU model: %u GPUs and %u gpu_pstates", num_gpus, num_gpu_pstates);

	/* We need num_gpu_pstates */
  coefficients = (coefficient_t **) malloc(sizeof(coefficient_t *) * num_gpu_pstates);

  if (coefficients == NULL) {
		return EAR_ERROR;
  }
  for (i = 0; i < num_gpu_pstates; i++)
  {
    coefficients[i] = (coefficient_t *) malloc(sizeof(coefficient_t) * num_gpu_pstates);
    if (coefficients[i] == NULL) {
			return EAR_ERROR;
    }

    for (ref = 0; ref < num_gpu_pstates; ref++)
    {
      /* We assume all the GPUs have the same list of frequencies */	
      coefficients[i][ref].pstate_ref = gpuf_list[0][i]; 
      coefficients[i][ref].pstate = gpuf_list[0][ref];   
      coefficients[i][ref].available = 1;
    }
  }


  char coeffs_path[GENERIC_NAME];


#if 0
  if (hack_file == NULL){
	  verbose(0, "ERROR: GPU coefficients are not yet supported in ear.conf. Define %s", HACK_EARL_GPU_COEFF_FILE);
	  return EAR_ERROR;
  }else{
    int cfile_size;
    strcpy(coeffs_path,hack_file);
    debug("Using hck coefficients path %s", coeffs_path);
    cfile_size = coeff_file_size(coeffs_path);
    if (cfile_size == 0) num_coeffs = 0;
    else{
      int num_coeffs_in_file = cfile_size / sizeof(coefficient_t);
      num_coeffs = cfile_size ;
      debug("%d coefficients found in file", num_coeffs_in_file);
      coefficients_sm = (coefficient_t *) calloc(num_coeffs_in_file, sizeof(coefficient_t));
      coeff_file_read_no_alloc(coeffs_path, coefficients_sm, cfile_size);
      debug("Coefficients read from file");
    }
  }

  if (num_coeffs > 0){
      num_coeffs = num_coeffs / sizeof(coefficient_t);

      int ccoeff;
      state_t st, st1;
      for (ccoeff=0;ccoeff<num_coeffs;ccoeff++){
          st = mgt_gpu_freq_get_next(no_ctx, 0, coefficients_sm[ccoeff].pstate_ref, &ref, FREQ_TOP); 
          st1 = mgt_gpu_freq_get_next(no_ctx, 0, coefficients_sm[ccoeff].pstate, &i, FREQ_TOP); 
          if (state_ok(st) && state_ok(st1)){  
							debug("Freqs %lu and %lu found at indexes %u and %u", coefficients_sm[ccoeff].pstate_ref, 	coefficients_sm[ccoeff].pstate, ref, i);
              memcpy(&coefficients[ref][i],&coefficients_sm[ccoeff],sizeof(coefficient_t));
          }else{
						debug("Freqs %lu and %lu not found", coefficients_sm[ccoeff].pstate_ref,   coefficients_sm[ccoeff].pstate);
					}
      }
  }
#endif
	gpu_model_init=1;	
	return EAR_SUCCESS;
}


state_t energy_model_project_time(signature_t *signature, ulong from_ps, ulong to_ps, double *proj_time)
{
	state_t st = EAR_SUCCESS;
	coefficient_t *coeff;

	if (proj_time == NULL) return EAR_ERROR;
	memset(proj_time, 0, sizeof(double) * num_gpus);

	if (from_ps == to_ps)
	{
		for (uint g = 0; g < num_gpus; g++) proj_time[g] = signature->time;
		return st;
	}

	if (gpu_model_init && valid_range(from_ps, to_ps))
	{
		coeff = &coefficients[from_ps][to_ps];
		if (coeff->available)
		{
		sig_ext_t *sigex;
  	dcgmi_sig_t *dcgmis;
		/* SIgnature Extended */
		sigex = (sig_ext_t *)signature->sig_ext;
		if (sigex == NULL)  return EAR_ERROR;
		/* DCGMI data */
		dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
		if (dcgmis == NULL) return EAR_ERROR;
		/* Sets where each data is placed. Could be the same */
		dcgmi_t *dcgmis_data_64 = dcgmis->dcgmi_metrics[FP64_SET];
		dcgmi_t *dcgmis_data_32 = dcgmis->dcgmi_metrics[FP32_SET];
		dcgmi_t *dcgmis_data_16 = dcgmis->dcgmi_metrics[FP16_SET];
		dcgmi_t *dcgmis_data_ten = dcgmis->dcgmi_metrics[TNS_SET];
		dcgmi_t *dcgmis_data_dram = dcgmis->dcgmi_metrics[DRAM_SET];
		if ((dcgmis_data_64 == NULL) || (dcgmis_data_32 == NULL) || (dcgmis_data_16 == NULL) 
		 || (dcgmis_data_ten == NULL) || (dcgmis_data_dram == NULL)) 
						return EAR_ERROR;


			for (uint g = 0; g < num_gpus; g++) {

			double CTIME = signature->time;
			double FP64 = dcgmis_data_64[g].values[FP64_EV];
			double FP16  = dcgmis_data_16[g].values[FP16_EV];
			double FP32 = dcgmis_data_32[g].values[FP32_EV];
			double TENSOR = dcgmis_data_ten[g].values[TNS_EV];
			double DRAM   = dcgmis_data_dram[g].values[DRAM_EV];

			proj_time[g]  = project_time(coeff, FP64, FP16, FP32, TENSOR, CTIME, DRAM); // PENDING
			}
		}
	} else st = EAR_ERROR;

	return st;
}


state_t energy_model_project_power(signature_t *signature, ulong from_ps, ulong to_ps, double *proj_power)
{
	state_t st = EAR_SUCCESS;
	coefficient_t *coeff;

	assert(proj_power != NULL);

	if (proj_power == NULL) return EAR_ERROR;

	memset(proj_power, 0, sizeof(double) * num_gpus);

	if (from_ps == to_ps)
	{
		for (uint g = 0; g < num_gpus; g++) proj_power[g] = signature->gpu_sig.gpu_data[g].GPU_power;
		return st;
	}

	debug("module init: %d - valid: %d", gpu_model_init, valid_range(from_ps,to_ps));

	if (gpu_model_init && valid_range(from_ps, to_ps))
	{
		coeff = &coefficients[from_ps][to_ps];
		if (coeff->available)
		{
			sig_ext_t *sigex;
			dcgmi_sig_t *dcgmis;

			/* SIgnature Extended */
			sigex = (sig_ext_t *)signature->sig_ext;
			if (sigex == NULL)  return EAR_ERROR;

			/* DCGMI data */
			dcgmis = (dcgmi_sig_t *)&sigex->dcgmis;
			if (dcgmis == NULL) return EAR_ERROR;

			/* Sets where each data is placed. Could be the same. */

			int fp64_info[2];
			dcgmi_lib_get_event_info(fp64_active, fp64_info);
			dcgmi_t *dcgmis_data_64 = dcgmis->dcgmi_metrics[fp64_info[event_info_set]];

			int fp32_info[2];
			dcgmi_lib_get_event_info(fp32_active, fp32_info);
			dcgmi_t *dcgmis_data_32 = dcgmis->dcgmi_metrics[fp32_info[event_info_set]];

			int fp16_info[2];
			dcgmi_lib_get_event_info(fp16_active, fp16_info);
			dcgmi_t *dcgmis_data_16   = dcgmis->dcgmi_metrics[fp16_info[event_info_set]];

			int tensor_info[2];
			dcgmi_lib_get_event_info(tensor_active, tensor_info);
			dcgmi_t *dcgmis_data_ten  = dcgmis->dcgmi_metrics[tensor_info[event_info_set]];

			int dram_info[2];
			dcgmi_lib_get_event_info(dram_active, dram_info);
			dcgmi_t *dcgmis_data_dram = dcgmis->dcgmi_metrics[dram_info[event_info_set]];

			if (!dcgmis_data_64 || !dcgmis_data_32 || !dcgmis_data_16
					|| !dcgmis_data_ten || !dcgmis_data_dram)
				return EAR_ERROR;

			for (uint g = 0; g < num_gpus; g++)
			{
				debug("Going to project the power for the GPU %d...");
				double CPOWER = signature->gpu_sig.gpu_data[g].GPU_power;

				double FP64 = dcgmis_data_64[g].values[fp64_info[event_info_pos]];
				double FP16  = dcgmis_data_16[g].values[fp16_info[event_info_pos]];
				double FP32 = dcgmis_data_32[g].values[fp32_info[event_info_pos]];
				double TENSOR = dcgmis_data_ten[g].values[tensor_info[event_info_pos]];
				double DRAM   = dcgmis_data_dram[g].values[dram_info[event_info_pos]];

				proj_power[g] = project_power(coeff, FP64, FP16, FP32, TENSOR, CPOWER, DRAM);
				proj_power[g] = CPOWER * (from_ps - to_ps) * 0.05;
			}
		} else
		{
			debug("Coeffs from %lu to %lu not available",from_ps,to_ps);
			st = EAR_ERROR;
		}
	} else st = EAR_ERROR;

	return st;
}


uint energy_model_projection_available(ulong from_ps, ulong to_ps)
{
	return projection_available(from_ps, to_ps);
}


uint energy_model_all_projections_available()
{
	uint not_avail = 0;

	debug("Checking availability for %u p-states", num_gpu_pstates);
	for (uint ps = 0; ps < num_gpu_pstates && !not_avail; ps++)
	{
		for (uint pt = 0; pt < num_gpu_pstates && !not_avail; pt++)
		{
			if (ps != pt && !projection_available(ps, pt)) not_avail = 1;	
		}
	}

	return 1 - not_avail;
}


static int valid_range(ulong from_ps, ulong to_ps)
{
	if ((from_ps < num_gpu_pstates) && (to_ps < num_gpu_pstates)) return 1;
	else return 0;
}



static double project_time(coefficient_t *coeff, double FP64, double FP16, double FP32, double TENSOR, double CTIME, double DRAM)
{
	debug("project_time with FP 64 %lf FP16 %lf FP32 %lf TENSOR %lf CTIME %lf DRAM %lf", FP64, FP16, FP32, TENSOR, CTIME, DRAM);
	/* Replace this function with the model */
	return (double)CTIME;
}


static double project_power(coefficient_t *coeff, double FP64, double FP16, double FP32, double TENSOR, double CPOWER, double DRAM)
{
	debug("project_power with FP 64 %lf FP16 %lf FP32 %lf TENSOR %lf CPOWER %lf DRAM %lf", FP64, FP16, FP32, TENSOR, CPOWER, DRAM);
	/* Replace this function with the model */
	return (double)CPOWER;
}


static uint projection_available(ulong from_ps, ulong to_ps)
{
	if (valid_range(from_ps, to_ps))
	{
		if (coefficients[from_ps][to_ps].available)
		{
			return 1;
		}
	}

	return 0;
}
