/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef _CORESIGHT_NIDNT_H
#define _CORESIGHT_NIDNT_H

enum nidnt_debug_mode {
	NIDNT_MODE_SDCARD = 0x0,
	NIDNT_MODE_SDC_JTAG = 0x20,
	NIDNT_MODE_SDC_SWDTRC = 0x40,
	NIDNT_MODE_SDC_TRACE = 0x60,
	NIDNT_MODE_SDC_SWDUART = 0xf1,
	NIDNT_MODE_SDC_SPMI = 0xf2,
};

#ifdef CONFIG_CORESIGHT_TPIU
extern void coresight_nidnt_writel(unsigned int val, unsigned int off);

extern int coresight_nidnt_config_swoverride(enum nidnt_debug_mode mode);

extern int coresight_nidnt_config_qdsd_enable(bool enable);
extern void coresight_nidnt_set_hwdetect_param(bool val);

extern ssize_t coresight_nidnt_show_timeout_value(struct device *dev,
				struct device_attribute *attr, char *buf);

extern ssize_t coresight_nidnt_store_timeout_value(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size);

extern ssize_t coresight_nidnt_show_debounce_value(struct device *dev,
				     struct device_attribute *attr, char *buf);

extern ssize_t coresight_nidnt_store_debounce_value(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size);

extern int coresight_nidnt_init(struct platform_device *pdev);

extern int coresight_nidnt_enable_hwdetect(void);

extern int coresight_nidnt_get_status(void);
#else
static inline void coresight_nidnt_writel(unsigned int val,
						unsigned int off) {}

static inline int coresight_nidnt_config_swoverride(enum nidnt_debug_mode mode)
{
	return -ENOSYS;
}

static inline int coresight_nidnt_config_qdsd_enable(bool enable)
{
	return -ENOSYS;
}

static inline void coresight_nidnt_set_hwdetect_param(bool val) {}

static inline ssize_t coresight_nidnt_show_timeout_value(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return -ENOSYS;
}

static inline ssize_t coresight_nidnt_store_timeout_value(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return -ENOSYS;
}

static inline ssize_t coresight_nidnt_show_debounce_value(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return -ENOSYS;
}

static inline ssize_t coresight_nidnt_store_debounce_value(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return -ENOSYS;
}

static inline int coresight_nidnt_init(struct platform_device *pdev)
{
	return -ENOSYS;
}

int coresight_nidnt_enable_hwdetect(void)
{
	return -ENOSYS;
}

static inline int coresight_nidnt_get_status(void)
{
	return -ENOSYS;
}
#endif

#endif
