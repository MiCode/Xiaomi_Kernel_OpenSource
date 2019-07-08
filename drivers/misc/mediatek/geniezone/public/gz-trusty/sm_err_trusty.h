/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_SM_ERR_H
#define __LINUX_TRUSTY_SM_ERR_H

/* Errors from the secure monitor */

/* Unknown SMC (defined by ARM DEN 0028A(0.9.0) */
#define SM_ERR_UNDEFINED_SMC		0xFFFFFFFF
#define SM_ERR_INVALID_PARAMETERS	-2
/* Got interrupted. Call back with restart SMC */
#define SM_ERR_INTERRUPTED		-3
/* Got an restart SMC when we didn't expect it */
#define SM_ERR_UNEXPECTED_RESTART	-4
/* Temporarily busy. Call back with original args */
#define SM_ERR_BUSY			-5
/* Got a trusted_service SMC when a restart SMC is required */
#define SM_ERR_INTERLEAVED_SMC		-6
/* Unknown error */
#define SM_ERR_INTERNAL_FAILURE		-7
#define SM_ERR_NOT_SUPPORTED		-8
/* SMC call not allowed */
#define SM_ERR_NOT_ALLOWED		-9
#define SM_ERR_END_OF_INPUT		-10
/* Secure OS crashed */
#define SM_ERR_PANIC			-11
/* Got interrupted by FIQ. Call back with SMC_SC_RESTART_FIQ on same CPU */
#define SM_ERR_FIQ_INTERRUPTED		-12
/* SMC call waiting for another CPU */
#define SM_ERR_CPU_IDLE			-13
/* Got interrupted. Call back with new SMC_SC_NOP */
#define SM_ERR_NOP_INTERRUPTED		-14
/* Cpu idle after SMC_SC_NOP (not an error) */
#define SM_ERR_NOP_DONE			-15

#endif
