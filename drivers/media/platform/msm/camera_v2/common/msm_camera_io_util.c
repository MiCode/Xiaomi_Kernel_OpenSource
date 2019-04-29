/* Copyright (c) 2011-2014, 2017-2019, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/err.h>
#include <soc/qcom/camera2.h>
#include <linux/msm-bus.h>
#include "msm_camera_io_util.h"

#define BUFF_SIZE_128 128

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

void msm_camera_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: 0x%pK %08x\n", __func__,  (addr), (data));
	writel_relaxed((data), (addr));
}

/* This API is to write a block of data
 * to same address
 */
int32_t msm_camera_io_w_block(const u32 *addr, void __iomem *base,
	u32 len)
{
	int i;

	if (!addr || !len || !base)
		return -EINVAL;

	for (i = 0; i < len; i++) {
		CDBG("%s: len =%d val=%x base =%pK\n", __func__,
			len, addr[i], base);
		writel_relaxed(addr[i], base);
	}
	return 0;
}

/* This API is to write a block of registers
 *  which is like a 2 dimensional array table with
 *  register offset and data
 */
int32_t msm_camera_io_w_reg_block(const u32 *addr, void __iomem *base,
	u32 len)
{
	int i;

	if (!addr || !len || !base)
		return -EINVAL;

	for (i = 0; i < len; i = i + 2) {
		CDBG("%s: len =%d val=%x base =%pK reg=%x\n", __func__,
			len, addr[i + 1], base,  addr[i]);
		writel_relaxed(addr[i + 1], base + addr[i]);
	}
	return 0;
}

void msm_camera_io_w_mb(u32 data, void __iomem *addr)
{
	CDBG("%s: 0x%pK %08x\n", __func__,  (addr), (data));
	/* ensure write is done */
	wmb();
	writel_relaxed((data), (addr));
	/* ensure write is done */
	wmb();
}

int32_t msm_camera_io_w_mb_block(const u32 *addr, void __iomem *base, u32 len)
{
	int i;

	if (!addr || !len || !base)
		return -EINVAL;

	for (i = 0; i < len; i++) {
		/* ensure write is done */
		wmb();
		CDBG("%s: len =%d val=%x base =%pK\n", __func__,
			len, addr[i], base);
		writel_relaxed(addr[i], base);
	}
	/* ensure last write is done */
	wmb();
	return 0;
}

u32 msm_camera_io_r(void __iomem *addr)
{
	uint32_t data = readl_relaxed(addr);

	CDBG("%s: 0x%pK %08x\n", __func__,  (addr), (data));
	return data;
}

u32 msm_camera_io_r_mb(void __iomem *addr)
{
	uint32_t data;
	/* ensure read is done */
	rmb();
	data = readl_relaxed(addr);
	/* ensure read is done */
	rmb();
	CDBG("%s: 0x%pK %08x\n", __func__,  (addr), (data));
	return data;
}

static void msm_camera_io_memcpy_toio(void __iomem *dest_addr,
	void *src_addr, u32 len)
{
	int i;
	u32 __iomem *d = (u32 __iomem *) dest_addr;
	u32 *s = (u32 *) src_addr;

	for (i = 0; i < len; i++)
		writel_relaxed(*s++, d++);
}

int32_t msm_camera_io_poll_value(void __iomem *addr, u32 wait_data, u32 retry,
	unsigned long min_usecs, unsigned long max_usecs)
{
	uint32_t tmp, cnt = 0;
	int32_t rc = 0;

	if (!addr)
		return -EINVAL;

	tmp = msm_camera_io_r(addr);
	while ((tmp != wait_data) && (cnt++ < retry)) {
		if (min_usecs > 0 && max_usecs > 0)
			usleep_range(min_usecs, max_usecs);
		tmp = msm_camera_io_r(addr);
	}
	if (cnt > retry) {
		pr_debug("Poll failed by value\n");
		rc = -EINVAL;
	}
	return rc;
}

