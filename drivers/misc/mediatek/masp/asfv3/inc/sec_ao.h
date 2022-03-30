/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef SEC_AO_H
#define SEC_AO_H

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

#endif /* SEC_AO_H */
