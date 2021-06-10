// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */
#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/gameport.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/timex.h>
#include <linux/types.h>

#define IMSG_TAG "[tz_test_ca]"
#include <imsg_log.h>
#include "ta_core_api.h"
unsigned int xtest_teec_open_session(struct TEEC_Context *context,
					struct TEEC_Session *session,
					const struct TEEC_UUID *uuid,
					struct TEEC_Operation *op,
					uint32_t *ret_orig)
{
	return TEEC_OpenSession(context, session, uuid, TEEC_LOGIN_PUBLIC,
					NULL, op, ret_orig);
}
const char *ADBG_GetFileBase(const char *const FileName_p)
{
	const char *Ch_p = FileName_p;
	const char *Base_p = FileName_p;

	while (*Ch_p != '\0') {
		if (*Ch_p == '\\')
			Base_p = Ch_p + 1;

		Ch_p++;
	}

	return Base_p;
}
int case_res;
bool Do_ADBG_Expect(const char *const FileName_p, const int LineNumber,
			const int Expected, const int Got)
{
	if (Expected == Got)
		return true;

	IMSG_WARN("%s:%d: unexpected value: 0x%x, expected 0x%x\n",
					ADBG_GetFileBase(FileName_p),
					LineNumber, Got, Expected);

	case_res = -EINVAL;
	return false;
}

struct TEEC_Context xtest_isee_ctx_1000;
const struct TEEC_UUID isee_test_ta_uuid = DRM_01_UUID;

int compare_array_with_pattern(unsigned char *buffer, unsigned int pattern,
				unsigned int size_in_byte)
{
	unsigned char *buffer_ptr = (unsigned char *)buffer;
	unsigned char *pattern_ptr = (unsigned char *)&pattern;
	unsigned int i;
	unsigned int *got;

	for (i = 0; i < size_in_byte; i += 4) {
		if (buffer_ptr[i] != pattern_ptr[0])
			goto exit;
		if (buffer_ptr[i + 1] != pattern_ptr[1])
			goto exit;
		if (buffer_ptr[i + 2] != pattern_ptr[2])
			goto exit;
		if (buffer_ptr[i + 3] != pattern_ptr[3])
			goto exit;
	}
	return TA_CORE_API_PARAM_TYPE_PATTERN_MATCH;
exit:
	got = (unsigned int *)(&buffer_ptr[i]);
	IMSG_DEBUG("[%s]expect=0x%x, got[%d]=0x%x\n", __func__,
				pattern, i, *got);
	return TA_CORE_API_PARAM_TYPE_PATTERN_NOT_MATCH;
}

