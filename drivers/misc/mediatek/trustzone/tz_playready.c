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

#include "tz_playready.h"

#ifdef TZ_PLAYREADY_SECURETIME_SUPPORT

uint32_t TEE_update_pr_time_intee(KREE_SESSION_HANDLE session)
{
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret = TZ_RESULT_SUCCESS;
uint32_t file_result = PR_TIME_FILE_OK_SIGN;
struct file *file = NULL;
UINT64 u8Offset = 0;

int err = -ENODEV;
struct rtc_time tm;
DRM_UINT64 time_count64;
unsigned long time_count;
struct rtc_device *rtc;

struct TZ_JG_SECURECLOCK_INFO secure_time;
DRM_UINT64 cur_counter;

rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

if (rtc == NULL) {
	pr_warn("%s: unable to open rtc device (%s)\n",
				__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
	goto err_open;
}

paramTypes = TZ_ParamTypes3(TZPT_MEM_INPUT, TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT);
param[0].mem.buffer = (void *) &secure_time;
param[0].mem.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
param[1].mem.buffer = (void *) &cur_counter;
param[1].mem.size = sizeof(DRM_UINT64);

file = FILE_Open(PR_TIME_FILE_SAVE_PATH, O_RDWR, S_IRWXU);
if (file) {
	FILE_Read(file, (void *) &secure_time,
			sizeof(struct TZ_JG_SECURECLOCK_INFO), &u8Offset);
	filp_close(file, NULL);
} else {
	file_result = NO_SECURETD_FILE;
	goto err_read;
}

err = rtc_read_time(rtc, &tm);
if (err) {
	dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
	goto err_read;
}

err = rtc_valid_tm(&tm);
if (err) {
	dev_err(rtc->dev.parent, "hctosys: invalid date/time\n");
	goto err_invalid;
}

rtc_tm_to_time(&tm, &time_count);
time_count64 = (DRM_UINT64)time_count;
memcpy((char *) &cur_counter, (char *) &time_count64, sizeof(DRM_UINT64));

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_SET_CURRENT_COUNTER,
				paramTypes, param);
if (ret != TZ_RESULT_SUCCESS)
	pr_warn("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER error %d\n",
		ret);

if (param[2].value.a == PR_TIME_FILE_ERROR_SIGN) {
	file_result = PR_TIME_FILE_ERROR_SIGN;
	pr_warn("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER file_result %d\n",
		file_result);
} else if (param[2].value.a == PR_TIME_FILE_OK_SIGN) {
	file_result = PR_TIME_FILE_OK_SIGN;
	pr_warn("ServiceCall TZCMD_PLAYREADY_SET_CURRENT_COUNTER file_result %d\n",
		file_result);
}

err_invalid:
err_read:
rtc_class_close(rtc);
err_open:
if (file_result == PR_TIME_FILE_OK_SIGN)
	return ret;
else
	return file_result;

}

uint32_t TEE_update_pr_time_infile(KREE_SESSION_HANDLE session)
{
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret = TZ_RESULT_SUCCESS;
struct file *file = NULL;
UINT64 u8Offset = 0;

struct TZ_JG_SECURECLOCK_INFO secure_time;

paramTypes = TZ_ParamTypes3(TZPT_MEM_OUTPUT, TZPT_VALUE_INPUT,
				TZPT_VALUE_OUTPUT);
param[0].mem.buffer = (void *) &secure_time;
param[0].mem.size = sizeof(struct TZ_JG_SECURECLOCK_INFO);
param[1].value.a = 0;

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_GET_CURRENT_COUNTER,
					paramTypes, param);
if (ret != TZ_RESULT_SUCCESS) {
	pr_warn("ServiceCall error %d\n", ret);
	goto tz_error;
}

