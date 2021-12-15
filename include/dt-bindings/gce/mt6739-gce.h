/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DT_BINDINGS_GCE_MT6739_H
#define _DT_BINDINGS_GCE_MT6739_H

/* assign timeout 0 also means default */
#define CMDQ_NO_TIMEOUT		0xffffffff
#define CMDQ_TIMEOUT_DEFAULT	1000

/* GCE thread priority */
#define CMDQ_THR_PRIO_LOWEST	0
#define CMDQ_THR_PRIO_1		1
#define CMDQ_THR_PRIO_2		2
#define CMDQ_THR_PRIO_3		3
#define CMDQ_THR_PRIO_4		4
#define CMDQ_THR_PRIO_5		5
#define CMDQ_THR_PRIO_6		6
#define CMDQ_THR_PRIO_HIGHEST	7

/* GCE subsys table */
#define SUBSYS_1300XXXX		0
#define SUBSYS_1400XXXX		1
#define SUBSYS_1401XXXX		2
#define SUBSYS_1402XXXX		3
#define SUBSYS_1502XXXX		4
#define SUBSYS_1880XXXX		5
#define SUBSYS_1881XXXX		6
#define SUBSYS_1882XXXX		7
#define SUBSYS_1883XXXX		8
#define SUBSYS_1884XXXX		9
#define SUBSYS_1000XXXX		10
#define SUBSYS_1001XXXX		11
#define SUBSYS_1002XXXX		12
#define SUBSYS_1003XXXX		13
#define SUBSYS_1004XXXX		14
#define SUBSYS_1005XXXX		15
#define SUBSYS_1020XXXX		16
#define SUBSYS_1028XXXX		17
#define SUBSYS_1700XXXX		18
#define SUBSYS_1701XXXX		19
#define SUBSYS_1702XXXX		20
#define SUBSYS_1703XXXX		21
#define SUBSYS_1800XXXX		22
#define SUBSYS_1801XXXX		23
#define SUBSYS_1802XXXX		24
#define SUBSYS_1804XXXX		25
#define SUBSYS_1805XXXX		26
#define SUBSYS_1808XXXX		27
#define SUBSYS_180aXXXX		28
#define SUBSYS_180bXXXX		29


/* CMDQ sw tokens
 * Following definitions are gce sw token which may use by clients
 * event operation API.
 * Note that token 512 to 639 may set secure
 */

/* end of hw event and begin of sw token */
#define CMDQ_MAX_HW_EVENT				512

/* Config thread notify trigger thread */
#define CMDQ_SYNC_TOKEN_CONFIG_DIRTY			640
/* Trigger thread notify config thread */
#define CMDQ_SYNC_TOKEN_STREAM_EOF			641
/* Block Trigger thread until the ESD check finishes. */
#define CMDQ_SYNC_TOKEN_ESD_EOF				642
#define CMDQ_SYNC_TOKEN_STREAM_BLOCK			643
/* check CABC setup finish */
#define CMDQ_SYNC_TOKEN_CABC_EOF			644

/* Notify normal CMDQ there are some secure task done
 * MUST NOT CHANGE, this token sync with secure world
 */
#define CMDQ_SYNC_SECURE_THR_EOF			647

/* CMDQ use sw token */
#define CMDQ_SYNC_TOKEN_USER_0				649
#define CMDQ_SYNC_TOKEN_USER_1				650
#define CMDQ_SYNC_TOKEN_POLL_MONITOR			651
#define CMDQ_SYNC_TOKEN_TPR_LOCK			652

/* ISP sw token */
#define CMDQ_SYNC_TOKEN_MSS				665
#define CMDQ_SYNC_TOKEN_MSF				666

/* GPR access tokens (for HW register backup)
 * There are 15 32-bit GPR, 3 GPR form a set
 * (64-bit for address, 32-bit for value)
 * MUST NOT CHANGE, these tokens sync with MDP
 */
#define CMDQ_SYNC_TOKEN_GPR_SET_0			700
#define CMDQ_SYNC_TOKEN_GPR_SET_1			701
#define CMDQ_SYNC_TOKEN_GPR_SET_2			702
#define CMDQ_SYNC_TOKEN_GPR_SET_3			703
#define CMDQ_SYNC_TOKEN_GPR_SET_4			704

/* Resource lock event to control resource in GCE thread */
#define CMDQ_SYNC_RESOURCE_WROT0			710
#define CMDQ_SYNC_RESOURCE_WROT1			711

/* event for gpr timer, used in sleep and poll with timeout */
#define CMDQ_TOKEN_GPR_TIMER_R0				994
#define CMDQ_TOKEN_GPR_TIMER_R1				995
#define CMDQ_TOKEN_GPR_TIMER_R2				996
#define CMDQ_TOKEN_GPR_TIMER_R3				997
#define CMDQ_TOKEN_GPR_TIMER_R4				998
#define CMDQ_TOKEN_GPR_TIMER_R5				999
#define CMDQ_TOKEN_GPR_TIMER_R6				1000
#define CMDQ_TOKEN_GPR_TIMER_R7				1001
#define CMDQ_TOKEN_GPR_TIMER_R8				1002
#define CMDQ_TOKEN_GPR_TIMER_R9				1003
#define CMDQ_TOKEN_GPR_TIMER_R10			1004
#define CMDQ_TOKEN_GPR_TIMER_R11			1005
#define CMDQ_TOKEN_GPR_TIMER_R12			1006
#define CMDQ_TOKEN_GPR_TIMER_R13			1007
#define CMDQ_TOKEN_GPR_TIMER_R14			1008
#define CMDQ_TOKEN_GPR_TIMER_R15			1009

#define CMDQ_EVENT_MAX					0x3FF
/* CMDQ sw tokens END */

#endif
