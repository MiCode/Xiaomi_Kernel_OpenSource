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

#ifndef __CMDQSECTL_API_H__
#define __CMDQSECTL_API_H__

/**
 * Command IDs for normal world(TLC or linux kernel) to Trustlet
 */
#define CMD_CMDQ_TL_SUBMIT_TASK	  1
/* (not used)release resource in secure path per session */
#define CMD_CMDQ_TL_RES_RELEASE	  2
#define CMD_CMDQ_TL_CANCEL_TASK	  3
/* create global resource for secure path */
#define CMD_CMDQ_TL_PATH_RES_ALLOCATE 4
/* destroy globacl resource for secure path */
#define CMD_CMDQ_TL_PATH_RES_RELEASE  5
/* create shared memory in Normal and Secure world */
#define CMD_CMDQ_TL_INIT_SHARED_MEMORY 6
/* register secure irq */
#define CMD_CMDQ_TL_REGISTER_SECURE_IRQ 8
#define CMD_CMDQ_TL_DUMP_SMI_LARB	9

/* entry cmdqSecTl, and do nothing */
#define CMD_CMDQ_TL_TEST_HELLO_TL	(4000)
/* entry cmdqSecTl and cmdqSecDr, and do nothing */
#define CMD_CMDQ_TL_TEST_DUMMY	  (4001)
#define CMD_CMDQ_TL_TEST_SMI_DUMP	(4002)
#define CMD_CMDQ_TL_TRAP_DR_INFINITELY (4004)
#define CMD_CMDQ_TL_DUMP (4005)

#define CMD_CMDQ_TL_SECTRACE_MAP      (3000)
#define CMD_CMDQ_TL_SECTRACE_UNMAP    (3001)
#define CMD_CMDQ_TL_SECTRACE_TRANSACT (3002)


/**
 * Termination codes
 */
#define EXIT_ERROR				  ((uint32_t)(-1))

/**
 * TCI message data: see cmdq_sec_iwc_common.h
 */

/**
 * Trustlet UUID:
 * filename of output bin is {TL_UUID}.tlbin
 */
#ifndef CONFIG_MTK_CMDQ_TAB
#define TL_CMDQ_UUID { { 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
#else
#define TZ_TA_CMDQ_NAME "CMDQ_TA"
#define TZ_TA_CMDQ_UUID "5c071864-505d-11e4-9e35-164230d1df67"
#endif

#endif /*__CMDQSECTEST_API_H__*/
