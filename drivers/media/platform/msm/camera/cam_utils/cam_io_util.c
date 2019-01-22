/* Copyright (c) 2011-2014, 2017-2018, The Linux Foundation.
 * All rights reserved.
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include "cam_io_util.h"
#include "cam_debug_util.h"

int cam_io_w(uint32_t data, void __iomem *addr)
{
	if (!addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);
	writel_relaxed_no_log(data, addr);

	return 0;
}

int cam_io_w_mb(uint32_t data, void __iomem *addr)
{
	if (!addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);
	/* Ensure previous writes are done */
	wmb();
	writel_relaxed_no_log(data, addr);
	/* Ensure previous writes are done */
	wmb();

	return 0;
}

int cam_io_w_vfe_mb(uint32_t data, void __iomem *addr)
{
	if (!addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);
	writel_relaxed(data, addr);
	/* Ensure previous writes are done */
	wmb();

	return 0;
}

int cam_io_w_csid_mb(uint32_t data, void __iomem *addr)
{
	if (!addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);
	writel_relaxed(data, addr);
	/* Ensure previous writes are done */
	wmb();

	return 0;
}

uint32_t cam_io_r(void __iomem *addr)
{
	uint32_t data;

	if (!addr) {
		CAM_ERR(CAM_UTIL, "Invalid args");
		return 0;
	}

	data = readl_relaxed(addr);
	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);

	return data;
}

uint32_t cam_io_r_mb(void __iomem *addr)
{
	uint32_t data;

	if (!addr) {
		CAM_ERR(CAM_UTIL, "Invalid args");
		return 0;
	}

	/* Ensure previous read is done */
	rmb();
	data = readl_relaxed(addr);
	CAM_DBG(CAM_UTIL, "0x%pK %08x", addr, data);
	/* Ensure previous read is done */
	rmb();

	return data;
}

int cam_io_memcpy(void __iomem *dest_addr,
	void __iomem *src_addr, uint32_t len)
{
	int i;
	uint32_t *d = (uint32_t *) dest_addr;
	uint32_t *s = (uint32_t *) src_addr;

	if (!dest_addr || !src_addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "%pK %pK %d", dest_addr, src_addr, len);

	for (i = 0; i < len/4; i++) {
		CAM_DBG(CAM_UTIL, "0x%pK %08x", d, *s);
		writel_relaxed(*s++, d++);
	}

	return 0;
}

int  cam_io_memcpy_mb(void __iomem *dest_addr,
	void __iomem *src_addr, uint32_t len)
{
	int i;
	uint32_t *d = (uint32_t *) dest_addr;
	uint32_t *s = (uint32_t *) src_addr;

	if (!dest_addr || !src_addr)
		return -EINVAL;

	CAM_DBG(CAM_UTIL, "%pK %pK %d", dest_addr, src_addr, len);

	/*
	 * Do not use cam_io_w_mb to avoid double wmb() after a write
	 * and before the next write.
	 */
	wmb();
	for (i = 0; i < (len / 4); i++) {
		CAM_DBG(CAM_UTIL, "0x%pK %08x", d, *s);
		writel_relaxed(*s++, d++);
	}
	/* Ensure previous writes are done */
	wmb();

	return 0;
}

int cam_io_poll_value(void __iomem *addr, uint32_t wait_data, uint32_t retry,
	unsigned long min_usecs, unsigned long max_usecs)
{
	uint32_t tmp, cnt = 0;
	int rc = 0;

	if (!addr)
		return -EINVAL;

	tmp = readl_relaxed(addr);
	while ((tmp != wait_data) && (cnt++ < retry)) {
		if (min_usecs > 0 && max_usecs > 0)
			usleep_range(min_usecs, max_usecs);
		tmp = readl_relaxed(addr);
	}

	if (cnt > retry) {
		CAM_DBG(CAM_UTIL, "Poll failed by value");
		rc = -EINVAL;
	}

	return rc;
}

