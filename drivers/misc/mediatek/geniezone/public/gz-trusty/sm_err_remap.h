/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __LINUX_TRUSTY_SM_ERR_REMAP_H
#define __LINUX_TRUSTY_SM_ERR_REMAP_H

/****************************************************/
/********** UN-CHANGED (SAME AS TRUSTY) *************/
/****************************************************/
#define SM_ERR_UNDEFINED_SMC        0xFFFFFFFF
#define SM_ERR_INVALID_PARAMETERS   -2
#define SM_ERR_INTERLEAVED_SMC      -6
#define SM_ERR_INTERNAL_FAILURE     -7
#define SM_ERR_NOT_SUPPORTED        -8
#define SM_ERR_NOT_ALLOWED          -9
#define SM_ERR_END_OF_INPUT         -10
#define SM_ERR_PANIC                -11
#define SM_ERR_FIQ_INTERRUPTED      -12

/****************************************************/
/********** REMAPPED ERRORS *************************/
/****************************************************/
#define SM_ERR_INTERRUPTED          SM_ERR_GZ_INTERRUPTED
#define SM_ERR_UNEXPECTED_RESTART   SM_ERR_GZ_UNEXPECTED_RESTART
#define SM_ERR_BUSY                 SM_ERR_GZ_BUSY
#define SM_ERR_CPU_IDLE             SM_ERR_GZ_CPU_IDLE
#define SM_ERR_NOP_INTERRUPTED      SM_ERR_GZ_NOP_INTERRUPTED
#define SM_ERR_NOP_DONE             SM_ERR_GZ_NOP_DONE

#endif /* __LINUX_TRUSTY_SM_ERR_REMAP_H */