int32_t msm_camera_io_poll_value_wmask(void __iomem *addr, u32 wait_data,
	u32 bmask, u32 retry, unsigned long min_usecs, unsigned long max_usecs)
{
	uint32_t tmp, cnt = 0;
	int32_t rc = 0;

	if (!addr)
		return -EINVAL;

	tmp = msm_camera_io_r(addr);
	while (((tmp & bmask) != wait_data) && (cnt++ < retry)) {
		if (min_usecs > 0 && max_usecs > 0)
			usleep_range(min_usecs, max_usecs);
		tmp = msm_camera_io_r(addr);
	}
	if (cnt > retry) {
		pr_debug("Poll failed with mask\n");
		rc = -EINVAL;
	}
	return rc;
}

void msm_camera_io_dump(void __iomem *addr, int size, int enable)
{
	char line_str[128];
	int i;
	ptrdiff_t p = 0;
	size_t offset = 0, used = 0;
	u32 data;

	CDBG("%s: addr=%pK size=%d\n", __func__, addr, size);

	if (!addr || (size <= 0) || !enable)
		return;

	line_str[0] = '\0';
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			used = snprintf(line_str + offset,
				sizeof(line_str) - offset, "0x%04tX: ", p);
			if (offset + used >= sizeof(line_str)) {
				pr_err("%s\n", line_str);
				offset = 0;
				line_str[0] = '\0';
			} else {
				offset += used;
			}
		}
		data = readl_relaxed(addr + p);
		p = p + 4;
		used = snprintf(line_str + offset,
			sizeof(line_str) - offset, "%08x ", data);
		if (offset + used >= sizeof(line_str)) {
			pr_err("%s\n", line_str);
			offset = 0;
			line_str[0] = '\0';
		} else {
			offset += used;
		}
		if ((i + 1) % 4 == 0) {
			pr_err("%s\n", line_str);
			line_str[0] = '\0';
			offset = 0;
		}
	}
	if (line_str[0] != '\0')
		pr_err("%s\n", line_str);
}

void msm_camera_io_dump_wstring_base(void __iomem *addr,
	struct msm_cam_dump_string_info *dump_data,
	int size)
{
	int i, u = sizeof(struct msm_cam_dump_string_info);

	pr_debug("%s: addr=%pK data=%pK size=%d u=%d, cnt=%d\n", __func__,
		addr, dump_data, size, u,
		(size/u));

	if (!addr || (size <= 0) || !dump_data) {
		pr_err("%s: addr=%pK data=%pK size=%d\n", __func__,
			addr, dump_data, size);
		return;
	}
	for (i = 0; i < (size / u); i++)
		pr_debug("%s 0x%x\n", (dump_data + i)->print,
			readl_relaxed((dump_data + i)->offset + addr));
}

void msm_camera_io_memcpy(void __iomem *dest_addr,
	void *src_addr, u32 len)
{
	CDBG("%s: %pK %pK %d\n", __func__, dest_addr, src_addr, len);
	msm_camera_io_memcpy_toio(dest_addr, src_addr, len / 4);
}

void msm_camera_io_memcpy_mb(void __iomem *dest_addr,
	void *src_addr, u32 len)
{
	int i;
	u32 __iomem *d = (u32 __iomem *) dest_addr;
	u32 *s = (u32 *) src_addr;
	/* This is generic function called who needs to register
	 * writes with memory barrier
	 */
	wmb();
	for (i = 0; i < (len / 4); i++) {
		msm_camera_io_w(*s++, d++);
		/* ensure write is done after every iteration */
		wmb();
	}
}

int msm_cam_clk_sel_src(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct msm_cam_clk_info *clk_src_info, int num_clk)
{
	int i;
	int rc = 0;
	struct clk *mux_clk = NULL;
	struct clk *src_clk = NULL;

	for (i = 0; i < num_clk; i++) {
		if (clk_src_info[i].clk_name) {
			mux_clk = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(mux_clk)) {
				pr_err("%s get failed\n",
					 clk_info[i].clk_name);
				continue;
			}
			src_clk = clk_get(dev, clk_src_info[i].clk_name);
			if (IS_ERR(src_clk)) {
				pr_err("%s get failed\n",
					clk_src_info[i].clk_name);
				continue;
			}
			clk_set_parent(mux_clk, src_clk);
		}
	}
	return rc;
}

