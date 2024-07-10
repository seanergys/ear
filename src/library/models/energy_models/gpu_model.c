/*
*
* This program is part of the EAR software.
*
* EAR provides a dynamic, transparent and ligth-weigth solution for
* Energy management. It has been developed in the context of the
* Barcelona Supercomputing Center (BSC)&Lenovo Collaboration project.
*
* 
* BSC Contact   mailto:ear-support@bsc.es
* 
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
/* Do we need a gpu_coefficient_gpu_t or can we use the same ? */
static coefficient_gpu_t **coefficients;
static coefficient_gpu_t *coefficients_sm;
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
static double project_time(coefficient_gpu_t *coeff, double ctime, double fp_act, double dram_act, ulong to_freq);



/** Computes the projection of the \ref signature DC node power by using \ref coeffs.
 * \pre Input arguments must be initialized. */
static double project_power(coefficient_gpu_t *coeff, double cpower, double fp_act, double dram_act, ulong to_freq);


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
  coefficients = (coefficient_gpu_t **) malloc(sizeof(coefficient_gpu_t *) * num_gpu_pstates);

  if (coefficients == NULL) {
		return EAR_ERROR;
  }
  for (i = 0; i < num_gpu_pstates; i++)
  {
    coefficients[i] = (coefficient_gpu_t *) malloc(sizeof(coefficient_gpu_t) * num_gpu_pstates);
    if (coefficients[i] == NULL) {
			return EAR_ERROR;
    }

    //debug("index i %d freq %lu", i, gpuf_list[0][i]);
    for (ref = 0; ref < num_gpu_pstates; ref++)
    {
      /* We assume all the GPUs have the same list of frequencies */	
      coefficients[i][ref].pstate_ref = gpuf_list[0][i]; 
      coefficients[i][ref].pstate = gpuf_list[0][ref];   
      coefficients[i][ref].available = 0;
    }
  }

  char coeffs_path[GENERIC_NAME];

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
      int num_coeffs_in_file = cfile_size / sizeof(coefficient_gpu_t);
      num_coeffs = cfile_size ;
      debug("%d coefficients found in file", num_coeffs_in_file);
      coefficients_sm = (coefficient_gpu_t *) calloc(num_coeffs_in_file, sizeof(coefficient_gpu_t));
      coeff_gpu_file_read_no_alloc(coeffs_path, coefficients_sm, cfile_size);
    }
  }

  if (num_coeffs > 0){
      num_coeffs = num_coeffs / sizeof(coefficient_gpu_t);
      // TODO ref=0 (projection form nominal) and i from 0 to num_coeffs_in_file
      int ccoeff;
      state_t st, st1;
      for (ccoeff=0; ccoeff<num_coeffs; ccoeff++){
          st = mgt_gpu_freq_get_next(no_ctx, 0, coefficients_sm[ccoeff].pstate_ref, &ref, FREQ_TOP); 
          st1 = mgt_gpu_freq_get_next(no_ctx, 0, coefficients_sm[ccoeff].pstate, &i, FREQ_TOP); 
          if (state_ok(st) && state_ok(st1)){  
							//debug("Freqs %lu and %lu found at indexes %u and %u", coefficients_sm[ccoeff].pstate_ref, 	coefficients_sm[ccoeff].pstate, ref, i);
              memcpy(&coefficients[ref][i], &coefficients_sm[ccoeff], sizeof(coefficient_gpu_t));
          }else{
						debug("Freqs %lu and %lu not found", coefficients_sm[ccoeff].pstate_ref, coefficients_sm[ccoeff].pstate);
					}
      }
  }
	gpu_model_init=1;	
	return EAR_SUCCESS;
}

