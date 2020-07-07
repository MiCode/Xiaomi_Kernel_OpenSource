/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


#ifndef __SM_ERR_MTEE_H__
#define __SM_ERR_MTEE_H__

/****************************************************/
/********** Generic Errors **************************/
/****************************************************/

/* Unknown SMC (defined by ARM DEN 0028A(0.9.0) */
#define SM_ERR_UNDEFINED_SMC		(0xFFFFFFFF)
#define SM_ERR_UNK			(0xFFFFFFFF)

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

/****************************************************/
/********** GZ Specific Errors **********************/
/****************************************************/
#define SM_ERR_GZ_MIN_CODE		(-1000)
#define SM_ERR_GZ_INTERRUPTED		(-1001)
#define SM_ERR_GZ_UNEXPECTED_RESTART	(-1002)
#define SM_ERR_GZ_BUSY			(-1003)
#define SM_ERR_GZ_CPU_IDLE		(-1004)
#define SM_ERR_GZ_NOP_INTERRUPTED	(-1005)
#define SM_ERR_GZ_NOP_DONE		(-1006)
#define SM_ERR_GZ_MAX_CODE		(-1999)

/****************************************************/
/********** VMM Guest OS Specific Errors ************/
/****************************************************/
#define SM_ERR_NBL_MIN_CODE		(-2000)
#define SM_ERR_NBL_INTERRUPTED		(-2001)
#define SM_ERR_NBL_UNEXPECTED_RESTART	(-2002)
#define SM_ERR_NBL_BUSY			(-2003)
#define SM_ERR_NBL_CPU_IDLE		(-2004)
#define SM_ERR_NBL_NOP_INTERRUPTED	(-2005)
#define SM_ERR_NBL_NOP_DONE		(-2006)
#define SM_ERR_NBL_MAX_CODE		(-2999)

/****************************************************/
/********** REMAPPED VM ERRORS **********************/
/****************************************************/
#define SM_ERR_NBL_NATIVE_INTERRUPTED		-3
#define SM_ERR_NBL_NATIVE_UNEXPECTED_RESTART	-4
#define SM_ERR_NBL_NATIVE_BUSY			-5
#define SM_ERR_NBL_NATIVE_CPU_IDLE		-13
#define SM_ERR_NBL_NATIVE_NOP_INTERRUPTED	-14
#define SM_ERR_NBL_NATIVE_NOP_DONE		-15

#endif /* end of __SM_ERR_MTEE_H__ */