int msm_cam_clk_enable(struct device *dev, struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	int i;
	int rc = 0;
	long clk_rate;

	if (enable) {
		for (i = 0; i < num_clk; i++) {
			CDBG("%s enable %s\n", __func__, clk_info[i].clk_name);
			clk_ptr[i] = clk_get(dev, clk_info[i].clk_name);
			if (IS_ERR(clk_ptr[i])) {
				pr_err("%s get failed\n", clk_info[i].clk_name);
				rc = PTR_ERR(clk_ptr[i]);
				goto cam_clk_get_err;
			}
			if (clk_info[i].clk_rate > 0) {
				clk_rate = clk_round_rate(clk_ptr[i],
					clk_info[i].clk_rate);
				if (clk_rate < 0) {
					pr_err("%s round failed\n",
						   clk_info[i].clk_name);
					goto cam_clk_set_err;
				}
				rc = clk_set_rate(clk_ptr[i],
					clk_rate);
				if (rc < 0) {
					pr_err("%s set failed\n",
						clk_info[i].clk_name);
					goto cam_clk_set_err;
				}

			} else if (clk_info[i].clk_rate == INIT_RATE) {
				clk_rate = clk_get_rate(clk_ptr[i]);
				if (clk_rate == 0) {
					clk_rate =
						  clk_round_rate(clk_ptr[i], 0);
					if (clk_rate < 0) {
						pr_err("%s round rate failed\n",
							  clk_info[i].clk_name);
						goto cam_clk_set_err;
					}
					rc = clk_set_rate(clk_ptr[i],
								clk_rate);
					if (rc < 0) {
						pr_err("%s set rate failed\n",
							  clk_info[i].clk_name);
						goto cam_clk_set_err;
					}
				}
			}
			rc = clk_prepare(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s prepare failed\n",
					   clk_info[i].clk_name);
				goto cam_clk_prepare_err;
			}

			rc = clk_enable(clk_ptr[i]);
			if (rc < 0) {
				pr_err("%s enable failed\n",
					   clk_info[i].clk_name);
				goto cam_clk_enable_err;
			}
			if (clk_info[i].delay > 20) {
				msleep(clk_info[i].delay);
			} else if (clk_info[i].delay) {
				usleep_range(clk_info[i].delay * 1000,
					(clk_info[i].delay * 1000) + 1000);
			}
		}
	} else {
		for (i = num_clk - 1; i >= 0; i--) {
			if (!IS_ERR_OR_NULL(clk_ptr[i])) {
				CDBG("%s disable %s\n", __func__,
					clk_info[i].clk_name);
				clk_disable(clk_ptr[i]);
				clk_unprepare(clk_ptr[i]);
				clk_put(clk_ptr[i]);
				clk_ptr[i] = NULL;
			}
		}
	}
	return rc;


cam_clk_enable_err:
	clk_unprepare(clk_ptr[i]);
cam_clk_prepare_err:
cam_clk_set_err:
	clk_put(clk_ptr[i]);
cam_clk_get_err:
	for (i--; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(clk_ptr[i])) {
			clk_disable(clk_ptr[i]);
			clk_unprepare(clk_ptr[i]);
			clk_put(clk_ptr[i]);
			clk_ptr[i] = NULL;
		}
	}
	return rc;
}

