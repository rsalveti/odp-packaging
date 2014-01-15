/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_system_info.h>
#include <odp_internal.h>
#include <odp_debug.h>
#include <odp_align.h>
#include <stdio.h>
#include <string.h>



typedef struct {
	uint64_t cpu_hz;
	int      cache_line_size;
	int      core_count;
	char     model_str[128];

} odp_system_info_t;


typedef struct {
	const char *cpu_arch_str;
	int (*cpuinfo_parser)(FILE *file, odp_system_info_t *sysinfo);

} odp_compiler_info_t;


static odp_system_info_t odp_system_info;


/*
 * HW specific /proc/cpuinfo file parsing
 */

#if defined __x86_64__ || defined __i386__

static int cpuinfo_x86(FILE *file, odp_system_info_t *sysinfo)
{
	char str[1024];
	char *pos;
	double mhz = 0.0;
	int model = 0;
	int count = 2;

	while (fgets(str, sizeof(str), file) != NULL && count > 0) {
		if (!mhz) {
			pos = strstr(str, "cpu MHz");

			if (pos) {
				sscanf(pos, "cpu MHz : %lf", &mhz);
				count--;
			}
		}

		if (!model) {
			pos = strstr(str, "model name");

			if (pos) {
				int len;
				pos = strchr(str, ':');
				strncpy(sysinfo->model_str, pos+2,
					sizeof(sysinfo->model_str));
				len = strlen(sysinfo->model_str);
				sysinfo->model_str[len - 1] = 0;
				model = 1;
				count--;
			}
		}
	}

	sysinfo->cpu_hz = (uint64_t) (mhz * 1000000.0);

	return 0;
}


#elif defined __arm__

static int cpuinfo_arm(FILE *file ODP_UNUSED,
odp_system_info_t *sysinfo ODP_UNUSED)
{
	return 0;
}

#elif defined __OCTEON__

static int cpuinfo_octeon(FILE *file, odp_system_info_t *sysinfo)
{
	char str[1024];
	char *pos;
	double mhz = 0.0;
	int model = 0;
	int count = 2;

	while (fgets(str, sizeof(str), file) != NULL && count > 0) {
		if (!mhz) {
			pos = strstr(str, "BogoMIPS");

			if (pos) {
				sscanf(pos, "BogoMIPS : %lf", &mhz);
				count--;
			}
		}

		if (!model) {
			pos = strstr(str, "cpu model");

			if (pos) {
				int len;
				pos = strchr(str, ':');
				strncpy(sysinfo->model_str, pos+2,
					sizeof(sysinfo->model_str));
				len = strlen(sysinfo->model_str);
				sysinfo->model_str[len - 1] = 0;
				model = 1;
				count--;
			}
		}
	}

	/* bogomips seems to be 2x freq */
	sysinfo->cpu_hz = (uint64_t) (mhz * 1000000.0 / 2.0);

	return 0;
}

#else
	#error GCC target not found
#endif


static odp_compiler_info_t compiler_info = {
#ifdef __GNUC__

	#if defined __x86_64__ || defined __i386__
	.cpu_arch_str = "x86",
	.cpuinfo_parser = cpuinfo_x86

	#elif defined __arm__
	.cpu_arch_str = "arm",
	.cpuinfo_parser = cpuinfo_arm

	#elif defined __OCTEON__
	.cpu_arch_str = "octeon",
	.cpuinfo_parser = cpuinfo_octeon

	#else
		#error GCC target not found
	#endif
#else
	#error Compiler is not GCC
#endif
};



#if defined __x86_64__ || defined __i386__

#define FILE0 "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size"

/*
 * Analysis of /sys/devices/system/cpu/ files
 */
static int systemcpu(odp_system_info_t *sysinfo)
{
	FILE  *file;
	char str[128];
	int size = 0;
	char cpu_str[128];
	int cpu = 0;

	file = fopen(FILE0, "rt");

	if (file == NULL) {
		/* File not found */
		return -1;
	}

	if (fgets(str, sizeof(str), file) != NULL) {
		/* Read cache line size */
		sscanf(str, "%i", &size);
	}

	fclose(file);

	sysinfo->cache_line_size = size;

	if (size != ODP_CACHE_LINE_SIZE) {
		printf("WARNING: Cache line sizes definitions don't match.\n");
		printf("  odp_sys_cache_line_size %i\n", size);
		printf("  ODP_CACHE_LINE_SIZE     %i\n\n", ODP_CACHE_LINE_SIZE);
	}


	sprintf(cpu_str, "/sys/devices/system/cpu/cpu%i", cpu);

	while ((file = fopen(cpu_str, "rt")) != NULL) {
		fclose(file);
		sysinfo->core_count++;
		cpu++;
		sprintf(cpu_str, "/sys/devices/system/cpu/cpu%i", cpu);
	}

	return 0;
}

#else

/*
 * Use sysconf and dummy values in generic case
 */

#include <unistd.h>
#include <sys/sysinfo.h>

static int systemcpu(odp_system_info_t *sysinfo)
{
	long ret;

	ret = sysconf(_SC_NPROCESSORS_CONF);

	if (ret < 0)
		return -1;

	sysinfo->core_count = ret;


	/* Dummy values */
	sysinfo->cpu_hz          = 1400000000;
	sysinfo->cache_line_size = 64;

	strncpy(sysinfo->model_str, "UNKNOWN", sizeof(sysinfo->model_str));

	return 0;
}

#endif

/*
 * System info initialisation
 */
int odp_system_info_init(void)
{
	FILE  *file;

	memset(&odp_system_info, 0, sizeof(odp_system_info_t));

	file = fopen("/proc/cpuinfo", "rt");

	if (file == NULL) {
		/* File not found */
		return -1;
	}

	compiler_info.cpuinfo_parser(file, &odp_system_info);

	fclose(file);

	systemcpu(&odp_system_info);

	return 0;
}



/*
 *************************
 * Public access functions
 *************************
 */

uint64_t odp_sys_cpu_hz(void)
{
	return odp_system_info.cpu_hz;
}

const char *odp_sys_cpu_model_str(void)
{
	return odp_system_info.model_str;
}

int odp_sys_cache_line_size(void)
{
	return odp_system_info.cache_line_size;
}

int odp_sys_core_count(void)
{
	return odp_system_info.core_count;
}