int xtest_isee_test_1001(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PRINT, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1002(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_SPECIFIC_RET_VALUE,
			TEEC_InvokeCommand(&session, TA_CORE_API_CMD_RET_VALUE,
						NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1003(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_RESULT(TEE_ERROR_UNKNOWN_COMMAND,
		TEEC_InvokeCommand(&session, TA_CORE_API_UNSUPPORTED_CMD,
					NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1004(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	uint64_t time_start, time_end, time_freq, time_ms;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.params[0].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[0].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[1].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[1].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[2].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[2].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[3].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.params[3].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT);

	(void)ADBG_EXPECT_TEEC_SUCCESS(
		TEEC_InvokeCommand(&session, TA_CORE_API_CMD_READ_CNTVCT,
					&op, &ret_orig));

	time_start = (uint64_t)op.params[0].value.b << 32
					& 0xFFFFFFFF00000000lu;
	time_start |= op.params[0].value.a;
	time_end = (uint64_t)op.params[1].value.b << 32
					& 0xFFFFFFFF00000000lu;
	time_end |= op.params[1].value.a;
	time_freq = (uint64_t)op.params[2].value.b << 32
					& 0xFFFFFFFF00000000lu;
	time_freq |= op.params[2].value.a;
	time_ms = (uint64_t)op.params[3].value.b << 32
					& 0xFFFFFFFF00000000lu;
	time_ms |= op.params[3].value.a;

	(void)ADBG_EXPECT_TEEC_RESULT(1, time_start !=
				TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO);
	(void)ADBG_EXPECT_TEEC_RESULT(1, time_end !=
				TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO);
	(void)ADBG_EXPECT_TEEC_RESULT(1, time_end > time_start);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1005(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;
	uint32_t loop_count = 1000, i = 0;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < loop_count; i++)
		res |= TEEC_InvokeCommand(&session, TA_CORE_API_CMD_RET_VALUE,
					NULL, &ret_orig);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_SPECIFIC_RET_VALUE, res);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1010(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	int ta_verify_ret = 0;

	UNUSED(ta_verify_ret);

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.params[0].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern1;
	op.params[0].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern2;
	op.params[1].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern3;
	op.params[1].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern4;
	op.params[2].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern5;
	op.params[2].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern6;
	op.params[3].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern7;
	op.params[3].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern8;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT,
					TEEC_VALUE_INPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
					TA_CORE_API_CMD_PARAM_TYPE_VALUE_INPUT,
					&op, &ret_orig));

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1011(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	int ta_verify_ret = 0;

	UNUSED(ta_verify_ret);

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT,
					TEEC_VALUE_OUTPUT,
					TEEC_VALUE_OUTPUT,
					TEEC_VALUE_OUTPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_VALUE_OUTPUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern8,
							op.params[0].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern7,
							op.params[0].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern6,
							op.params[1].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern5,
							op.params[1].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern4,
							op.params[2].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern3,
							op.params[2].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern2,
							op.params[3].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern1,
							op.params[3].value.b);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1012(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.params[0].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternA;
	op.params[0].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternB;
	op.params[1].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternC;
	op.params[1].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternD;
	op.params[2].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternA;
	op.params[2].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternB;
	op.params[3].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternC;
	op.params[3].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternD;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_VALUE_INOUT,
				 &op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternF,
							op.params[0].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternF,
							op.params[0].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternF,
							op.params[1].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternF,
							op.params[1].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternE,
							op.params[2].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternE,
							op.params[2].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternE,
							op.params[3].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternE,
							op.params[3].value.b);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1013(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	op.params[0].value.a = TA_CORE_API_PARAM_TYPE_Test_Pattern0;
	op.params[0].value.b = TA_CORE_API_PARAM_TYPE_Test_Pattern0;
	op.params[2].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternA;
	op.params[2].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternA;
	op.params[3].value.a = TA_CORE_API_PARAM_TYPE_Test_PatternB;
	op.params[3].value.b = TA_CORE_API_PARAM_TYPE_Test_PatternB;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
					TEEC_VALUE_OUTPUT,
					TEEC_VALUE_INOUT,
					TEEC_VALUE_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(
	    TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
	    TEEC_InvokeCommand(&session, TA_CORE_API_CMD_PARAM_TYPE_VALUE_MIX,
						&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternC,
							op.params[1].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternC,
							op.params[1].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternD,
							op.params[2].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_PatternD,
							op.params[2].value.b);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern1,
							op.params[3].value.a);
	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_Test_Pattern1,
							op.params[3].value.b);

	TEEC_CloseSession(&session);
	return case_res;
}

#define TEMP_BUFFER_COUNT 4
#define SIZE_1M (1024 * 1024)
#define TEMP_BUFFER_TEST_SIZE SIZE_1M
int xtest_isee_test_1020(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern0,
					TA_CORE_API_PARAM_TYPE_Test_Pattern1,
					TA_CORE_API_PARAM_TYPE_Test_Pattern2,
					TA_CORE_API_PARAM_TYPE_Test_Pattern3};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		op.params[i].tmpref.buffer = temp_buffer[i];
		op.params[i].tmpref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					TEEC_MEMREF_TEMP_INPUT,
					TEEC_MEMREF_TEMP_INPUT,
					TEEC_MEMREF_TEMP_INPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
			TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_INPUT,
			&op, &ret_orig));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1021(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {0};
	uint32_t ret_orig;
	int i;

	UNUSED(temp_buffer_pattern);

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		op.params[i].tmpref.buffer = temp_buffer[i];
		op.params[i].tmpref.size = sizeof(unsigned int);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_MEMREF_TEMP_OUTPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_OUTPUT,
				&op, &ret_orig));


	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[0].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern4,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[1].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern5,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[2].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern6,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[3].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern7,
					sizeof(unsigned int)));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1022(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern8,
					TA_CORE_API_PARAM_TYPE_Test_Pattern9,
					TA_CORE_API_PARAM_TYPE_Test_PatternA,
					TA_CORE_API_PARAM_TYPE_Test_PatternB};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		op.params[i].tmpref.buffer = temp_buffer[i];
		op.params[i].tmpref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
					TEEC_MEMREF_TEMP_INOUT,
					TEEC_MEMREF_TEMP_INOUT,
					TEEC_MEMREF_TEMP_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_INOUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[0].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_PatternC,
					op.params[0].tmpref.size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[1].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_PatternD,
					op.params[1].tmpref.size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[2].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_PatternE,
					op.params[2].tmpref.size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[3].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_PatternF,
					op.params[3].tmpref.size));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1023(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
				TA_CORE_API_PARAM_TYPE_Test_Pattern8,
				TA_CORE_API_PARAM_TYPE_Test_Pattern7,
				TA_CORE_API_PARAM_TYPE_Test_Pattern6,
				TA_CORE_API_PARAM_TYPE_Test_Pattern5};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);

	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		op.params[i].tmpref.buffer = temp_buffer[i];
		op.params[i].tmpref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					TEEC_MEMREF_TEMP_OUTPUT,
					TEEC_MEMREF_TEMP_INOUT,
					TEEC_MEMREF_TEMP_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_MIX,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[1].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern3,
					op.params[1].tmpref.size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[2].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern2,
					op.params[2].tmpref.size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(op.params[3].tmpref.buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern1,
					op.params[3].tmpref.size));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1030(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
				TA_CORE_API_PARAM_TYPE_Test_Pattern0,
				TA_CORE_API_PARAM_TYPE_Test_Pattern1,
				TA_CORE_API_PARAM_TYPE_Test_Pattern2,
				TA_CORE_API_PARAM_TYPE_Test_Pattern3};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = TEEC_MEM_INPUT;
		res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto share_mem_allocate_failed;
		}
		b_shared_mem_registered[i] = 1;
		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);

		MEMSET_UINT32(shared_mem[i].buffer, temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INPUT,
				&op, &ret_orig));

