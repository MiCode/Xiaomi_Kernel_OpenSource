/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