file = FILE_Open(PR_TIME_FILE_SAVE_PATH, O_RDWR|O_CREAT, S_IRWXU);
if (file) {
	FILE_Write(file, (void *) &secure_time,
			sizeof(struct TZ_JG_SECURECLOCK_INFO), &u8Offset);
	filp_close(file, NULL);
} else {
	pr_warn("FILE_Open PR_TIME_FILE_SAVE_PATH return NULL\n");
}

tz_error:

return ret;

}



uint32_t TEE_Icnt_pr_time(KREE_SESSION_HANDLE session)
{
MTEEC_PARAM param[4];
uint32_t paramTypes;
TZ_RESULT ret = TZ_RESULT_SUCCESS;
unsigned long time_count = 1392967151;

struct rtc_device *rtc = NULL;
struct rtc_time tm;
int err = -ENODEV;

struct TM_PR secure_TM;

paramTypes = TZ_ParamTypes2(TZPT_VALUE_INPUT, TZPT_MEM_OUTPUT);
param[1].mem.buffer = (void *) &secure_TM;
param[1].mem.size = sizeof(struct TM_PR);
rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

if (rtc == NULL) {
	pr_warn("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
	goto err_open;
}

err = rtc_read_time(rtc, &tm);
if (err) {
	dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");
	goto err_read;

}

err = rtc_valid_tm(&tm);
if (err) {
	dev_err(rtc->dev.parent,
			"hctosys: invalid date/time\n");
	goto err_invalid;
}

rtc_tm_to_time(&tm, &time_count);
#if 0
pr_debug("securetime increase result: %d %d %d %d %d %d %d\n",
	tm.tm_yday, tm.tm_year, tm.tm_mon, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif
param[0].value.a = time_count;

ret = KREE_TeeServiceCall(session, TZCMD_PLAYREADY_INC_CURRENT_COUNTER,
				paramTypes, param);
if (ret != TZ_RESULT_SUCCESS)
	pr_warn("ServiceCall error %d\n", ret);

#if 0
pr_debug("securetime increase result: %d %d %d %d %d %d %d\n", secure_TM.tm_yday
	, secure_TM.tm_year, secure_TM.tm_mon, secure_TM.tm_mday
	, secure_TM.tm_hour, secure_TM.tm_min, secure_TM.tm_sec);
#endif

err_read:
err_invalid:
rtc_class_close(rtc);
err_open:

return ret;
}

#define THREAD_COUNT_FREQ 10
#define THREAD_SAVEFILE_VALUE (1*60*60)
static int check_count;
static KREE_SESSION_HANDLE icnt_session;

int update_securetime_thread(void *data)
{
TZ_RESULT ret;
unsigned int update_ret = 0;
uint32_t nsec = THREAD_COUNT_FREQ;

ret = KREE_CreateSession(TZ_TA_PLAYREADY_UUID, &icnt_session);
if (ret != TZ_RESULT_SUCCESS) {
	pr_warn("CreateSession error %d\n", ret);
	return 1;
}

set_freezable();

schedule_timeout_interruptible(HZ * nsec);

update_ret = TEE_update_pr_time_intee(icnt_session);
if (update_ret == NO_SECURETD_FILE || update_ret == PR_TIME_FILE_ERROR_SIGN) {
	TEE_update_pr_time_infile(icnt_session);
	TEE_update_pr_time_intee(icnt_session);
}

for (;;) {
	if (kthread_should_stop())
		break;
	if (try_to_freeze())
		continue;
	schedule_timeout_interruptible(HZ * nsec);
	if (icnt_session) {
		TEE_Icnt_pr_time(icnt_session);
		check_count += THREAD_COUNT_FREQ;
	if ((check_count < 0) || (check_count > THREAD_SAVEFILE_VALUE)) {
		TEE_update_pr_time_infile(icnt_session);
		check_count = 0;
	}
}
}

ret = KREE_CloseSession(icnt_session);
if (ret != TZ_RESULT_SUCCESS)
	pr_warn("CloseSession error %d\n", ret);

return 0;
}
#endif