share_mem_allocate_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1031(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		shared_mem[i].size = sizeof(unsigned int);
		shared_mem[i].flags = TEEC_MEM_OUTPUT;
		res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto share_mem_allocate_failed;
		}
		b_shared_mem_registered[i] = 1;
		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(unsigned int);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_OUTPUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[0].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern4,
					shared_mem[0].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[1].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern5,
					shared_mem[1].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[2].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern6,
					shared_mem[2].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[3].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern7,
					shared_mem[3].size));

share_mem_allocate_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1032(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern8,
					TA_CORE_API_PARAM_TYPE_Test_Pattern9,
					TA_CORE_API_PARAM_TYPE_Test_PatternA,
					TA_CORE_API_PARAM_TYPE_Test_PatternB};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
		res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto share_mem_allocate_failed;
		}
		b_shared_mem_registered[i] = 1;
		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);

		MEMSET_UINT32(shared_mem[i].buffer, temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));
	}

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INOUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[0].buffer,
				TA_CORE_API_PARAM_TYPE_Test_PatternC,
				shared_mem[0].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[1].buffer,
				TA_CORE_API_PARAM_TYPE_Test_PatternD,
				shared_mem[1].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[2].buffer,
				TA_CORE_API_PARAM_TYPE_Test_PatternE,
				shared_mem[2].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[3].buffer,
				TA_CORE_API_PARAM_TYPE_Test_PatternF,
				shared_mem[3].size));

share_mem_allocate_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1033(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned int shared_mem_flags[TEMP_BUFFER_COUNT] = {
					TEEC_MEM_INPUT, TEEC_MEM_OUTPUT,
					TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
					TEEC_MEM_INPUT | TEEC_MEM_OUTPUT};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern8,
					TA_CORE_API_PARAM_TYPE_Test_Pattern7,
					TA_CORE_API_PARAM_TYPE_Test_Pattern6,
					TA_CORE_API_PARAM_TYPE_Test_Pattern5};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = shared_mem_flags[i];
		res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto share_mem_allocate_failed;
		}
		b_shared_mem_registered[i] = 1;
		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);

		MEMSET_UINT32(shared_mem[i].buffer, temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_MIX,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[1].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern3,
					shared_mem[1].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[2].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern2,
					shared_mem[2].size));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(shared_mem[3].buffer,
					TA_CORE_API_PARAM_TYPE_Test_Pattern1,
					shared_mem[3].size));

