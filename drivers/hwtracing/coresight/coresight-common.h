/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CORESIGHT_COMMON_H
#define _CORESIGHT_COMMON_H

#define BM(lsb, msb)		((BIT(msb) - BIT(lsb)) + BIT(msb))
#define BVAL(val, n)            ((val & BIT(n)) >> n)

#endif
