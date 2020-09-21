/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MNOC_UTIL_H__
#define __MNOC_UTIL_H__

#include <linux/types.h>
#include <linux/platform_device.h>

/*
 * BIT Operation
 */
#undef  BIT
#define BIT(_bit_) (unsigned int)(1 << (_bit_))
#define BITS(_bits_, _val_) ((((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define BITMASK(_bits_) (((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
& ~((1U << ((0) ? _bits_)) - 1))
#define GET_BITS_VAL(_bits_, _val_) (((_val_) & \
(BITMASK(_bits_))) >> ((0) ? _bits_))


/**
 * Read/Write a field of a register.
 * @addr:       Address of the register
 * @range:      The field bit range in the form of MSB:LSB
 * @val:        The value to be written to the field
 */
//#define mnoc_read(addr)	ioread32((void*) (uintptr_t) addr)
#define mnoc_read(addr)	ioread32((void __iomem *) (uintptr_t) (addr))

#define mnoc_write(addr,  val) \
iowrite32(val, (void __iomem *) (uintptr_t) addr)
#define mnoc_read_field(addr, range) GET_BITS_VAL(range, mnoc_read(addr))
#define mnoc_write_field(addr, range, val) mnoc_write(addr, (mnoc_read(addr) \
& ~(BITMASK(range))) | BITS(range, val))
#define mnoc_set_bit(addr, set) mnoc_write(addr, (mnoc_read(addr) | (set)))
#define mnoc_clr_bit(addr, clr) mnoc_write(addr, (mnoc_read(addr) & ~(clr)))



/* platform */
const struct of_device_id *mnoc_util_get_device_id(void);

void infra2apu_sram_en(void);
void infra2apu_sram_dis(void);

void apu2infra_bus_protect_en(void);
void apu2infra_bus_protect_dis(void);


#endif /* __MNOC_UTIL_H__ */