share_mem_allocate_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1034(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern0,
					TA_CORE_API_PARAM_TYPE_Test_Pattern1,
					TA_CORE_API_PARAM_TYPE_Test_Pattern2,
					TA_CORE_API_PARAM_TYPE_Test_Pattern3};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		shared_mem[i].buffer = temp_buffer[i];
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = TEEC_MEM_INPUT;

		res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
					TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}
		b_shared_mem_registered[i] = 1;

		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INPUT,
				&op, &ret_orig));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1035(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		shared_mem[i].buffer = temp_buffer[i];
		shared_mem[i].size = sizeof(unsigned int);
		shared_mem[i].flags = TEEC_MEM_OUTPUT;

		res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}
		b_shared_mem_registered[i] = 1;

		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(unsigned int);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_OUTPUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[0],
					TA_CORE_API_PARAM_TYPE_Test_Pattern4,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[1],
					TA_CORE_API_PARAM_TYPE_Test_Pattern5,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[2],
					TA_CORE_API_PARAM_TYPE_Test_Pattern6,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[3],
					TA_CORE_API_PARAM_TYPE_Test_Pattern7,
					sizeof(unsigned int)));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1036(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
				TA_CORE_API_PARAM_TYPE_Test_Pattern8,
				TA_CORE_API_PARAM_TYPE_Test_Pattern9,
				TA_CORE_API_PARAM_TYPE_Test_PatternA,
				TA_CORE_API_PARAM_TYPE_Test_PatternB};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
				&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		shared_mem[i].buffer = temp_buffer[i];
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

		res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}
		b_shared_mem_registered[i] = 1;

		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INOUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[0],
					TA_CORE_API_PARAM_TYPE_Test_PatternC,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[1],
					TA_CORE_API_PARAM_TYPE_Test_PatternD,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[2],
					TA_CORE_API_PARAM_TYPE_Test_PatternE,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[3],
					TA_CORE_API_PARAM_TYPE_Test_PatternF,
					sizeof(unsigned int)));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1037(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem[TEMP_BUFFER_COUNT] = { {0} };
	unsigned int b_shared_mem_registered[TEMP_BUFFER_COUNT] = {0};
	unsigned int shared_mem_flags[TEMP_BUFFER_COUNT] = {
					TEEC_MEM_INPUT, TEEC_MEM_OUTPUT,
					TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
					TEEC_MEM_INPUT | TEEC_MEM_OUTPUT};
	unsigned char *temp_buffer[TEMP_BUFFER_COUNT] = {0};
	unsigned int temp_buffer_pattern[TEMP_BUFFER_COUNT] = {
					TA_CORE_API_PARAM_TYPE_Test_Pattern8,
					TA_CORE_API_PARAM_TYPE_Test_Pattern7,
					TA_CORE_API_PARAM_TYPE_Test_Pattern6,
					TA_CORE_API_PARAM_TYPE_Test_Pattern5};
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		temp_buffer[i] = (unsigned char *)malloc(
						TEMP_BUFFER_TEST_SIZE);
		if (!temp_buffer[i]) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}

		MEMSET_UINT32(temp_buffer[i], temp_buffer_pattern[i],
						sizeof(temp_buffer_pattern[i]));

		shared_mem[i].buffer = temp_buffer[i];
		shared_mem[i].size = sizeof(temp_buffer_pattern[i]);
		shared_mem[i].flags = shared_mem_flags[i];

		res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000,
							&shared_mem[i]);
		if (res != TEEC_SUCCESS) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(
						TEEC_ERROR_OUT_OF_MEMORY);
			goto buffer_alloc_failed;
		}
		b_shared_mem_registered[i] = 1;

		op.params[i].memref.parent = &shared_mem[i];
		op.params[i].memref.size = sizeof(temp_buffer_pattern[i]);
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE,
					TEEC_MEMREF_WHOLE);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_MIX,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[1],
					TA_CORE_API_PARAM_TYPE_Test_Pattern3,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[2],
					TA_CORE_API_PARAM_TYPE_Test_Pattern2,
					sizeof(unsigned int)));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
			ARRAY_MATCH_PATTERN(temp_buffer[3],
					TA_CORE_API_PARAM_TYPE_Test_Pattern1,
					sizeof(unsigned int)));