int msm_camera_config_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int config)
{
	int i = 0, j = 0;
	int rc = 0;
	struct camera_vreg_t *curr_vreg;

	if (num_vreg_seq > num_vreg) {
		pr_err("%s:%d vreg sequence invalid\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!num_vreg_seq)
		num_vreg_seq = num_vreg;

	if ((cam_vreg == NULL) && num_vreg_seq) {
		pr_err("%s:%d cam_vreg NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (config) {
		for (i = 0; i < num_vreg_seq; i++) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			curr_vreg = &cam_vreg[j];
			reg_ptr[j] = regulator_get(dev,
				curr_vreg->reg_name);
			if (IS_ERR_OR_NULL(reg_ptr[j])) {
				pr_err("%s: %s get failed\n",
					 __func__,
					 curr_vreg->reg_name);
				reg_ptr[j] = NULL;
				goto vreg_get_fail;
			}
			if (regulator_count_voltages(reg_ptr[j]) > 0) {
				rc = regulator_set_voltage(
					reg_ptr[j],
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
				if (rc < 0) {
					pr_err("%s: %s set voltage failed\n",
						__func__,
						curr_vreg->reg_name);
					goto vreg_set_voltage_fail;
				}
				if (curr_vreg->op_mode >= 0) {
					rc = regulator_set_load(
						reg_ptr[j],
						curr_vreg->op_mode);
					rc = 0;
					if (rc < 0) {
						pr_err(
						"%s:%s set optimum mode fail\n",
						__func__,
						curr_vreg->reg_name);
						goto vreg_set_opt_mode_fail;
					}
				}
			}
		}
	} else {
		for (i = num_vreg_seq-1; i >= 0; i--) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			curr_vreg = &cam_vreg[j];
			if (reg_ptr[j]) {
				if (regulator_count_voltages(reg_ptr[j]) > 0) {
					if (curr_vreg->op_mode >= 0) {
						regulator_set_load(
							reg_ptr[j], 0);
					}
					regulator_set_voltage(
						reg_ptr[j], 0, curr_vreg->
						max_voltage);
				}
				regulator_put(reg_ptr[j]);
				reg_ptr[j] = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
if (regulator_count_voltages(reg_ptr[j]) > 0)
	regulator_set_load(reg_ptr[j], 0);

vreg_set_opt_mode_fail:
if (regulator_count_voltages(reg_ptr[j]) > 0)
	regulator_set_voltage(reg_ptr[j], 0,
		curr_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(reg_ptr[j]);
	reg_ptr[j] = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		if (vreg_seq) {
			j = vreg_seq[i];
			if (j >= num_vreg)
				continue;
		} else
			j = i;
		curr_vreg = &cam_vreg[j];
		goto vreg_unconfig;
	}
	return -ENODEV;
}

int msm_camera_enable_vreg(struct device *dev, struct camera_vreg_t *cam_vreg,
		int num_vreg, enum msm_camera_vreg_name_t *vreg_seq,
		int num_vreg_seq, struct regulator **reg_ptr, int enable)
{
	int i = 0, j = 0, rc = 0;

	if (num_vreg_seq > num_vreg) {
		pr_err("%s:%d vreg sequence invalid\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (!num_vreg_seq)
		num_vreg_seq = num_vreg;

	if (enable) {
		for (i = 0; i < num_vreg_seq; i++) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			if (IS_ERR_OR_NULL(reg_ptr[j])) {
				pr_err("%s: %s null regulator\n",
					__func__, cam_vreg[j].reg_name);
				goto disable_vreg;
			}
			rc = regulator_enable(reg_ptr[j]);
			if (rc < 0) {
				pr_err("%s: %s enable failed\n",
					__func__, cam_vreg[j].reg_name);
				goto disable_vreg;
			}
			if (cam_vreg[j].delay > 20)
				msleep(cam_vreg[j].delay);
			else if (cam_vreg[j].delay)
				usleep_range(cam_vreg[j].delay * 1000,
					(cam_vreg[j].delay * 1000) + 1000);
		}
	} else {
		for (i = num_vreg_seq-1; i >= 0; i--) {
			if (vreg_seq) {
				j = vreg_seq[i];
				if (j >= num_vreg)
					continue;
			} else
				j = i;
			if (reg_ptr[j]) {
				regulator_disable(reg_ptr[j]);
				if (cam_vreg[j].delay > 20)
					msleep(cam_vreg[j].delay);
				else if (cam_vreg[j].delay)
					usleep_range(
						cam_vreg[j].delay * 1000,
						(cam_vreg[j].delay * 1000)
						+ 1000);
			}
		}
	}
	return rc;
disable_vreg:
	for (i--; i >= 0; i--) {
		if (vreg_seq) {
			j = vreg_seq[i];
			if (j >= num_vreg)
				continue;
		} else
			j = i;
		regulator_disable(reg_ptr[j]);
		if (cam_vreg[j].delay > 20)
			msleep(cam_vreg[j].delay);
		else if (cam_vreg[j].delay)
			usleep_range(cam_vreg[j].delay * 1000,
				(cam_vreg[j].delay * 1000) + 1000);
	}
	return rc;
}

void msm_camera_bus_scale_cfg(uint32_t bus_perf_client,
		enum msm_bus_perf_setting perf_setting)
{
	int rc = 0;

	if (!bus_perf_client) {
		pr_err("%s: Bus Client NOT Registered!!!\n", __func__);
		return;
	}
	switch (perf_setting) {
	case S_EXIT:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 1);
		msm_bus_scale_unregister_client(bus_perf_client);
		break;
	case S_PREVIEW:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 1);
		break;
	case S_VIDEO:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 2);
		break;
	case S_CAPTURE:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 3);
		break;
	case S_ZSL:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 4);
		break;
	case S_LIVESHOT:
		rc = msm_bus_scale_client_update_request(bus_perf_client, 5);
		break;
	case S_DEFAULT:
		break;
	default:
		pr_err("%s: INVALID CASE\n", __func__);
	}
}

int msm_camera_set_gpio_table(struct msm_gpio_set_tbl *gpio_tbl,
	uint8_t gpio_tbl_size, int gpio_en)
{
	int rc = 0, i;

	if (gpio_en) {
		for (i = 0; i < gpio_tbl_size; i++) {
			gpio_set_value_cansleep(gpio_tbl[i].gpio,
				gpio_tbl[i].flags);
			usleep_range(gpio_tbl[i].delay,
				gpio_tbl[i].delay + 1000);
		}
	} else {
		for (i = gpio_tbl_size - 1; i >= 0; i--) {
			if (gpio_tbl[i].flags)
				gpio_set_value_cansleep(gpio_tbl[i].gpio,
					GPIOF_OUT_INIT_LOW);
		}
	}
	return rc;
}

int msm_camera_config_single_vreg(struct device *dev,
	struct camera_vreg_t *cam_vreg, struct regulator **reg_ptr, int config)
{
	int rc = 0;
	const char *vreg_name = NULL;

	if (!dev || !cam_vreg || !reg_ptr) {
		pr_err("%s: get failed NULL parameter\n", __func__);
		goto vreg_get_fail;
	}
	if (cam_vreg->type == VREG_TYPE_CUSTOM) {
		if (cam_vreg->custom_vreg_name == NULL) {
			pr_err("%s : can't find sub reg name",
				__func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->custom_vreg_name;
	} else {
		if (cam_vreg->reg_name == NULL) {
			pr_err("%s : can't find reg name", __func__);
			goto vreg_get_fail;
		}
		vreg_name = cam_vreg->reg_name;
	}

	if (config) {
		CDBG("%s enable %s\n", __func__, vreg_name);
		*reg_ptr = regulator_get(dev, vreg_name);
		if (IS_ERR(*reg_ptr)) {
			pr_err("%s: %s get failed\n", __func__, vreg_name);
			*reg_ptr = NULL;
			goto vreg_get_fail;
		}
		if (regulator_count_voltages(*reg_ptr) > 0) {
			CDBG("%s: voltage min=%d, max=%d\n",
				__func__, cam_vreg->min_voltage,
				cam_vreg->max_voltage);
			rc = regulator_set_voltage(
				*reg_ptr, cam_vreg->min_voltage,
				cam_vreg->max_voltage);
			if (rc < 0) {
				pr_err("%s: %s set voltage failed\n",
					__func__, vreg_name);
				goto vreg_set_voltage_fail;
			}
			if (cam_vreg->op_mode >= 0) {
				rc = regulator_set_load(*reg_ptr,
					cam_vreg->op_mode);
				if (rc < 0) {
					pr_err(
					"%s: %s set optimum mode failed\n",
					__func__, vreg_name);
					goto vreg_set_opt_mode_fail;
				}
			}
		}
		rc = regulator_enable(*reg_ptr);
		if (rc < 0) {
			pr_err("%s: %s regulator_enable failed\n", __func__,
				vreg_name);
			goto vreg_unconfig;
		}
	} else {
		CDBG("%s disable %s\n", __func__, vreg_name);
		if (*reg_ptr) {
			CDBG("%s disable %s\n", __func__, vreg_name);
			regulator_disable(*reg_ptr);
			if (regulator_count_voltages(*reg_ptr) > 0) {
				if (cam_vreg->op_mode >= 0)
					regulator_set_load(*reg_ptr, 0);
				regulator_set_voltage(
					*reg_ptr, 0, cam_vreg->max_voltage);
			}
			regulator_put(*reg_ptr);
			*reg_ptr = NULL;
		} else {
			pr_err("%s can't disable %s\n", __func__, vreg_name);
		}
	}
	return 0;

vreg_unconfig:
if (regulator_count_voltages(*reg_ptr) > 0)
	regulator_set_load(*reg_ptr, 0);

vreg_set_opt_mode_fail:
if (regulator_count_voltages(*reg_ptr) > 0)
	regulator_set_voltage(*reg_ptr, 0, cam_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(*reg_ptr);
	*reg_ptr = NULL;

vreg_get_fail:
	return -ENODEV;
}

int msm_camera_request_gpio_table(struct gpio *gpio_tbl, uint8_t size,
	int gpio_en)
{
	int rc = 0, i = 0, err = 0;

	if (!gpio_tbl || !size) {
		pr_err("%s:%d invalid gpio_tbl %pK / size %d\n", __func__,
			__LINE__, gpio_tbl, size);
		return -EINVAL;
	}
	for (i = 0; i < size; i++) {
		CDBG("%s:%d i %d, gpio %d dir %ld\n", __func__, __LINE__, i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}
	if (gpio_en) {
		for (i = 0; i < size; i++) {
			err = gpio_request_one(gpio_tbl[i].gpio,
				gpio_tbl[i].flags, gpio_tbl[i].label);
			if (err) {
				/*
				 * After GPIO request fails, contine to
				 * apply new gpios, outout a error message
				 * for driver bringup debug
				 */
				pr_err("%s:%d gpio %d:%s request fails\n",
					__func__, __LINE__,
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		gpio_free_array(gpio_tbl, size);
	}
	return rc;
}

/*
 * msm_camera_get_dt_reg_settings - Get dt reg settings from device-tree.
 * @of_node: Pointer to device of_node from dev.
 * @dt_prop_name: String of the property to search in of_node from dev.
 * @reg_s: Double pointer will be allocated by this function and filled.
 * @size: Pointer to fill the length of the available entries.
 */
int msm_camera_get_dt_reg_settings(struct device_node *of_node,
	const char *dt_prop_name, uint32_t **reg_s,
	unsigned int *size)
{
	int ret;
	unsigned int cnt;

	if (!of_node || !dt_prop_name || !size || !reg_s) {
		pr_err("%s: Error invalid args %pK:%pK:%pK:%pK\n",
			__func__, size, reg_s, of_node, dt_prop_name);
		return -EINVAL;
	}
	if (!of_get_property(of_node, dt_prop_name, &cnt)) {
		pr_debug("Missing dt reg settings for %s\n", dt_prop_name);
		return -ENOENT;
	}

	if (!cnt || (cnt % 8)) {
		pr_err("%s: Error invalid number of entries cnt=%d\n",
			__func__, cnt);
		return -EINVAL;
	}
	cnt /= 4;
	if (cnt != 0) {
		*reg_s = kcalloc(cnt, sizeof(uint32_t),
				GFP_KERNEL);
		if (!*reg_s)
			return -ENOMEM;
		ret = of_property_read_u32_array(of_node,
				dt_prop_name,
				*reg_s,
				cnt);
		if (ret < 0) {
			pr_err("%s: No dt reg info read for %s ret=%d\n",
				__func__, dt_prop_name, ret);
			kfree(*reg_s);
			return -ENOENT;
		}
		*size = cnt;
	} else {
		pr_err("%s: Error invalid entries\n", __func__);
		return -EINVAL;
	}

	return ret;
}

/*
 * msm_camera_get_dt_reg_settings - Free dt reg settings memory.
 * @reg_s: Double pointer will be allocated by this function and filled.
 * @size: Pointer to set the length as invalid.
 */
void msm_camera_put_dt_reg_settings(uint32_t **reg_s,
	unsigned int *size)
{
	kfree(*reg_s);
	*reg_s = NULL;
	*size = 0;
}

int msm_camera_hw_write_dt_reg_settings(void __iomem *base,
	uint32_t *reg_s,
	unsigned int size)
{
	int32_t rc = 0;

	if (!reg_s || !base || !size) {
		pr_err("%s: Error invalid args\n", __func__);
		return -EINVAL;
	}
	rc = msm_camera_io_w_reg_block((const u32 *) reg_s,
		base, size);
	if (rc < 0)
		pr_err("%s: Failed dt reg setting write\n", __func__);
	return rc;
}

