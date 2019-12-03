/*******************************************************************************
* Copyright (C) 2015 Maxim Integrated Products, Inc., All rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
* * This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
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
*     Module Name: SYS
*     Description: performs s
*        Filename: ucl_sys.c
*          Author: LSL
*        Compiler: gcc
*
*******************************************************************************
 */
#include "ucl_config.h"
#include "ucl_defs.h"
#include "ucl_retdefs.h"
#include "ucl_types.h"

#include <ucl_hash.h>
#ifdef HASH_SHA256
#include <ucl_sha256.h>
#endif
#ifdef HASH_SIA256
#include <ucl_sia256.h>
#endif

#ifdef HASH_SHA3
#include <ucl_sha3.h>
#endif

__API__ int hash_size[MAX_HASH_FUNCTIONS];

int __API__ ucl_init(void)
{
#ifdef HASH_SHA256
    hash_size[UCL_SHA256] = UCL_SHA256_HASHSIZE;
#endif
#ifdef HASH_SHA3
    hash_size[UCL_SHA3] = UCL_SHA3_512_HASHSIZE;
#endif
#ifdef HASH_SIA256
    hash_size[UCL_SIA256] = UCL_SIA256_HASHSIZE;
#endif
    return UCL_OK;
}

