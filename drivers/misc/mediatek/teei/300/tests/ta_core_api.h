/*
 * Copyright (c) 2015-2016 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef TA_CORE_API_H
#define TA_CORE_API_H

#include <tee_client_api.h>

/* errors defined and used by TA/DRV test cases only */
#include "teei_internal_types.h"
#include "teei_ta_drv_types.h"

#define DRM_01_UUID \
	{ \
		0x08110000, 0x0000, 0x0000, \
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } \
	}

/* TA Commands */
#define TA_CORE_API_CMD_PRINT 0x1
#define TA_CORE_API_CMD_RET_VALUE 0x2
#define TA_CORE_API_CMD_READ_CNTVCT 0x3
#define TA_CORE_API_CMD_PARAM_TYPE_VALUE_INPUT 0x10
#define TA_CORE_API_CMD_PARAM_TYPE_VALUE_OUTPUT 0x11
#define TA_CORE_API_CMD_PARAM_TYPE_VALUE_INOUT 0x12
#define TA_CORE_API_CMD_PARAM_TYPE_VALUE_MIX 0x13
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_INPUT 0x20
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_OUTPUT 0x21
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_INOUT 0x22
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_TMEP_MIX 0x23
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INPUT 0x30
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_OUTPUT 0x31
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_INOUT 0x32
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_WHOLE_MIX 0x33
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INPUT 0x40
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_OUTPUT 0x41
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_INOUT 0x42
#define TA_CORE_API_CMD_PARAM_TYPE_MEMREF_PARTIAL_MIX 0x43
#define TA_CORE_API_CMD_RPMB_WRITE 0x101
#define TA_CORE_API_CMD_RPMB_READ 0x102
#define TA_CORE_API_CMD_Get_System_Time 0x103
#define TA_CORE_API_CMD_INVOKE_COMMAND_ENTRY_POINT 0x500
#define TA_CORE_API_CMD_ALLOCATE_TRANSIENT_OBJECT 0x501
#define TA_CORE_API_CMD_POPULATE_TRANSIENT_OBJECT 0x503
#define TA_CORE_API_CMD_ALLOCATE_OPERATION 0x504
#define TA_CORE_API_CMD_SET_OPERATION_KEY 0x506
#define TA_CORE_API_CMD_GENERATE_RANDOM 0x507
#define TA_CORE_API_CMD_CHECK_MEMORY_ACCESS_RIGHTS 0x508
#define TA_CORE_API_CMD_MALLOC 0x509
#define TA_CORE_API_CMD_REALLOC 0x510
#define TA_CORE_API_CMD_MEM_MOVE 0x512
#define TA_CORE_API_CMD_MEM_COMPARE 0x513
#define TA_CORE_API_CMD_MEM_FILL 0x514
#define TA_CORE_API_CMD_GET_PROPERTY_AS_UUID 0x515
#define TA_CORE_API_CMD_LOG_VPRINTF 0x516
#define TA_CORE_API_CMD_LOG_PRINTF 0x517
#define TA_CORE_API_CMD_DBG_PRINTF 0x518
#define TA_CORE_API_CMD_DBG_VPRINTF 0x519
#define TA_CRYPTO_API_CMD_AES_ECB 0x1300
#define TA_CRYPTO_API_CMD_AES_CBC 0x1301
#define TA_CRYPTO_API_CMD_AES_CTR 0x1302
#define TA_CRYPTO_API_CMD_SHA1 0x1303
#define TA_CRYPTO_API_CMD_SHA256 0x1304
#define TA_CRYPTO_API_CMD_HMAC_SHA256 0x1305
#define TA_CRYPTO_API_CMD_HMAC_SHA1 0x1306
#define TA_CRYPTO_API_CMD_RSA_PKCS1_PSS_MGF1_SHA1 0x1307
#define TA_CRYPTO_API_CMD_RSA_NOPAD 0x1308
#define TA_CRYPTO_API_CMD_ASYMMETRIC_ENCRYPT 0x1400
#define TA_CRYPTO_API_CMD_ASYMMETRIC_DECRYPT 0x1401
#define TA_CRYPTO_API_CMD_ASYMMETRIC_SIGN_DIGEST 0x1402
#define TA_CRYPTO_API_CMD_ASYMMETRIC_VERIFY_DIGEST 0x1403
#define TA_CRYPTO_API_CMD_DIGEST_UPDATE 0x1404
#define TA_CRYPTO_API_CMD_DIGEST_DO_FINAL 0x1405
#define TA_CRYPTO_API_CMD_CIPHER_INIT 0x1406
#define TA_CRYPTO_API_CMD_CIPHER_UPDATE 0x1407
#define TA_CRYPTO_API_CMD_CIPHER_DO_FINAL 0x1408
#define TA_CRYPTO_API_CMD_MAC_INIT 0x1409
#define TA_CRYPTO_API_CMD_MAC_UPDATE 0x1410
#define TA_CRYPTO_API_CMD_MAC_COMPUTE_FINAL 0x1411
#define TA_CRYPTO_API_CMD_MAC_COMPARE_FINAL 0x1412
#define TA_CORE_API_UNSUPPORTED_CMD 0xFFFFFFF1