buffer_alloc_failed:
	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		if (temp_buffer[i])
			free(temp_buffer[i]);
		if (b_shared_mem_registered[i])
			TEEC_ReleaseSharedMemory(&shared_mem[i]);
	}

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1040(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *shared_buf_ptr = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT;
	res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	shared_buf_ptr = shared_mem.buffer;
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern0, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern1, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_Pattern2, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_Pattern3, TEMP_BUFFER_TEST_SIZE);

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INPUT,
				&op, &ret_orig));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1041(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *shared_buf_ptr = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_OUTPUT;
	res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	shared_buf_ptr = shared_mem.buffer;

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
			TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_OUTPUT,
			&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 0,
			TA_CORE_API_PARAM_TYPE_Test_Pattern4,
			TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
			TA_CORE_API_PARAM_TYPE_Test_Pattern5,
			TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
			TA_CORE_API_PARAM_TYPE_Test_Pattern6,
			TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
			TA_CORE_API_PARAM_TYPE_Test_Pattern7,
			TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1042(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *shared_buf_ptr = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	shared_buf_ptr = shared_mem.buffer;
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern8, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern9, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_PatternA, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_PatternB, TEMP_BUFFER_TEST_SIZE);

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INOUT,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 0,
				TA_CORE_API_PARAM_TYPE_Test_PatternC,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
				TA_CORE_API_PARAM_TYPE_Test_PatternD,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
				TA_CORE_API_PARAM_TYPE_Test_PatternE,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
				TA_CORE_API_PARAM_TYPE_Test_PatternF,
				TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1043(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *shared_buf_ptr = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	res = TEEC_AllocateSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	shared_buf_ptr = shared_mem.buffer;
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern8, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern7, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_Pattern6, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_Pattern5, TEMP_BUFFER_TEST_SIZE);

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_MIX,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 1,
				TA_CORE_API_PARAM_TYPE_Test_Pattern3,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 2,
				TA_CORE_API_PARAM_TYPE_Test_Pattern2,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(shared_buf_ptr + TEMP_BUFFER_TEST_SIZE * 3,
				TA_CORE_API_PARAM_TYPE_Test_Pattern1,
				TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1044(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *temp_buffer = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE * 4);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto buffer_alloc_failed;
	}
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern0, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern1, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_Pattern2, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_Pattern3, TEMP_BUFFER_TEST_SIZE);

	shared_mem.buffer = temp_buffer;
	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT;
	res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_INPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
			TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INPUT,
				&op, &ret_orig));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:
	if (temp_buffer)
		free(temp_buffer);
buffer_alloc_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1045(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *temp_buffer = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE * 4);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto buffer_alloc_failed;
	}

	shared_mem.buffer = temp_buffer;
	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_OUTPUT;
	res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
			TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_OUTPUT,
			&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 0,
				TA_CORE_API_PARAM_TYPE_Test_Pattern4,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
				TA_CORE_API_PARAM_TYPE_Test_Pattern5,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
				TA_CORE_API_PARAM_TYPE_Test_Pattern6,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
				TA_CORE_API_PARAM_TYPE_Test_Pattern7,
				TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:
	if (temp_buffer)
		free(temp_buffer);
buffer_alloc_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1046(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *temp_buffer = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE * 4);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto buffer_alloc_failed;
	}
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern8, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern9, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_PatternA, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_PatternB, TEMP_BUFFER_TEST_SIZE);

	shared_mem.buffer = temp_buffer;
	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
			TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INOUT,
			&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 0,
				TA_CORE_API_PARAM_TYPE_Test_PatternC,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
				TA_CORE_API_PARAM_TYPE_Test_PatternD,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
				TA_CORE_API_PARAM_TYPE_Test_PatternE,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
				TA_CORE_API_PARAM_TYPE_Test_PatternF,
				TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:
	if (temp_buffer)
		free(temp_buffer);
