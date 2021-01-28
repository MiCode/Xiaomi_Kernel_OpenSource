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

/* An example test TA implementation.
 */

#ifndef __TRUSTZONE_TA_TEST__
#define __TRUSTZONE_TA_TEST__

#define TZ_TA_TEST_UUID "0d5fe516-821d-11e2-bdb4-d485645c4310"

/* Data Structure for Test TA */
/* You should define data structure used both in REE/TEE here
 * N/A for Test TA
 */

/* Command for Test TA */
#define TZCMD_TEST_ADD 0
#define TZCMD_TEST_MUL 1
#define TZCMD_TEST_ADD_MEM 2
#define TZCMD_TEST_DO_A 3
#define TZCMD_TEST_DO_B 4
#define TZCMD_TEST_SLEEP 5
#define TZCMD_ECHO_MEM_TEST 6
#define TZCMD_TEST_DO_C 7
#define TZCMD_TEST_DO_D 8
#define TZCMD_TEST_SECUREFUNC 9
#define TZCMD_TEST_CP_SBUF2NBUF 10
#define TZCMD_TEST_CP_NBUF2SBUF 11
#define TZCMD_TEST_THREAD 12

#define TZCMD_TEST_SYSCALL 0x1234
#define TZCMD_GET_MUTEX 0x2345
#define TZCMD_SEND_MUTEX 0x2346
#define TZCMD_TEST_MUTEX 0x2347

#define TZCMD_MEM_MUL_TEST 0x6000
#define TZCMD_DMA_TEST 0x6100
#define TZCMD_SHARED_MEM_TEST 0x5588

#define TZCMD_ABORT_TEST 0x9876

#endif /* __TRUSTZONE_TA_TEST__ */