/* Param Type Test Values */
#define TA_CORE_API_PARAM_TYPE_Test_Pattern0 0x12345678
#define TA_CORE_API_PARAM_TYPE_Test_Pattern1 0x90ABCEDF
#define TA_CORE_API_PARAM_TYPE_Test_Pattern2 0x87654321
#define TA_CORE_API_PARAM_TYPE_Test_Pattern3 0xFDECBA09
#define TA_CORE_API_PARAM_TYPE_Test_Pattern4 0x55667788
#define TA_CORE_API_PARAM_TYPE_Test_Pattern5 0x33441122
#define TA_CORE_API_PARAM_TYPE_Test_Pattern6 0xAAAACCCC
#define TA_CORE_API_PARAM_TYPE_Test_Pattern7 0xBBBBDDDD
#define TA_CORE_API_PARAM_TYPE_Test_Pattern8 0x11111111
#define TA_CORE_API_PARAM_TYPE_Test_Pattern9 0x22222222
#define TA_CORE_API_PARAM_TYPE_Test_PatternA 0x33333333
#define TA_CORE_API_PARAM_TYPE_Test_PatternB 0x44444444
#define TA_CORE_API_PARAM_TYPE_Test_PatternC 0x55555555
#define TA_CORE_API_PARAM_TYPE_Test_PatternD 0x66666666
#define TA_CORE_API_PARAM_TYPE_Test_PatternE 0x77777777
#define TA_CORE_API_PARAM_TYPE_Test_PatternF 0xFFFFFFFF
#define TA_CORE_API_PARAM_TYPE_Test_Pattern_ZERO 0x00000000

/* Error Code */
#define TA_CORE_API_SPECIFIC_RET_VALUE 0x23457896
#define TA_CORE_API_PARAM_TYPE_TEST_SUCCESS 0xABCD5555
#define TA_CORE_API_PARAM_TYPE_TEST_FAIL 0xDEAD9999
#define TA_CORE_API_PARAM_TYPE_BUFFER0_TEST_FAIL 0xDEAD0000
#define TA_CORE_API_PARAM_TYPE_BUFFER1_TEST_FAIL 0xDEAD1111
#define TA_CORE_API_PARAM_TYPE_BUFFER2_TEST_FAIL 0xDEAD2222
#define TA_CORE_API_PARAM_TYPE_BUFFER3_TEST_FAIL 0xDEAD3333
#define TA_CORE_API_PARAM_TYPE_PATTERN_MATCH 0x1
#define TA_CORE_API_PARAM_TYPE_PATTERN_NOT_MATCH 0x0

extern unsigned int xtest_teec_open_session(struct TEEC_Context *context,
						struct TEEC_Session *session,
						const struct TEEC_UUID *uuid,
						struct TEEC_Operation *op,
						uint32_t *ret_orig);
extern const char *ADBG_GetFileBase(const char *const FileName_p);
extern bool Do_ADBG_Expect(const char *const FileName_p,
				const int LineNumber,
				const int Expected,
				const int Got);
#define ADBG_EXPECT_ENUM(Expected, Got) \
			Do_ADBG_Expect(__FILE__, __LINE__, Expected, Got)
#define ADBG_EXPECT_TEEC_SUCCESS(got) ADBG_EXPECT_ENUM(TEEC_SUCCESS, got)
#define ADBG_EXPECT_TEEC_RESULT(exp, got) ADBG_EXPECT_ENUM(exp, got)

extern int compare_array_with_pattern(unsigned char *buffer,
					unsigned int pattern,
					unsigned int size_in_byte);
#define ARRAY_MATCH_PATTERN(buffer, pattern, size_in_byte) \
	compare_array_with_pattern(buffer, pattern, size_in_byte)

#define malloc(size) kmalloc(size, GFP_KERNEL | GFP_ATOMIC)
#define free kfree
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define tz_malloc(size) __get_free_pages(GFP_KERNEL, \
					get_order(ROUND_UP(size, SZ_4K)))
#define tz_free(addr, size) free_pages((unsigned long)addr, \
					get_order(ROUND_UP(size, SZ_4K)))
extern int case_res;

#define UNUSED(x) (void)(x)

#endif
