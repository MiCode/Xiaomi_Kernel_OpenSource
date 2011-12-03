/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PMIC 8xxx driver header file
 *
 */

#ifndef __MFD_PM8XXX_CORE_H
#define __MFD_PM8XXX_CORE_H

#include <linux/mfd/core.h>

enum pm8xxx_version {
	PM8XXX_VERSION_8058,
	PM8XXX_VERSION_8901,
	PM8XXX_VERSION_8921,
	PM8XXX_VERSION_8821,
	PM8XXX_VERSION_8018,
	PM8XXX_VERSION_8922,
};

/* PMIC version specific silicon revisions */
#define PM8XXX_REVISION_8058_TEST	0
#define PM8XXX_REVISION_8058_1p0	1
#define PM8XXX_REVISION_8058_2p0	2
#define PM8XXX_REVISION_8058_2p1	3

#define PM8XXX_REVISION_8901_TEST	0
#define PM8XXX_REVISION_8901_1p0	1
#define PM8XXX_REVISION_8901_1p1	2
#define PM8XXX_REVISION_8901_2p0	3
#define PM8XXX_REVISION_8901_2p1	4

#define PM8XXX_REVISION_8921_TEST	0
#define PM8XXX_REVISION_8921_1p0	1
#define PM8XXX_REVISION_8921_1p1	2
#define PM8XXX_REVISION_8921_2p0	3
#define PM8XXX_REVISION_8921_3p0	4

#define PM8XXX_REVISION_8821_TEST	0
#define PM8XXX_REVISION_8821_1p0	1
#define PM8XXX_REVISION_8821_2p0	2
#define PM8XXX_REVISION_8821_2p1	3

#define PM8XXX_REVISION_8018_TEST	0
#define PM8XXX_REVISION_8018_1p0	1
#define PM8XXX_REVISION_8018_1p1	2
#define PM8XXX_REVISION_8018_2p0	3

#define PM8XXX_REVISION_8922_TEST	0
#define PM8XXX_REVISION_8922_1p0	1
#define PM8XXX_REVISION_8922_1p1	2
#define PM8XXX_REVISION_8922_2p0	3

struct pm8xxx_drvdata {
	int			(*pmic_readb) (const struct device *dev,
						u16 addr, u8 *val);
	int			(*pmic_writeb) (const struct device *dev,
						u16 addr, u8 val);
	int			(*pmic_read_buf) (const struct device *dev,
						u16 addr, u8 *buf, int n);
	int			(*pmic_write_buf) (const struct device *dev,
						u16 addr, u8 *buf, int n);
	int			(*pmic_read_irq_stat) (const struct device *dev,
						int irq);
	enum pm8xxx_version	(*pmic_get_version) (const struct device *dev);
	int			(*pmic_get_revision) (const struct device *dev);
	void			*pm_chip_data;
};

static inline int pm8xxx_readb(const struct device *dev, u16 addr, u8 *val)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_readb(dev, addr, val);
}

static inline int pm8xxx_writeb(const struct device *dev, u16 addr, u8 val)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_writeb(dev, addr, val);
}

static inline int pm8xxx_read_buf(const struct device *dev, u16 addr, u8 *buf,
									int n)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_read_buf(dev, addr, buf, n);
}

static inline int pm8xxx_write_buf(const struct device *dev, u16 addr, u8 *buf,
									int n)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_write_buf(dev, addr, buf, n);
}

static inline int pm8xxx_read_irq_stat(const struct device *dev, int irq)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_read_irq_stat(dev, irq);
}

static inline enum pm8xxx_version pm8xxx_get_version(const struct device *dev)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_get_version(dev);
}

static inline int pm8xxx_get_revision(const struct device *dev)
{
	struct pm8xxx_drvdata *dd = dev_get_drvdata(dev);

	if (!dd)
		return -EINVAL;
	return dd->pmic_get_revision(dev);
}

#endif
