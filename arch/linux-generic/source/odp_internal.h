/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP HW system information
 */

#ifndef ODP_INTERNAL_H_
#define ODP_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif



int odp_system_info_init(void);


void odp_thread_init_global(void);
void odp_thread_init_local(int thr_id);


int odp_shm_init_global(void);
int odp_shm_init_local(void);


int odp_buffer_pool_init_global(void);

int odp_pktio_init_global(void);
int odp_pktio_init_local(void);

int odp_queue_init_global(void);


int odp_schedule_init_global(void);


#ifdef __cplusplus
}
#endif

#endif







