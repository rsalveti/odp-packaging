/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of Linaro Limited nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIALDAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *
 * The OpenDataPlane API
 *
 */

/**
 * @mainpage
 *
 * @section sec_1 Introduction
 *
 * OpenDataPlane (ODP) provides a data plane application programming
 * environment that is easy to use, high performance and portable between
 * networking SoCs.
 *
 * @section sec_2 User guide
 *
 * @subsection sub2_1 The ODP API
 *
 * This file (odp.h) is the main ODP API file. User should include only this
 * file to keep portability since structure and naming of sub header files
 * may be change between implementations.
 *
 * @subsection sub2_2 Threading
 *
 * User can use processes or pthreads for multi-threading. Creation and
 * control of the threads is on user's responsibility. However, there
 * are helper API functions e.g. for managing pthreads (odp_linux.h).
 *
 * Threads used for ODP processing should be pinned into separate cores.
 * Commonly these threads process packets in a run-to-completion loop.
 * Application should avoid blocking threads used for ODP processing,
 * since it may cause blocking on other threads/cores.
 *
 * @subsection sub2_3 ODP initialisation
 *
 * Before calling any other ODP API functions, ODP library must be initialised
 * by calling odp_init_global() once and odp_init_local() on each of the cores
 * sharing the same ODP environment (instance).
 *
 */

#ifndef ODP_H_
#define ODP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_version.h>
#include <odp_std_types.h>

#include <odp_align.h>
#include <odp_hints.h>
#include <odp_debug.h>
#include <odp_packet.h>
#include <odp_coremask.h>
#include <odp_barrier.h>

#include <odp_atomic.h>
#include <odp_init.h>
#include <odp_system_info.h>
#include <odp_thread.h>



#ifdef __cplusplus
}
#endif
#endif
