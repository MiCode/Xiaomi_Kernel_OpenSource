/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/threads.h>

#if (NR_CPUS > 2)
#error code only tested for 1 or 2 CPUs.
#endif

/* Add 1 slot to store the register count for ETM state */
#define MAX_ETM_REGS (56 + 1)
#define MAX_ETM_STATE_SIZE (MAX_ETM_REGS * 4)

.global etm_read_reg
etm_read_reg:
    cmp     r0, #0x7F
    bhi     read_high
    ldr     r2, =read_table
    add     r2, r2, r0, lsl #3
    mov     pc, r2

read_high:
    cmp     r0, #0xC1
    beq     read_c1
    cmp     r0, #0xC4
    beq     read_c4
    cmp     r0, #0xC5
    beq     read_c5
    mov     r0, #0
    bx      lr

read_c1:
    mrc     p14, 1, r0, c1,  c1, 4  // register 0xC1 (OS Lock Status Reg)
    bx      lr

read_c4:
    mrc     p14, 1, r0, c1,  c4, 4  // register 0xC4 (Powerdown Ctl Reg)
    bx      lr

read_c5:
    mrc     p14, 1, r0, c1,  c5, 4  // register 0xC5 (Powerdown Status Reg)
    bx      lr

read_table:
    mrc     p14, 1, r0, c0,  c0, 0  // register 0x00
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 0  // register 0x01
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 0  // register 0x02
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 0  // register 0x03
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 0  // register 0x04
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 0  // register 0x05
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 0  // register 0x06
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 0  // register 0x07
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 0  // register 0x08
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 0  // register 0x09
    bx      lr
    mrc     p14, 1, r0, c0, c10, 0  // register 0x0A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 0  // register 0x0B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 0  // register 0x0C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 0  // register 0x0D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 0  // register 0x0E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 0  // register 0x0F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 1  // register 0x10
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 1  // register 0x11
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 1  // register 0x12
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 1  // register 0x13
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 1  // register 0x14
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 1  // register 0x15
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 1  // register 0x16
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 1  // register 0x17
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 1  // register 0x18
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 1  // register 0x19
    bx      lr
    mrc     p14, 1, r0, c0, c10, 1  // register 0x1A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 1  // register 0x1B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 1  // register 0x1C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 1  // register 0x1D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 1  // register 0x1E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 1  // register 0x1F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 2  // register 0x20
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 2  // register 0x21
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 2  // register 0x22
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 2  // register 0x23
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 2  // register 0x24
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 2  // register 0x25
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 2  // register 0x26
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 2  // register 0x27
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 2  // register 0x28
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 2  // register 0x29
    bx      lr
    mrc     p14, 1, r0, c0, c10, 2  // register 0x2A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 2  // register 0x2B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 2  // register 0x2C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 2  // register 0x2D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 2  // register 0x2E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 2  // register 0x2F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 3  // register 0x30
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 3  // register 0x31
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 3  // register 0x32
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 3  // register 0x33
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 3  // register 0x34
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 3  // register 0x35
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 3  // register 0x36
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 3  // register 0x37
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 3  // register 0x38
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 3  // register 0x39
    bx      lr
    mrc     p14, 1, r0, c0, c10, 3  // register 0x3A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 3  // register 0x3B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 3  // register 0x3C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 3  // register 0x3D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 3  // register 0x3E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 3  // register 0x3F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 4  // register 0x40
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 4  // register 0x41
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 4  // register 0x42
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 4  // register 0x43
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 4  // register 0x44
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 4  // register 0x45
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 4  // register 0x46
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 4  // register 0x47
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 4  // register 0x48
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 4  // register 0x49
    bx      lr
    mrc     p14, 1, r0, c0, c10, 4  // register 0x4A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 4  // register 0x4B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 4  // register 0x4C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 4  // register 0x4D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 4  // register 0x4E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 4  // register 0x4F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 5  // register 0x50
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 5  // register 0x51
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 5  // register 0x52
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 5  // register 0x53
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 5  // register 0x54
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 5  // register 0x55
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 5  // register 0x56
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 5  // register 0x57
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 5  // register 0x58
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 5  // register 0x59
    bx      lr
    mrc     p14, 1, r0, c0, c10, 5  // register 0x5A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 5  // register 0x5B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 5  // register 0x5C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 5  // register 0x5D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 5  // register 0x5E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 5  // register 0x5F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 6  // register 0x60
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 6  // register 0x61
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 6  // register 0x62
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 6  // register 0x63
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 6  // register 0x64
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 6  // register 0x65
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 6  // register 0x66
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 6  // register 0x67
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 6  // register 0x68
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 6  // register 0x69
    bx      lr
    mrc     p14, 1, r0, c0, c10, 6  // register 0x6A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 6  // register 0x6B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 6  // register 0x6C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 6  // register 0x6D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 6  // register 0x6E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 6  // register 0x6F
    bx      lr
    mrc     p14, 1, r0, c0,  c0, 7  // register 0x70
    bx      lr
    mrc     p14, 1, r0, c0,  c1, 7  // register 0x71
    bx      lr
    mrc     p14, 1, r0, c0,  c2, 7  // register 0x72
    bx      lr
    mrc     p14, 1, r0, c0,  c3, 7  // register 0x73
    bx      lr
    mrc     p14, 1, r0, c0,  c4, 7  // register 0x74
    bx      lr
    mrc     p14, 1, r0, c0,  c5, 7  // register 0x75
    bx      lr
    mrc     p14, 1, r0, c0,  c6, 7  // register 0x76
    bx      lr
    mrc     p14, 1, r0, c0,  c7, 7  // register 0x77
    bx      lr
    mrc     p14, 1, r0, c0,  c8, 7  // register 0x78
    bx      lr
    mrc     p14, 1, r0, c0,  c9, 7  // register 0x79
    bx      lr
    mrc     p14, 1, r0, c0, c10, 7  // register 0x7A
    bx      lr
    mrc     p14, 1, r0, c0, c11, 7  // register 0x7B
    bx      lr
    mrc     p14, 1, r0, c0, c12, 7  // register 0x7C
    bx      lr
    mrc     p14, 1, r0, c0, c13, 7  // register 0x7D
    bx      lr
    mrc     p14, 1, r0, c0, c14, 7  // register 0x7E
    bx      lr
    mrc     p14, 1, r0, c0, c15, 7  // register 0x7F
    bx      lr

