/*
 * Please, don't add this file to libcommon because clock_gettime() requires
 * -lrt on systems with old libc.
 *
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */

#define _GNU_SOURCE
#include <time.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

#include "monotonic.h"

int get_boot_time(struct timeval *boot_time)
{
	struct timespec hires_uptime;
	struct timeval lores_uptime;
	struct timeval now;
	struct sysinfo info;

	if (gettimeofday(&now, NULL) != 0)
		return -errno;
#ifdef CLOCK_BOOTTIME
	if (clock_gettime(CLOCK_BOOTTIME, &hires_uptime) == 0) {
		TIMESPEC_TO_TIMEVAL(&lores_uptime, &hires_uptime);
		timersub(&now, &lores_uptime, boot_time);
		return 0;
	}
#endif
	/* fallback */
	if (sysinfo(&info) != 0)
		return -errno;

	boot_time->tv_sec = now.tv_sec - info.uptime;
	boot_time->tv_usec = 0;
	return 0;
}

time_t get_suspended_time(void)
{
#if defined(CLOCK_BOOTTIME) && defined(CLOCK_MONOTONIC)
	struct timespec boot, mono;

	if (clock_gettime(CLOCK_BOOTTIME, &boot) == 0 &&
	    clock_gettime(CLOCK_MONOTONIC, &mono) == 0)
		return boot.tv_sec - mono.tv_sec;
#endif
	return 0;
}

int gettime_monotonic(struct timeval *tv)
{
#ifdef CLOCK_MONOTONIC
	/* Can slew only by ntp and adjtime */
	int ret;
	struct timespec ts;

	/* Linux specific, can't slew */
	if (!(ret = clock_gettime(UL_CLOCK_MONOTONIC, &ts))) {
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
	}
	return ret;
#else
	return gettimeofday(tv, NULL);
#endif
}


