/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_SPARSEMEM_H
#define __ASM_SPARSEMEM_H

#ifdef CONFIG_SPARSEMEM
#define MAX_PHYSMEM_BITS	CONFIG_ARM64_PA_BITS
#ifndef CONFIG_MEMORY_HOTPLUG
#define SECTION_SIZE_BITS	30
#else
#define SECTION_SIZE_BITS	CONFIG_HOTPLUG_SIZE_BITS
#endif
#endif

#endif
