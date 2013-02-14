/* Copyright (c) 2010,2011 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PM8XXX_UPL_H__
#define __PM8XXX_UPL_H__

struct pm8xxx_upl_device;

#define PM8XXX_UPL_DEV_NAME		"pm8xxx-upl"

/* control masks and flags */
#define PM8XXX_UPL_MOD_ENABLE_MASK	(0x10)
#define PM8XXX_UPL_MOD_ENABLE		(0x10)
#define PM8XXX_UPL_MOD_DISABLE		(0x00)

#define PM8XXX_UPL_OUT_DTEST_MASK	(0xE0)
#define PM8XXX_UPL_OUT_GPIO_ONLY	(0x00)
#define PM8XXX_UPL_OUT_DTEST_1		(0x80)
#define PM8XXX_UPL_OUT_DTEST_2		(0xA0)
#define PM8XXX_UPL_OUT_DTEST_3		(0xC0)
#define PM8XXX_UPL_OUT_DTEST_4		(0xE0)

#define PM8XXX_UPL_IN_A_MASK		(0x01)
#define PM8XXX_UPL_IN_A_GPIO		(0x00)
#define PM8XXX_UPL_IN_A_DTEST		(0x01)
#define PM8XXX_UPL_IN_B_MASK		(0x02)
#define PM8XXX_UPL_IN_B_GPIO		(0x00)
#define PM8XXX_UPL_IN_B_DTEST		(0x02)
#define PM8XXX_UPL_IN_C_MASK		(0x04)
#define PM8XXX_UPL_IN_C_GPIO		(0x00)
#define PM8XXX_UPL_IN_C_DTEST		(0x04)
#define PM8XXX_UPL_IN_D_MASK		(0x08)
#define PM8XXX_UPL_IN_D_GPIO		(0x00)
#define PM8XXX_UPL_IN_D_DTEST		(0x08)

/*
 * pm8xxx_upl_request - request a handle to access UPL device
 */
struct pm8xxx_upl_device *pm8xxx_upl_request(void);

int pm8xxx_upl_read_truthtable(struct pm8xxx_upl_device *upldev,
				u16 *truthtable);

int pm8xxx_upl_write_truthtable(struct pm8xxx_upl_device *upldev,
				u16 truthtable);

/*
 * pm8xxx_upl_config - configure UPL I/O settings and UPL enable/disable
 *
 * @upldev: the UPL device
 * @mask: setting mask to configure
 * @flags: setting flags
 */
int pm8xxx_upl_config(struct pm8xxx_upl_device *upldev, u32 mask, u32 flags);

#endif /* __PM8XXX_UPL_H__ */