buffer_alloc_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1047(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	struct TEEC_SharedMemory shared_mem = {0};
	unsigned char *temp_buffer = NULL;
	uint32_t ret_orig;
	int i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	temp_buffer = (unsigned char *)malloc(TEMP_BUFFER_TEST_SIZE * 4);
	if (!temp_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto buffer_alloc_failed;
	}
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 0,
		TA_CORE_API_PARAM_TYPE_Test_Pattern8, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
		TA_CORE_API_PARAM_TYPE_Test_Pattern7, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
		TA_CORE_API_PARAM_TYPE_Test_Pattern6, TEMP_BUFFER_TEST_SIZE);
	MEMSET_UINT32(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
		TA_CORE_API_PARAM_TYPE_Test_Pattern5, TEMP_BUFFER_TEST_SIZE);

	shared_mem.buffer = temp_buffer;
	shared_mem.size = TEMP_BUFFER_TEST_SIZE * 4;
	shared_mem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	res = TEEC_RegisterSharedMemory(&xtest_isee_ctx_1000, &shared_mem);
	if (res != TEEC_SUCCESS) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		goto share_mem_reg_failed;
	}

	for (i = 0; i < TEMP_BUFFER_COUNT; i++) {
		op.params[i].memref.parent = &shared_mem;
		op.params[i].memref.offset = TEMP_BUFFER_TEST_SIZE * i;
		op.params[i].memref.size = TEMP_BUFFER_TEST_SIZE;
	}
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
					TEEC_MEMREF_PARTIAL_OUTPUT,
					TEEC_MEMREF_PARTIAL_INOUT,
					TEEC_MEMREF_PARTIAL_INOUT);

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_TEST_SUCCESS,
		TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_MIX,
				&op, &ret_orig));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 1,
				TA_CORE_API_PARAM_TYPE_Test_Pattern3,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 2,
				TA_CORE_API_PARAM_TYPE_Test_Pattern2,
				TEMP_BUFFER_TEST_SIZE));

	(void)ADBG_EXPECT_TEEC_RESULT(TA_CORE_API_PARAM_TYPE_PATTERN_MATCH,
		ARRAY_MATCH_PATTERN(temp_buffer + TEMP_BUFFER_TEST_SIZE * 3,
				TA_CORE_API_PARAM_TYPE_Test_Pattern1,
				TEMP_BUFFER_TEST_SIZE));

	TEEC_ReleaseSharedMemory(&shared_mem);
share_mem_reg_failed:
	if (temp_buffer)
		free(temp_buffer);