// TODO
state_t energy_model_project_time(signature_t *signature, ulong from_ps, ulong to_ps, double *proj_time)
{
	state_t st = EAR_SUCCESS;
	coefficient_gpu_t *coeff;

	if (proj_time == NULL) return EAR_ERROR;
	//memset(proj_time, 0, sizeof(double) * num_gpus);
	memset(proj_time, 0, sizeof(double));

	if (from_ps == to_ps)
	{
    //for (uint g = 0; g < num_gpus; g++) proj_time[g] = signature->time;
		proj_time[0] = signature->time;
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

		int fp64_info[2];
		dcgmi_lib_get_event_info(fp64_active, &fp64_info);
		gpuprof_t *dcgmis_data_64 = dcgmis->event_metrics_sets[fp64_info[event_info_set]];

		int fp32_info[2];
		dcgmi_lib_get_event_info(fp32_active, &fp32_info);
		gpuprof_t *dcgmis_data_32 = dcgmis->event_metrics_sets[fp32_info[event_info_set]];

		int fp16_info[2];
		dcgmi_lib_get_event_info(fp16_active, &fp16_info);
		gpuprof_t *dcgmis_data_16   = dcgmis->event_metrics_sets[fp16_info[event_info_set]];

		int tensor_info[2];
		dcgmi_lib_get_event_info(tensor_active, &tensor_info);
		gpuprof_t *dcgmis_data_ten  = dcgmis->event_metrics_sets[tensor_info[event_info_set]];
    
    int dram_info[2];
    dcgmi_lib_get_event_info(dram_active, &dram_info);
    gpuprof_t *dcgmis_data_dram = dcgmis->event_metrics_sets[dram_info[event_info_set]];

    if (!dcgmis_data_64 || !dcgmis_data_32 || !dcgmis_data_16
        || !dcgmis_data_ten || !dcgmis_data_dram)
      return EAR_ERROR;

    double ctime = signature->time;
    double fp16 = 0;
    double fp32 = 0;
    double fp64 = 0;
    double tensor = 0;
    double dram_act = 0;
    
    for (uint g = 0; g < num_gpus; g++)
    {
      fp16 = fp16 + dcgmis_data_16[g].values[fp16_info[event_info_pos]]/num_gpus;
      fp32 = fp32 + dcgmis_data_32[g].values[fp32_info[event_info_pos]]/num_gpus;
      fp64 = fp64 + dcgmis_data_64[g].values[fp64_info[event_info_pos]]/num_gpus;
      tensor = tensor + dcgmis_data_ten[g].values[tensor_info[event_info_pos]]/num_gpus;
      dram_act = dram_act + dcgmis_data_dram[g].values[dram_info[event_info_pos]]/num_gpus;
    }
    double fp_act = fp16 + fp32 + fp64 + tensor;
    if(fp_act == 0 && dram_act == 0){
      proj_time[0] = ctime;
    }else{
      proj_time[0] = project_time(coeff, ctime, fp_act, dram_act, coeff->pstate);
    }
		}
	} else st = EAR_ERROR;

	return st;
}


