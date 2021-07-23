/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef SECURITY_AO_H
#define SECURITY_AO_H

/******************************************************************************
 * CHIP SELECTION
 ******************************************************************************/
extern void __iomem *security_ao_base;

/******************************************************************************
 * MACROS DEFINITIONS
 ******************************************************************************/

/* miscx index*/
#define BOOT_MISC2_IDX (2)

#define RST_CON_BIT(idx) (0x1 << idx)

/* boot misc2 flags */
#define BOOT_MISC2_VERITY_ERR (0x1 << 0)

/******************************************************************************
 * HARDWARE DEFINITIONS
 ******************************************************************************/
#define MISC_LOCK_KEY_MAGIC    (0x0000ad98)

#define BOOT_MISC2       (security_ao_base + 0x088)
#define MISC_LOCK_KEY    (security_ao_base + 0x100)
#define RST_CON          (security_ao_base + 0x108)

#endif /* SECURITY_AO_H */