buffer_alloc_failed:

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1100(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	unsigned char *rpmb_buffer = NULL;
	int rpmb_access_ret = 0;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

#define RPMB_TEST_SIZE 128
#define RPMB_TEST_PATTERN 0xFA
	rpmb_buffer = (unsigned char *)malloc(RPMB_TEST_SIZE);
	if (!rpmb_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		return case_res;
	}
	memset(rpmb_buffer, RPMB_TEST_PATTERN, RPMB_TEST_SIZE);

	op.params[0].tmpref.buffer = rpmb_buffer;
	op.params[0].tmpref.size = RPMB_TEST_SIZE;
	op.params[1].value.a = RPMB_TEST_SIZE;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE);

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_RPMB_WRITE, &op, &ret_orig));

	rpmb_access_ret = op.params[1].value.a;
	(void)ADBG_EXPECT_TEEC_SUCCESS(rpmb_access_ret);

	free(rpmb_buffer);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1101(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	unsigned char *rpmb_buffer = NULL;
	int rpmb_access_ret = 0, i;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

#define RPMB_TEST_SIZE 128
#define RPMB_TEST_PATTERN 0xFA
	rpmb_buffer = (unsigned char *)malloc(RPMB_TEST_SIZE);
	if (!rpmb_buffer) {
		(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_OUT_OF_MEMORY);
		return case_res;
	}
	memset(rpmb_buffer, 0x00, RPMB_TEST_SIZE);

	op.params[0].tmpref.buffer = rpmb_buffer;
	op.params[0].tmpref.size = RPMB_TEST_SIZE;
	op.params[1].value.a = RPMB_TEST_SIZE;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,
				TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE);

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_RPMB_READ, &op, &ret_orig));

	rpmb_access_ret = op.params[1].value.a;
	(void)ADBG_EXPECT_TEEC_SUCCESS(rpmb_access_ret);

	for (i = 0; i < RPMB_TEST_SIZE; i++) {
		if (rpmb_buffer[i] != RPMB_TEST_PATTERN) {
			(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_ERROR_BAD_FORMAT);
			break;
		}
	}

	free(rpmb_buffer);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1102(void)
{
	unsigned int res;
	struct TEEC_Session session;
	struct TEEC_Operation op;
	uint32_t ret_orig;
	uint32_t sys_time_s, sys_time_ms;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

#define SYSTEM_NON_ZERO_CHECK_VALUE 0x12345678
	op.params[0].value.a = SYSTEM_NON_ZERO_CHECK_VALUE;
	op.params[1].value.a = SYSTEM_NON_ZERO_CHECK_VALUE;
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_VALUE_INOUT,
					TEEC_NONE, TEEC_NONE);

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
			TA_CORE_API_CMD_Get_System_Time, &op, &ret_orig));

	sys_time_s = op.params[0].value.a;
	sys_time_ms = op.params[1].value.a;
	(void)ADBG_EXPECT_TEEC_SUCCESS(sys_time_s ==
					SYSTEM_NON_ZERO_CHECK_VALUE);
	(void)ADBG_EXPECT_TEEC_SUCCESS(sys_time_ms ==
					SYSTEM_NON_ZERO_CHECK_VALUE);
	(void)ADBG_EXPECT_TEEC_SUCCESS(sys_time_s == 0x0 &&
					sys_time_ms == 0x0);

	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1200(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_INVOKE_COMMAND_ENTRY_POINT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1201(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_ALLOCATE_TRANSIENT_OBJECT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1203(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_POPULATE_TRANSIENT_OBJECT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1204(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_ALLOCATE_OPERATION,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1206(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_SET_OPERATION_KEY,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1207(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_GENERATE_RANDOM,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1208(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_CHECK_MEMORY_ACCESS_RIGHTS,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1209(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_MALLOC, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1210(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_REALLOC, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1212(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_MEM_MOVE, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1213(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_MEM_COMPARE, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1214(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_MEM_FILL, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1215(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_GET_PROPERTY_AS_UUID,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1216(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_LOG_VPRINTF, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1217(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_LOG_PRINTF, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1218(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_DBG_PRINTF, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1219(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CORE_API_CMD_DBG_VPRINTF, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1300(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_AES_ECB, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1301(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_AES_CBC, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1302(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_AES_CTR, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1303(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_SHA1, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1304(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_SHA256, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1305(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_HMAC_SHA256,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1306(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_HMAC_SHA1, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1307(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_RSA_PKCS1_PSS_MGF1_SHA1,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1308(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_RSA_NOPAD, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1400(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_ASYMMETRIC_ENCRYPT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1401(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_ASYMMETRIC_DECRYPT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1402(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_ASYMMETRIC_SIGN_DIGEST,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1403(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_ASYMMETRIC_VERIFY_DIGEST,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1404(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_DIGEST_UPDATE,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1405(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_DIGEST_DO_FINAL,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1406(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_CIPHER_INIT,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1407(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_CIPHER_UPDATE,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1408(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_CIPHER_DO_FINAL,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1409(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_MAC_INIT, NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1410(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_MAC_UPDATE,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1411(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_MAC_COMPUTE_FINAL,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1412(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				TA_CRYPTO_API_CMD_MAC_COMPARE_FINAL,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1800(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				MTK_TEST_CRYPTO_AES_ECB_PERFORMANCE1,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1801(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				MTK_TEST_CRYPTO_AES_ECB_PERFORMANCE2,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1802(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				MTK_TEST_CRYPTO_AES_ECB_PERFORMANCE3,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}

int xtest_isee_test_1803(void)
{
	unsigned int res;
	struct TEEC_Session session;
	uint32_t ret_orig;

	res = xtest_teec_open_session(&xtest_isee_ctx_1000, &session,
					&isee_test_ta_uuid, NULL, &ret_orig);
	if (!ADBG_EXPECT_TEEC_SUCCESS(res))
		return res;

	(void)ADBG_EXPECT_TEEC_SUCCESS(TEEC_InvokeCommand(&session,
				MTK_TEST_CRYPTO_AES_ECB_PERFORMANCE4,
				NULL, &ret_orig));
	TEEC_CloseSession(&session);
	return case_res;
}
