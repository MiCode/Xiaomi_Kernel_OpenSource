/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TRUSTZONE_TA_SECURE_TIMER__
#define __TRUSTZONE_TA_SECURE_TIMER__


#define TZ_TA_SECURETIME_UUID         "b25bf100-d276-11e2-9c9c-9c9c9c9c9c9c"


#define uint64 unsigned long long

#define TZ_SECURETIME_BIRTHDATE 1392967151

/* used for getting encrypted prtime struct to save file when shutdown and
 * suspend or after THREAD_SAVEFILE_VALUE second
 */
#define TZCMD_SECURETIME_GET_CURRENT_COUNTER 0x00000000

/* used for set new playready time using the current rtc counter and
 * encrypted saved prtime struct when resume and bootup
 */
#define TZCMD_SECURETIME_SET_CURRENT_COUNTER 0x00000001

/* used for increase current counter at least PR_TIME_INC_COUNTER secs and
 * no more than PR_TIME_MAX_COUNTER_OFFSET secs
 */
#define TZCMD_SECURETIME_INC_CURRENT_COUNTER 0x00000002

/* used for network time-sync module to sync pr_time */
#define TZCMD_SECURETIME_SET_CURRENT_PRTIME 0x00000003
#define GB_TIME_INC_COUNTER 5
#define GB_TIME_MAX_COUNTER_OFFSET 8

#define GB_TIME_FILE_BASE        50000
#define GB_TIME_FILE_ERROR_SIGN  (GB_TIME_FILE_BASE + 1)
#define GB_TIME_FILE_OK_SIGN   (GB_TIME_FILE_BASE + 2)
#define GB_NO_SECURETD_FILE        (GB_TIME_FILE_BASE + 3)
#define GB_TIME_ERROR_SETPRTIME   (GB_TIME_FILE_BASE + 4)

#define DRM_UINT64 unsigned long long

struct TZ_GB_SECURETIME_INFO {
	unsigned long long hwcounter;
	unsigned long long gb_time;
	char signature[8];
};

struct TM_GB {
	int     tm_sec;         /* seconds */
	int     tm_min;         /* minutes */
	int     tm_hour;        /* hours */
	int     tm_mday;        /* day of the month */
	int     tm_mon;         /* month */
	int     tm_year;        /* year */
	int     tm_wday;        /* day of the week */
	int     tm_yday;        /* day in the year */
	int     tm_isdst;       /* daylight saving time */

	long int tm_gmtoff;     /* Seconds east of UTC.  */
	const char *tm_zone;    /* Timezone abbreviation.  */
};

/* end of SUPPORT_GB_SECURE_CLOCK */

struct SECURETIME_IVDATA {
	unsigned long long qwInitializationVector;
	unsigned long long qwBlockOffset;
	unsigned long  bByteOffset;
};


struct TZ_SECURETIME_ENCINFO {
	char role[100];
	unsigned int dataSize; /* total enc buffer size	*/
	unsigned int segNum; /* trunk number */
	struct SECURETIME_IVDATA iv[10]; /* IV data for each trunk */
	unsigned int offset[10]; /* pointer to an integer array,
				  * each element describes clear data size
				  */
	unsigned int length[10]; /* pointer to an integer array,
				  * each element describe enc data size
				  */
	unsigned int dstHandle;  /* true : dstData is a handle;
				  * false : dstData is a buffer;
				  */
};

/* only be userd in tee, in user or kernel, should call the tee_service call */
/* unsigned long long GetTee_SecureTime(); */


#endif /* __TRUSTZONE_TA_SECURE_TIMER__ */

