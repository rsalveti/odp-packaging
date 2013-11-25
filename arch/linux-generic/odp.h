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
 * environment that is easy to use, high performance, and portable
 * between networking SoCs. This documentation is both a user guide
 * for developers who wish to use ODP and a detailed reference for ODP
 * programmers covering APIs, data structures, files, etc.
 *
 * ODP consists of a common layer and an implementation layer.
 * Applications written to the common layer are portable across all
 * ODP implementations.  To compile and run an ODP application, it is
 * compiled against a specific ODP implementation layer.  The purpose
 * of the implementation layer is to provide an optimal mapping of ODP
 * APIs to the underlying capabilities (including hardware
 * co-processing and acceleration support) of of SoCs hosting ODP
 * implementations.  As a bootstrapping mechanism for applications, as
 * well as to provide a model for ODP implementers, ODP provides a
 * 'linux-generic' reference implementation designed to run on any SoC
 * which has a Linux kernel.  While linux-generic is not a performance
 * target, it does provide a starting point for ODP implementers and
 * application programmers alike.
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
 * ODP does not specify a threading model.  Applications can use
 * processes or pthreads, or Roll-Your-Own (RYO) threading/fibre
 * mechanisms for multi-threading as needed. Creation and control of
 * threads is the responsibility of the ODP application. For optimal
 * performance on many-core SoCs, it is recommended that threads be
 * run on dedicated cores. ODP provides high-level APIs for core
 * enumeration and assignment while the corresponding ODP
 * implementation layer provides the appropriate mechanisms to realize
 * these functions.
 *
 * Threads used for ODP processing should be pinned into separate cores.
 * Commonly these threads process packets in a run-to-completion loop.
 * Application should avoid blocking threads used for ODP processing,
 * since it may cause blocking on other threads/cores.
 *
 * @subsection sub2_3 ODP initialisation
 *
 * Before calling any other ODP API functions, ODP library must be
 * initialised by calling odp_init_global() once and odp_init_local()
 * on each of the cores sharing the same ODP environment (instance).
 *
 * @subsection sub2_4 API Categories
 *
 * APIs provided by ODP cover the following areas:
 *
 *   - Memory Management\n\n
 *   This includes macros and other APIs to control memory alignments
 *   of data structures as well as allocation/deallocation services
 *   for ODP-managed objects.  Note that ODP does not wrapper malloc()
 *   or similar platform specific APIs for the sake of wrappering.\n\n
 *
 *   - Packet Management\n\n
 *   This includes APIs and accessor functions for packet descriptors
 *   as well as packet receipt and transmission.\n\n
 *
 *   - Synchronization\n\n
 *   This includes APIs and related functions for synchronization
 *   involving other ODP APIs, such as barriers and related atomics.
 *   Again, as ODP does not specify a threading model applications
 *   make use whatever synchronization primitives are native to the
 *   model they use.\n\n
 *
 *   - Core Enumeration and managment\n\n
 *   This includes APIs to allow applications to enumerate and
 *   reference cores and per-core data structures.\n\n
 *
 *   - Add others here as they are defined...
 *
 * @subsection sub2_5 Miscellaneous Facilities
 *
 * ODP includes miscellaneous facilities for compiler hints and
 * optimizations common in GCC.  [Not sure if we want to consider
 * these an "API" per se].
 *
 * @subsection sub2_6 Application Programming Model
 *
 * ODP supports applications that execute using a "run to completion"
 * programming model.  This means that once dispatched, application
 * threads are not interrupted by the kernel or other scheduling
 * entity.
 *
 * Application threads receive work requests as \a events that are
 * delivered on application and/or implementation defined
 * \a queues.  ODP application code would thus normally be
 * structured as follows:
 *
 * ~~~{.c}
 * #include <odp.h>
 * ...other needed #includes
 *
 * int main (int argc, char *argv[])
 * {
 *         ...application-specific initialization
 *         odp_init_global();
 *
 *         ...launch threads
 *         ...wait for threads to terminate
 * }
 *
 * void worker_thread (parameters)
 * {
 *         odp_init_local();
 *
 *         while (1) {
 *             do_work(get_work());  // Replace with ODP calls when defined
 *         }
 *
 * }
 * ~~~
 *
 * Events are receved on input queues and are processed until they are
 * placed on an output queue of some sort.  The thread then gets the
 * next event to be processed from an input queue and repeats the
 * process.
 *
 * @subsection sub3_1 Asynchronous Operations
 *
 * Note that work to be performed by a thread may require access to an
 * asynchronous function that takes a significant amount of time to
 * complete.  In such cases the event is forwarded to another worker
 * thread or hardware accelerator, depending on the implementation, by
 * placing it on anothert queue, which is an output queue of the
 * thread making the request. This event in turn is received and
 * processed by the thread/accelerator that handles it via its input
 * queue.  When this aysynchronous event is complete, the event is
 * placed on the handler's output queue, which feeds back to the
 * original requestor's input queue.  When the requesting thread next
 * receives this event it resumes processing of the event following
 * the asynchronous event and works on it either until it is ready for
 * final disposition, or until another asynchronous operation is
 * required to process the event.
 *
 * @subsection sub3_2 Queue Linkages
 *
 * The mapping of input and output queues that connect worker threads
 * to accelerators and related offload functions is a cooperation
 * between the implementation and the ODP application.  The
 * implementation defines the service funtions that are available to
 * worker threads (e.g., cypto offload services) and as part of that
 * definition defines the queue structure that connects requests to
 * those services as well as the outputs from those services that
 * connect back to the requesting workers.  The ODP application, in
 * turn, defines the number of worker threads and how they cooperate
 * among themselves.  Note that the application may use ODP core
 * enumeration APIs to decide how many such worker threads should be
 * deployed.
 */

#ifndef ODP_H_
#define ODP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_config.h>

#include <odp_version.h>
#include <odp_std_types.h>

#include <odp_align.h>
#include <odp_hints.h>
#include <odp_debug.h>
#include <odp_packet.h>
#include <odp_coremask.h>
#include <odp_barrier.h>
#include <odp_spinlock.h>

#include <odp_atomic.h>
#include <odp_init.h>
#include <odp_system_info.h>
#include <odp_thread.h>
#include <odp_shared_memory.h>


#ifdef __cplusplus
}
#endif
#endif
