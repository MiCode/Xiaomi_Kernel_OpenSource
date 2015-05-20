/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_APIC_H_
#define _VTSS_APIC_H_

#include <linux/irq.h>

/**
 * Interrupt vector for PMU overflow event
 *
 *     Choose the highest unused IDT vector possible so that our
 *     callback routine runs at the highest priority allowed;
 *     must avoid using pre-defined vectors in,
 *              include/asm/irq.h
 *              include/asm/hw_irq.h
 *              include/asm/irq_vectors.h
 */
/* FIRST_DEVICE_VECTOR should be valid for kernels 2.6.33 and earlier */
#if defined(FIRST_DEVICE_VECTOR)
#define CPU_PERF_VECTOR     (FIRST_DEVICE_VECTOR - 1)
/* FIRST_EXTERNAL_VECTOR should be valid for kernels 2.6.34 and later */
#else
#define CPU_PERF_VECTOR     (FIRST_EXTERNAL_VECTOR + 1)
#endif

/* Has the APIC Been enabled */
#define VTSS_APIC_BASE_GLOBAL_ENABLED(a)  ((a) & 1 << 11)
#define VTSS_APIC_VIRTUAL_WIRE_ENABLED(a) ((a) & 0x100)

/* APIC control functions */
void vtss_pmi_enable(void);
void vtss_pmi_disable(void);
void vtss_apic_ack_eoi(void);
int  vtss_apic_read_priority(void);
void vtss_apic_init(void);
void vtss_apic_fini(void);
int vtss_apic_map(void);
void vtss_apic_unmap(void);

#endif /* _VTSS_APIC_H_ */
