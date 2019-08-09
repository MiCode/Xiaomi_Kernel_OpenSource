/*************************************************************************/ /*!
@File
@Title          Kernel/User mode general purpose shared memory.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    General purpose shared memory (i.e. information page) mapped by
			    kernel space driver and user space clients. All information page
				entries are sizeof(IMG_UINT32) on both 32/64-bit environments.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef _INFO_PAGE_DEFS_H_
#define _INFO_PAGE_DEFS_H_


/* CacheOp information page entries */
#define CACHEOP_INFO_IDX_START     0x00
#define CACHEOP_INFO_GFSEQNUM0     (CACHEOP_INFO_IDX_START + 0) /*!< Current global flush sequence number */
#define CACHEOP_INFO_GFSEQNUM1     (CACHEOP_INFO_IDX_START + 1) /*!< Validity global flush sequence number */
#define CACHEOP_INFO_UMRBFONLY     (CACHEOP_INFO_IDX_START + 2) /*!< Use UM flush only (i.e no KM GF) */
#define CACHEOP_INFO_UMKMTHRESHLD  (CACHEOP_INFO_IDX_START + 3) /*!< UM=>KM routing threshold in bytes */
#define CACHEOP_INFO_KMGFTHRESHLD  (CACHEOP_INFO_IDX_START + 4) /*!< KM/GF threshold in bytes */
#define CACHEOP_INFO_KMDFTHRESHLD  (CACHEOP_INFO_IDX_START + 5) /*!< KM/DF threshold in bytes */
#define CACHEOP_INFO_LINESIZE      (CACHEOP_INFO_IDX_START + 6) /*!< CPU data cache line size */
#define CACHEOP_INFO_PGSIZE        (CACHEOP_INFO_IDX_START + 7) /*!< CPU MMU page size */

/* HWPerf information page entries */
#define HWPERF_INFO_IDX_START      0x08
#define HWPERF_FILTER_SERVICES_IDX (HWPERF_INFO_IDX_START + 0)
#define HWPERF_FILTER_EGL_IDX      (HWPERF_INFO_IDX_START + 1)
#define HWPERF_FILTER_OPENGLES_IDX (HWPERF_INFO_IDX_START + 2)
#define HWPERF_FILTER_OPENCL_IDX   (HWPERF_INFO_IDX_START + 3)
#define HWPERF_FILTER_OPENRL_IDX   (HWPERF_INFO_IDX_START + 4)
#define HWPERF_FILTER_VULKAN_IDX   (HWPERF_INFO_IDX_START + 5)
#define HWPERF_INFO_IDX_END        (HWPERF_INFO_IDX_START + 6)

/* BVNC of the core */
#define CORE_ID_IDX_START                   (HWPERF_INFO_IDX_END)
#define CORE_ID_BRANCH                      (CORE_ID_IDX_START + 0)
#define CORE_ID_VERSION                     (CORE_ID_IDX_START + 1)
#define CORE_ID_NUMBER_OF_SCALABLE_UNITS    (CORE_ID_IDX_START + 2)
#define CORE_ID_CONFIG                      (CORE_ID_IDX_START + 3)

#endif /* _INFO_PAGE_DEFS_H_ */
