/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __HV_ERR_H__
#define __HV_ERR_H__

/****************************************************/
/********** Generic Errors **************************/
/****************************************************/
#define SM_ERR_UNK                  (0xFFFFFFFF)

/****************************************************/
/********** GZ Specific Errors **********************/
/****************************************************/
#define SM_ERR_GZ_MIN_CODE              (-1000)
#define SM_ERR_GZ_INTERRUPTED           (-1001)
#define SM_ERR_GZ_UNEXPECTED_RESTART    (-1002)
#define SM_ERR_GZ_BUSY                  (-1003)
#define SM_ERR_GZ_CPU_IDLE              (-1004)
#define SM_ERR_GZ_NOP_INTERRUPTED       (-1005)
#define SM_ERR_GZ_NOP_DONE              (-1006)
#define SM_ERR_GZ_MAX_CODE              (-1999)

/****************************************************/
/********** VMM Guest OS Specific Errors ************/
/****************************************************/
#define SM_ERR_VM_MIN_CODE              (-2000)
#define SM_ERR_VM_INTERRUPTED           (-2001)
#define SM_ERR_VM_UNEXPECTED_RESTART    (-2002)
#define SM_ERR_VM_BUSY                  (-2003)
#define SM_ERR_VM_CPU_IDLE              (-2004)
#define SM_ERR_VM_NOP_INTERRUPTED       (-2005)
#define SM_ERR_VM_NOP_DONE              (-2006)
#define SM_ERR_VM_MAX_CODE              (-2999)

#endif /* end of __HV_ERR_H__ */