int cam_io_poll_value_wmask(void __iomem *addr, uint32_t wait_data,
	uint32_t bmask, uint32_t retry, unsigned long min_usecs,
	unsigned long max_usecs)
{
	uint32_t tmp, cnt = 0;
	int rc = 0;

	if (!addr)
		return -EINVAL;

	tmp = readl_relaxed(addr);
	while (((tmp & bmask) != wait_data) && (cnt++ < retry)) {
		if (min_usecs > 0 && max_usecs > 0)
			usleep_range(min_usecs, max_usecs);
		tmp = readl_relaxed(addr);
	}

	if (cnt > retry) {
		CAM_DBG(CAM_UTIL, "Poll failed with mask");
		rc = -EINVAL;
	}

	return rc;
}

int cam_io_w_same_offset_block(const uint32_t *data, void __iomem *addr,
	uint32_t len)
{
	int i;

	if (!data || !len || !addr)
		return -EINVAL;

	for (i = 0; i < len; i++) {
		CAM_DBG(CAM_UTIL, "i= %d len =%d val=%x addr =%pK",
			i, len, data[i], addr);
		writel_relaxed(data[i], addr);
	}

	return 0;
}

int cam_io_w_mb_same_offset_block(const uint32_t *data, void __iomem *addr,
	uint32_t len)
{
	int i;

	if (!data || !len || !addr)
		return -EINVAL;

	for (i = 0; i < len; i++) {
		CAM_DBG(CAM_UTIL, "i= %d len =%d val=%x addr =%pK",
			i, len, data[i], addr);
		/* Ensure previous writes are done */
		wmb();
		writel_relaxed(data[i], addr);
	}

	return 0;
}

#define __OFFSET(__i)   (data[__i][0])
#define __VAL(__i)      (data[__i][1])
int cam_io_w_offset_val_block(const uint32_t data[][2],
	void __iomem *addr_base, uint32_t len)
{
	int i;

	if (!data || !len || !addr_base)
		return -EINVAL;

	for (i = 0; i < len; i++) {
		CAM_DBG(CAM_UTIL, "i= %d len =%d val=%x addr_base =%pK reg=%x",
			i, len, __VAL(i), addr_base, __OFFSET(i));
		writel_relaxed(__VAL(i), addr_base + __OFFSET(i));
	}

	return 0;
}

int cam_io_w_mb_offset_val_block(const uint32_t data[][2],
	void __iomem *addr_base, uint32_t len)
{
	int i;

	if (!data || !len || !addr_base)
		return -EINVAL;

	/* Ensure write is done */
	wmb();
	for (i = 0; i < len; i++) {
		CAM_DBG(CAM_UTIL, "i= %d len =%d val=%x addr_base =%pK reg=%x",
			i, len, __VAL(i), addr_base, __OFFSET(i));
		writel_relaxed(__VAL(i), addr_base + __OFFSET(i));
	}

	return 0;
}

#define BYTES_PER_REGISTER           4
#define NUM_REGISTER_PER_LINE        4
#define REG_OFFSET(__start, __i)    (__start + (__i * BYTES_PER_REGISTER))
int cam_io_dump(void __iomem *base_addr, uint32_t start_offset, int size)
{
	char          line_str[128];
	char         *p_str;
	int           i;
	uint32_t      data;

	CAM_DBG(CAM_UTIL, "addr=%pK offset=0x%x size=%d",
		base_addr, start_offset, size);

	if (!base_addr || (size <= 0))
		return -EINVAL;

	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size; i++) {
		if (i % NUM_REGISTER_PER_LINE == 0) {
			snprintf(p_str, 12, "0x%08x: ",
				REG_OFFSET(start_offset, i));
			p_str += 11;
		}
		data = readl_relaxed(base_addr + REG_OFFSET(start_offset, i));
		snprintf(p_str, 9, "%08x ", data);
		p_str += 8;
		if ((i + 1) % NUM_REGISTER_PER_LINE == 0) {
			CAM_ERR(CAM_UTIL, "%s", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CAM_ERR(CAM_UTIL, "%s", line_str);

	return 0;
}

