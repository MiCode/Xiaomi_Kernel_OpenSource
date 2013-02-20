/* Copyright (c) 2008-2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * DAL remote test device API.
 */

#include <linux/kernel.h>

#include <mach/dal.h>

#define REMOTE_UNITTEST_DEVICEID 0xDA1DA1DA

enum {
	DALRPC_TEST_API_0 = DALDEVICE_FIRST_DEVICE_API_IDX,
	DALRPC_TEST_API_1,
	DALRPC_TEST_API_2,
	DALRPC_TEST_API_3,
	DALRPC_TEST_API_4,
	DALRPC_TEST_API_5,
	DALRPC_TEST_API_6,
	DALRPC_TEST_API_7,
	DALRPC_TEST_API_8,
	DALRPC_TEST_API_9,
	DALRPC_TEST_API_10,
	DALRPC_TEST_API_11,
	DALRPC_TEST_API_12,
	DALRPC_TEST_API_13,
	DALRPC_TEST_API_14,
	DALRPC_TEST_API_15,
	DALRPC_TEST_API_16,
	DALRPC_TEST_API_17
};

#define REMOTE_UNITTEST_INARG_1 0x01010101
#define REMOTE_UNITTEST_INARG_2 0x20202020
#define REMOTE_UNITTEST_INARG_3 0x12121212
#define REMOTE_UNITTEST_INPUT_HANDLE 0xDA1FDA1F
#define REMOTE_UNITTEST_OUTARG_1 0xBEEFDEAD

#define REMOTE_UNITTEST_REGULAR_EVENT 0
#define REMOTE_UNITTEST_CALLBACK_EVENT 1

#define REMOTE_UNITTEST_BAD_PARAM 0x10

struct remote_test_data {
	uint32_t regular_event;
	uint32_t test[32];
	uint32_t payload_event;
};

static int remote_unittest_0(void *handle, uint32_t s1)
{
	return dalrpc_fcn_0(DALRPC_TEST_API_0, handle, s1);
}

static int remote_unittest_1(void *handle, uint32_t s1, uint32_t s2)
{
	return dalrpc_fcn_1(DALRPC_TEST_API_1, handle, s1, s2);
}

static int remote_unittest_2(void *handle, uint32_t s1, uint32_t *p_s2)
{
	return dalrpc_fcn_2(DALRPC_TEST_API_2, handle, s1, p_s2);
}

static int remote_unittest_3(void *handle, uint32_t s1, uint32_t s2,
			     uint32_t s3)
{
	return dalrpc_fcn_3(DALRPC_TEST_API_3, handle, s1, s2, s3);
}

static int remote_unittest_4(void *handle, uint32_t s1, uint32_t s2,
			     uint32_t *p_s3)
{
	return dalrpc_fcn_4(DALRPC_TEST_API_4, handle, s1, s2, p_s3);
}

static int remote_unittest_5(void *handle, const void *ibuf, uint32_t ilen)
{
	return dalrpc_fcn_5(DALRPC_TEST_API_5, handle, ibuf, ilen);
}

static int remote_unittest_6(void *handle, uint32_t s1, const void *ibuf,
			     uint32_t ilen)
{
	return dalrpc_fcn_6(DALRPC_TEST_API_6, handle, s1, ibuf, ilen);
}

static int remote_unittest_7(void *handle, const void *ibuf, uint32_t ilen,
			     void *obuf, uint32_t olen, uint32_t *oalen)
{
	return dalrpc_fcn_7(DALRPC_TEST_API_7, handle, ibuf, ilen, obuf,
			    olen, oalen);
}

static int remote_unittest_8(void *handle, const void *ibuf, uint32_t ilen,
			     void *obuf, uint32_t olen)
{
	return dalrpc_fcn_8(DALRPC_TEST_API_8, handle, ibuf, ilen, obuf, olen);
}

static int remote_unittest_9(void *handle, void *obuf, uint32_t olen)
{
	return dalrpc_fcn_9(DALRPC_TEST_API_9, handle, obuf, olen);
}

static int remote_unittest_10(void *handle, uint32_t s1, const void *ibuf,
			      uint32_t ilen, void *obuf, uint32_t olen,
			      uint32_t *oalen)
{
	return dalrpc_fcn_10(DALRPC_TEST_API_10, handle, s1, ibuf, ilen, obuf,
			     olen, oalen);
}

static int remote_unittest_11(void *handle, uint32_t s1, void *obuf,
			      uint32_t olen)
{
	return dalrpc_fcn_11(DALRPC_TEST_API_11, handle, s1, obuf, olen);
}

static int remote_unittest_12(void *handle, uint32_t s1, void *obuf,
			      uint32_t olen, uint32_t *oalen)
{
	return dalrpc_fcn_12(DALRPC_TEST_API_12, handle, s1, obuf, olen,
			     oalen);
}

static int remote_unittest_13(void *handle, const void *ibuf, uint32_t ilen,
			      const void *ibuf2, uint32_t ilen2, void *obuf,
			      uint32_t olen)
{
	return dalrpc_fcn_13(DALRPC_TEST_API_13, handle, ibuf, ilen, ibuf2,
			     ilen2, obuf, olen);
}

static int remote_unittest_14(void *handle, const void *ibuf, uint32_t ilen,
			      void *obuf, uint32_t olen, void *obuf2,
			      uint32_t olen2, uint32_t *oalen2)
{
	return dalrpc_fcn_14(DALRPC_TEST_API_14, handle, ibuf, ilen, obuf,
			     olen, obuf2, olen2, oalen2);
}

static int remote_unittest_15(void *handle, const void *ibuf, uint32_t ilen,
			      const void *ibuf2, uint32_t ilen2, void *obuf,
			      uint32_t olen, uint32_t *oalen, void *obuf2,
			      uint32_t olen2)
{
	return dalrpc_fcn_15(DALRPC_TEST_API_15, handle, ibuf, ilen, ibuf2,
			     ilen2, obuf, olen, oalen, obuf2, olen2);
}

static int remote_unittest_eventcfg(void *handle, const void *ibuf,
				    uint32_t ilen)
{
	return dalrpc_fcn_5(DALRPC_TEST_API_16, handle, ibuf, ilen);
}

static int remote_unittest_eventtrig(void *handle, uint32_t event_idx)
{
	return dalrpc_fcn_0(DALRPC_TEST_API_17, handle, event_idx);
}
