/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */



#ifndef ODP_SCHEDULE_INTERNAL_H_
#define ODP_SCHEDULE_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <odp_queue.h>


void odp_schedule_queue(odp_queue_t queue, int prio);



#ifdef __cplusplus
}
#endif

#endif


