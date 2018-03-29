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

#ifndef __TRUSTZONE_TA_PLAYREADY__
#define __TRUSTZONE_TA_PLAYREADY__


#define TZ_TA_PLAYREADY_UUID         "b25bf100-d276-11e2-8b8b-0800200c9a66"

#define PLAYREADY_PROVISIONED_CERT  1
#define PLAYREADY_PROVISIONED_KEY 2
#define PLAYREADY_PROVISIONED_CLEAR_KEY  3  /* for debug */

#define TZ_DRM_UI64EQL   1
#define TZ_DRM_UI64LES    2

#define TZ_TOKEN_TOKEN    1
#define TZ_TOKEN_VALUE     2
#define TZ_VALUE_TOKEN     3

#define SUPPORT_MULTIPLE_INSTANCE 1

#define C_SECONDS_IN_ROLLBACK_GRACE_PERIOD 30

/* Data Structure for Playready TA */
/* You should define data structure used both in REE/TEE here
     N/A for Playready TA */

/* Command for Playready TA */
#define TZCMD_PLAYREADY_BASE                             0x00000000
#define TZCMD_PLAYREADY_PROVISIONED_DATA_GET             0x00000001
#define TZCMD_PLAYREADY_ECCP256_KEYPAIR_GEN              0x00000002
#define TZCMD_PLAYREADY_ECCP256_KEY_SET                  0x00000003
#define TZCMD_PLAYREADY_ECDSAP256_SIGN                   0x00000004
#define TZCMD_PLAYREADY_ECCP256_DECRYPT                  0x00000005
#define TZCMD_PLAYREADY_OMAC1_KEY_SET                    0x00000006
#define TZCMD_PLAYREADY_OMAC1_VERIFY                     0x00000007
#define TZCMD_PLAYREADY_OMAC1_SIGN                       0x00000008
#define TZCMD_PLAYREADY_COPYBYTE                         0x00000009
#define TZCMD_PLAYREADY_CONTENTKEY_AESCTR_SET            0x0000000a
#define TZCMD_PLAYREADY_CONTENT_AESCTR_DECRYPT           0x0000000b
#define TZCMD_PLAYREADY_AESECB_KEY_SET                   0x0000000c
#define TZCMD_PLAYREADY_AESECB_ENCRYPT                   0x0000000d
#define TZCMD_PLAYREADY_AESECB_DECRYPT                   0x0000000e
#define TZCMD_PLAYREADY_GET_KFKEY                        0x0000000f
#define TZCMD_PLAYREADY_AESCBC_KEY_SET                   0x00000010
#define TZCMD_PLAYREADY_AESCBC_ENCRYPT                   0x00000011
#define TZCMD_PLAYREADY_AESCBC_DECRYPT                   0x00000012
#define TZCMD_PLAYREADY_HANDLE_CONTENT_AESCTR_DECRYPT    0x00000013
#define TZCMD_PLAYREADY_KEYFILE_DECRYPT                  0x00000014
#define TZCMD_PLAYREADY_KEYFILE_ENCRYPT                  0x00000015
#define TZCMD_PLAYREADY_TOKENTIME_COMPARE          0x00000016
#define TZCMD_PLAYREADY_TOKENTIME_UPDATE                 0x00000017
#define TZCMD_PLAYREADY_MACHINEDATETIME_CHECK            0x00000019
#define TZCMD_PLAYREADY_GET_HWID            0x00000020

/* SUPPORT_PLAYREADY_SECURE_CLOCK*/
#define TZ_SECURETIME_BIRTHDATE 1392967151
/* used for getting encrypted prtime struct to save file when shutdown and
     suspend or after THREAD_SAVEFILE_VALUE second*/
#define TZCMD_PLAYREADY_GET_CURRENT_COUNTER            0x00000030
/*  used for set new playready time using the current rtc counter and
      encrypted saved prtime struct when resume and bootup*/
#define TZCMD_PLAYREADY_SET_CURRENT_COUNTER            0x00000031
/*  used for increase current counter at least PR_TIME_INC_COUNTER secs
      and no more than PR_TIME_MAX_COUNTER_OFFSET secs*/
#define TZCMD_PLAYREADY_INC_CURRENT_COUNTER            0x00000032
/* used for network time-sync module to sync pr_time*/
#define TZCMD_PLAYREADY_SET_CURRENT_PRTIME            0x00000033
#define PR_TIME_INC_COUNTER 10
#define PR_TIME_MAX_COUNTER_OFFSET 15

#define PR_TIME_FILE_BASE        50000
#define PR_TIME_FILE_ERROR_SIGN  (PR_TIME_FILE_BASE + 1)
#define PR_TIME_FILE_OK_SIGN   (PR_TIME_FILE_BASE + 2)
#define NO_SECURETD_FILE        (PR_TIME_FILE_BASE + 3)
#define PR_TIME_ERROR_SETPRTIME   (PR_TIME_FILE_BASE + 4)

struct TZ_JG_SECURECLOCK_INFO {
unsigned long long hwcounter;
unsigned long long pr_time;
char signature[8];
};

struct TM_PR {
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

/*end of SUPPORT_PLAYREADY_SECURE_CLOCK*/

struct PLAYREADY_IVDATA {
unsigned long long qwInitializationVector;
unsigned long long qwBlockOffset;
unsigned long  bByteOffset;
};


struct TZ_PLAYREADY_ENCINFO {
char                                        role[100];
/*total enc buffer size */
unsigned int                         dataSize;
/* trunk number */
unsigned int                         segNum;
/* IV data for each trunk */
struct PLAYREADY_IVDATA          iv[10];
/* pointer to an integer array, each element describe clear data size*/
unsigned int                         offset[10];
/* pointer to an integer array, each element describe enc data size */
unsigned int                         length[10];
/* true : dstData is a handle; false : dstData is a buffer;  */
unsigned int                      dstHandle;
};

#ifdef SUPPORT_MULTIPLE_INSTANCE

#define DRM_AES_KEYSIZE_128 (16) /* Size ( in bytes ) of a 128 bit key */

/* Now at least two or more process will use ta_playready.c at same time,
   drm server and media server */
#define MAX_AESECB_KEYS_INSTANCE   4
#define MAX_OMAC1_KEYS_INSTANCE   4

struct TZ_PLAYREADY_AESECB_KEYS {
uint32_t handle;   /* tee session handle  */
char aesecbKey[DRM_AES_KEYSIZE_128];
uint32_t bProtect;
uint32_t bInUse;
};

struct TZ_PLAYREADY_OMAC1_KEYS {
uint32_t handle;   /* tee session handle */
char omac1Key[DRM_AES_KEYSIZE_128];
uint32_t bProtect;
uint32_t bInUse;
};

#endif

#endif /* __TRUSTZONE_TA_PLAYREADY__ */

