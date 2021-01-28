/*
 * Copyright (c) 2013 Google Inc. All rights reserved
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
