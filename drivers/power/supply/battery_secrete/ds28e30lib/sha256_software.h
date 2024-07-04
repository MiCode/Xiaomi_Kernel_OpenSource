
/*******************************************************************************
* Copyright (C) 2013 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated 
* Products, Inc. shall not be used except as stated in the Maxim Integrated 
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all 
* ownership rights.
*******************************************************************************
*/

/** @file sha256_software.h
*  @brief Include file for sha256_software.c
*/

#ifndef __SHA_H_
#define __SHA_H_

#define DEBUG_SHA
#include <linux/types.h>

// Generic SHA-256 Function
extern void ComputeSHA256(char* message, short length, ushort skipconst, ushort reverse, char* digest);
// Maxim specific SHA-256 MAC Functions
extern void Maxim_SetMACSecret(char *secret);
extern void Maxim_ComputeMAC256(char* MT, int length, char* MAC);
extern int Maxim_VerifyMAC256(char* MT, int length, char* compare_MAC);
extern void Maxim_CalculateNextSecret256(char* binding, char* partial, int page_num, char* ROM_NO, char* manid);


#endif