state_t energy_model_project_power(signature_t *signature, ulong from_ps, ulong to_ps, double *proj_power)
{
	state_t st = EAR_SUCCESS;
	coefficient_gpu_t *coeff;

	assert(proj_power != NULL);

	if (proj_power == NULL) return EAR_ERROR;

	memset(proj_power, 0, sizeof(double) * num_gpus);

	if (from_ps == to_ps)
	{
		for (uint g = 0; g < num_gpus; g++) proj_power[g] = signature->gpu_sig.gpu_data[g].GPU_power;
		return st;
	}

	if (gpu_model_init && valid_range(from_ps, to_ps))
	{
		coeff = &coefficients[from_ps][to_ps];
		if (coeff->available)
		{
      //debug("Power proj: ffrom %lu fto %lu", coeff->pstate_ref, coeff->pstate);
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
			dcgmi_lib_get_event_info(fp64_active, &fp64_info);
			gpuprof_t *dcgmis_data_64 = dcgmis->event_metrics_sets[fp64_info[event_info_set]];

			int fp32_info[2];
			dcgmi_lib_get_event_info(fp32_active, &fp32_info);
			gpuprof_t *dcgmis_data_32 = dcgmis->event_metrics_sets[fp32_info[event_info_set]];

			int fp16_info[2];
			dcgmi_lib_get_event_info(fp16_active, &fp16_info);
			gpuprof_t *dcgmis_data_16   = dcgmis->event_metrics_sets[fp16_info[event_info_set]];

			int tensor_info[2];
			dcgmi_lib_get_event_info(tensor_active, &tensor_info);
			gpuprof_t *dcgmis_data_ten  = dcgmis->event_metrics_sets[tensor_info[event_info_set]];

			int dram_info[2];
			dcgmi_lib_get_event_info(dram_active, &dram_info);
			gpuprof_t *dcgmis_data_dram = dcgmis->event_metrics_sets[dram_info[event_info_set]];

			if (!dcgmis_data_64 || !dcgmis_data_32 || !dcgmis_data_16
					|| !dcgmis_data_ten || !dcgmis_data_dram)
				return EAR_ERROR;

       double cpower;
       double fp16;
       double fp32;
       double fp64;
       double tensor;
       double fp_act;
       double dram_act;

			for (uint g = 0; g < num_gpus; g++)
			{
				//debug("Going to project the power for the GPU %d...", g);
				cpower = signature->gpu_sig.gpu_data[g].GPU_power;
				fp16 = dcgmis_data_16[g].values[fp16_info[event_info_pos]];
				fp32 = dcgmis_data_32[g].values[fp32_info[event_info_pos]];
				fp64 = dcgmis_data_64[g].values[fp64_info[event_info_pos]];
				tensor = dcgmis_data_ten[g].values[tensor_info[event_info_pos]];
				dram_act = dcgmis_data_dram[g].values[dram_info[event_info_pos]];
        fp_act = fp16 + fp32 + fp64 + tensor;

        if(fp_act == 0 && dram_act ==0){
          proj_power[g] =  cpower;
        }else{
				  proj_power[g] = project_power(coeff, cpower, fp_act, dram_act, coeff->pstate);
        }
			}
		} else
		{
			debug("Coeffs from %lu to %lu not available",from_ps, to_ps);
			st = EAR_ERROR;
		}
	} else st = EAR_ERROR;

	return st;
}


uint energy_model_projection_available(ulong from_ps, ulong to_ps)
{
  //debug("gpu_model : from %lu to %lu", from_ps, to_ps);
	return projection_available(from_ps, to_ps);
}


uint energy_model_all_projections_available()
{
	uint not_avail = 0;

	//debug("Checking availability for %u p-states", num_gpu_pstates);
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
  //debug("valid_range: from_ps %lu to_ps %lu num_gpu_pstates %d",  from_ps, to_ps, num_gpu_pstates);
	if ((from_ps < num_gpu_pstates) && (to_ps < num_gpu_pstates)) return 1;
	else return 0;
}


static double project_time(coefficient_gpu_t *coeff, double ctime, double fp_act, double dram_act, ulong to_freq)
{
  double proj_time;

  proj_time = ctime*(coeff->B0 + coeff->B1*fp_act/100 + coeff->B2*dram_act/100 + coeff->B3*to_freq/1000000.0); 
	
	debug(">time_proj %lf time_ref %lf fp_act %lf dram %lf to_freq %lf", proj_time, ctime, fp_act, dram_act, to_freq/1000000.0);

  return proj_time;
}


static double project_power(coefficient_gpu_t *coeff, double cpower, double fp_act, double dram_act, ulong to_freq)
{
  double proj_power;
  
  proj_power = coeff->A0*cpower + coeff->A1*fp_act/100 + coeff->A2*dram_act/100 + coeff->A3*to_freq/1000000.0; 

  debug(">power_proj %lf power_ref %lf fp_act %lf dram %lf to_freq %lf", proj_power, cpower, fp_act, dram_act, to_freq/1000000.0);

  return proj_power;
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
