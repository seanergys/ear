/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/
	if (ear_connect() != EAR_SUCCESS)
 	{
 		printf("error connecting eard\n");
		exit(1);
 	}
	/* Reading energy */
	if (ear_energy(&e_mj_init,&t_ms_init)!=EAR_SUCCESS)
 	{
 		printf("Error in ear_energy\n");
 	}
	while(i<5)
 	{
 		sleep(5);
		/* READING ENERGY */
		if (ear_energy(&e_mj_end,&t_ms_end)!=EAR_SUCCESS)
 		{
 			printf("Error in ear_energy\n");
 		}
		else
 		{
 			ts=t_ms_init/1000;
 			ts_e=t_ms_end/1000;
 			tstamp=localtime((time_t *)&ts);
 			strftime(s, sizeof(s), "%c", tstamp);
 			tstamp2=localtime((time_t *)&ts_e);
 			strftime(s2, sizeof(s), "%c", tstamp2);
 			printf("Start time %s End time %s\n",s,s2);
 			ear_energy_diff(e_mj_init,e_mj_end, &e_mj, t_ms_init,t_ms_end,&t_ms); 
			printf("Time consumed %lu (ms), energy consumed %lu(mJ), \
 			Avg power %lf(W)\n",t_ms,e_mj,(double)e_mj/(double)t_ms);
 			e_mj_init=e_mj_end;
 			t_ms_init=t_ms_end;
 		}
 		i++;
 	}
 	ear_disconnect();
}