.global etm_write_reg
etm_write_reg:
    cmp     r0, #0x7F
    bhi     write_high
    ldr     r2, =write_table
    add     r2, r2, r0, lsl #3
    mov     pc, r2

write_high:
    cmp     r0, #0xC0
    beq     write_c0
    cmp     r0, #0xC4
    beq     write_c4
    bx      lr

write_c0:
    mcr     p14, 1, r1, c1,  c0, 4  // register 0xC0 (OS Lock Access Reg)
    bx      lr

write_c4:
    mcr     p14, 1, r1, c1,  c4, 4  // register 0xC4 (Powerdown Ctl Reg)
    bx      lr

write_table:
    mcr     p14, 1, r1, c0,  c0, 0  // register 0x00
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 0  // register 0x01
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 0  // register 0x02
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 0  // register 0x03
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 0  // register 0x04
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 0  // register 0x05
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 0  // register 0x06
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 0  // register 0x07
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 0  // register 0x08
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 0  // register 0x09
    bx      lr
    mcr     p14, 1, r1, c0, c10, 0  // register 0x0A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 0  // register 0x0B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 0  // register 0x0C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 0  // register 0x0D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 0  // register 0x0E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 0  // register 0x0F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 1  // register 0x10
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 1  // register 0x11
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 1  // register 0x12
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 1  // register 0x13
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 1  // register 0x14
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 1  // register 0x15
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 1  // register 0x16
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 1  // register 0x17
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 1  // register 0x18
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 1  // register 0x19
    bx      lr
    mcr     p14, 1, r1, c0, c10, 1  // register 0x1A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 1  // register 0x1B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 1  // register 0x1C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 1  // register 0x1D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 1  // register 0x1E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 1  // register 0x1F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 2  // register 0x20
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 2  // register 0x21
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 2  // register 0x22
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 2  // register 0x23
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 2  // register 0x24
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 2  // register 0x25
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 2  // register 0x26
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 2  // register 0x27
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 2  // register 0x28
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 2  // register 0x29
    bx      lr
    mcr     p14, 1, r1, c0, c10, 2  // register 0x2A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 2  // register 0x2B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 2  // register 0x2C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 2  // register 0x2D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 2  // register 0x2E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 2  // register 0x2F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 3  // register 0x30
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 3  // register 0x31
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 3  // register 0x32
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 3  // register 0x33
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 3  // register 0x34
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 3  // register 0x35
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 3  // register 0x36
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 3  // register 0x37
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 3  // register 0x38
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 3  // register 0x39
    bx      lr
    mcr     p14, 1, r1, c0, c10, 3  // register 0x3A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 3  // register 0x3B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 3  // register 0x3C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 3  // register 0x3D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 3  // register 0x3E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 3  // register 0x3F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 4  // register 0x40
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 4  // register 0x41
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 4  // register 0x42
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 4  // register 0x43
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 4  // register 0x44
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 4  // register 0x45
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 4  // register 0x46
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 4  // register 0x47
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 4  // register 0x48
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 4  // register 0x49
    bx      lr
    mcr     p14, 1, r1, c0, c10, 4  // register 0x4A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 4  // register 0x4B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 4  // register 0x4C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 4  // register 0x4D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 4  // register 0x4E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 4  // register 0x4F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 5  // register 0x50
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 5  // register 0x51
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 5  // register 0x52
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 5  // register 0x53
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 5  // register 0x54
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 5  // register 0x55
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 5  // register 0x56
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 5  // register 0x57
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 5  // register 0x58
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 5  // register 0x59
    bx      lr
    mcr     p14, 1, r1, c0, c10, 5  // register 0x5A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 5  // register 0x5B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 5  // register 0x5C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 5  // register 0x5D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 5  // register 0x5E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 5  // register 0x5F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 6  // register 0x60
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 6  // register 0x61
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 6  // register 0x62
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 6  // register 0x63
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 6  // register 0x64
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 6  // register 0x65
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 6  // register 0x66
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 6  // register 0x67
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 6  // register 0x68
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 6  // register 0x69
    bx      lr
    mcr     p14, 1, r1, c0, c10, 6  // register 0x6A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 6  // register 0x6B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 6  // register 0x6C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 6  // register 0x6D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 6  // register 0x6E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 6  // register 0x6F
    bx      lr
    mcr     p14, 1, r1, c0,  c0, 7  // register 0x70
    bx      lr
    mcr     p14, 1, r1, c0,  c1, 7  // register 0x71
    bx      lr
    mcr     p14, 1, r1, c0,  c2, 7  // register 0x72
    bx      lr
    mcr     p14, 1, r1, c0,  c3, 7  // register 0x73
    bx      lr
    mcr     p14, 1, r1, c0,  c4, 7  // register 0x74
    bx      lr
    mcr     p14, 1, r1, c0,  c5, 7  // register 0x75
    bx      lr
    mcr     p14, 1, r1, c0,  c6, 7  // register 0x76
    bx      lr
    mcr     p14, 1, r1, c0,  c7, 7  // register 0x77
    bx      lr
    mcr     p14, 1, r1, c0,  c8, 7  // register 0x78
    bx      lr
    mcr     p14, 1, r1, c0,  c9, 7  // register 0x79
    bx      lr
    mcr     p14, 1, r1, c0, c10, 7  // register 0x7A
    bx      lr
    mcr     p14, 1, r1, c0, c11, 7  // register 0x7B
    bx      lr
    mcr     p14, 1, r1, c0, c12, 7  // register 0x7C
    bx      lr
    mcr     p14, 1, r1, c0, c13, 7  // register 0x7D
    bx      lr
    mcr     p14, 1, r1, c0, c14, 7  // register 0x7E
    bx      lr
    mcr     p14, 1, r1, c0, c15, 7  // register 0x7F
    bx      lr

