/*********************************************************************
 *
 * Copyright (C) 2006 Intel Corp
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel
 * Corporation or its suppliers and licensors. Title to the Material
 * remains with Intel Corporation or its suppliers and licensors. The
 * Material contains trade secrets and proprietary and confidential
 * information of Intel or its suppliers and licensors. The Material
 * is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express
 * written permission.
 *
 * Unless otherwise expressly permitted by Intel in a separate license
 * agreement, use of the Material is subject to the copyright notices,
 * trademarks, warranty, use, and disclosure restrictions reflected on
 * the outside of the media, in the documents themselves, and in the
 * "About" or "Read Me" or similar file contained within this source
 * code, and identified as (name of the file) . Unless otherwise
 * expressly agreed by Intel in writing, you may not remove or alter
 * such notices in any way.
 *
 *******************************************************************/

/* Linux-based compiler-dependent definitions. */

#ifndef __INCLUDE__OS__LINUX__IA32_H__
#define __INCLUDE__OS__LINUX__IA32_H__
#include <linux/types.h>
/**
 * Issue the CPUID instruction with special parameters.
 * @param leaf  CPUID leaf (i.e., content of eax)
 * @param b     ebx
 * @param c     ecx
 * @param d     edx
 * @param S     esi
 * @param D     edi
 * @returns an application-specific status.
 * It is expected that this function be used in conjunction with VX2.
 * In particular the currently encoded instruction stream does not
 * provide the typical interface functionality: values returned in
 * ebx, ecx, and edx are ignored.
 */
inline int cpuid (uint32_t leaf, uint32_t b_val = 0, uint32_t c = 0, uint32_t d = 0, uint32_t S = 0, uint32_t D = 0)
{
    int status;
    printk(KERN_ERR"entry cpuid %x %x %x %x %x %x \n", leaf, b_val, c, d, S, D);

    __asm__ __volatile__ (
        "push %%ebx\n mov %1,%%ebx\n cpuid\n mov %%ebx,%1\n pop %%ebx\n"
        : "=a" (status), "+g" (b_val), "+c" (c), "+d" (d)
        : "0" (leaf), "m" (b_val), "S" (S), "D" (D)
        : );

    printk(KERN_ERR"exit cpuid status (%x) %x %x %x %x %x %x", status, leaf, b_val, c, d, S, D);
    return status;
}

inline uint64_t rdtsc()
{
    uint32_t a, d;
        
    asm volatile ("rdtsc"
                  : "=a"(a), "=d"(d));
    return ((uint64_t)d << 32) | (uint64_t)a;
}

inline void vmfunc_emul(uint32_t a = 0, uint32_t c = 0)
{
    asm volatile ("vmcall"
                  : : "a"(a), "c"(c));
}

inline void vmfunc(uint32_t a = 0, uint32_t c = 0)
{

    asm volatile ("nop"
                  : : "a"(a), "c"(c));
    asm volatile (".byte 0x0f");
    asm volatile (".byte 0x01");
    asm volatile (".byte 0xd4");
}

#endif /* __INCLUDE__OS__LINUX__IA32_H__ */