.global l2tevselr0_write
l2tevselr0_write:
    mcr     p15, 3, r0, c15, c5, 2
    bx      lr

.global etm_save_reg
etm_save_reg:
	ldr     r3, =etm_state        /* store state at etm_state */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5 /* MPIDR */
	and	r2, r2, #15	      /* What CPU am I */
	ldr     r1, =MAX_ETM_STATE_SIZE
	mul     r2, r2, r1
	add	r3, r3, r2
#endif

	/* save etm state */
	mrc	p14, 1, r0, c1, c5, 4 /* read ETM PDSR to clear sticky bit */
	isb
	ldr     r1, =0xc5ACCE55       /* set ETMOSLAR lock */
	mcr     p14, 1, r1, c1, c0, 4
	isb

	mrc     p14, 1, r1, c1, c2, 4 /* ETMOSSRR state register count */
	cmp     r1, #(MAX_ETM_REGS)   /* check for state overflow */
	movgt   r1, #0                /* if not enough space, don't save */
	str     r1,[r3],#4            /* save count for restore */

1:	cmp     r1, #0
	mrcne   p14, 1, r2, c1, c2, 4 /* ETMOSSRR state value */
	strne   r2, [r3], #4          /* push value */
	subne   r1, r1, #1
	bne     1b

	mcr     p14, 1, r1, c1, c0, 4 /* r1 = 0, unlock ETMOSLAR */
	isb

	bx      lr

.global etm_restore_reg
etm_restore_reg:
	/* restore debug registers after power collapse  */
	ldr     r3, =etm_state        /* load state from etm_state */
#if (NR_CPUS >= 2)
	mrc	p15, 0, r2, c0, c0, 5 /* MPIDR */
	and	r2, r2, #15	      /* What CPU am I */
	ldr     r1, =MAX_ETM_STATE_SIZE
	mul     r2, r2, r1
	add	r3, r3, r2
#endif

	/* restore etm state */
	mrc	p14, 1, r0, c1, c5, 4 /* read ETM PDSR to clear sticky bit */
	isb
	ldr     r1, =0xc5ACCE55       /* set ETMOSLAR lock */
	mcr     p14, 1, r1, c1, c0, 4
	isb

	mrc     p14, 1, r1, c1, c2, 4 /* ETMOSSRR dummy read (required)*/
	ldr     r1, [r3], #4          /* load saved count */
	cmp     r1, #0                /* skip if none stored */
	beq     end_etm_restore_reg

1:	ldr     r2,[r3],#4
	mcr     p14, 1, r2, c1, c2, 4 /* ETMOSSRR write state value */
	subs    r1, r1, #1
	bne     1b
end_etm_restore_reg:
	mcr     p14, 1, r1, c1, c0, 4 /* r1 = 0, unlock ETMOSLAR */
	isb

	bx      lr


	.data

etm_state:
	.space	MAX_ETM_STATE_SIZE * NR_CPUS
