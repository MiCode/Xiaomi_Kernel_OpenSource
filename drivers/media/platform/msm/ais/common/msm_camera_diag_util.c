/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "msm_camera_diag_util.h"
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/irqreturn.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include "msm_camera_io_util.h"
#include "msm.h"


#define MAX_CLK_NUM	100
struct camera_diag_clk_list {
	struct msm_ais_diag_clk_info_t *clk_infolist;
	struct clk **ppclk;
	uint32_t clk_num;
	uint32_t clk_capacity;
	struct mutex lock;
};

struct camera_diag_ddrbw {
	struct msm_ais_diag_bus_info_t   bus_info;
	struct mutex lock;
};

static struct camera_diag_clk_list s_diag_clk_list;
static struct camera_diag_ddrbw s_ddrbw;

int msm_camera_get_reg_list(void __iomem *base,
		struct msm_camera_reg_list_cmd *reg_list)
{
	int rc = 0;
	uint32_t i;
	uint32_t *reg_values = NULL;
	uint32_t addrs_size = sizeof(uint32_t) * reg_list->reg_num;
	uint32_t *reg_addrs = kzalloc(addrs_size, GFP_KERNEL);

	if (!reg_addrs) {
		rc = -ENOMEM;
		goto alloc_addr_failed;
	}

	if (copy_from_user(reg_addrs,
		(void __user *)(reg_list->regaddr_list),
		sizeof(uint32_t) * reg_list->reg_num)) {
		rc = -EFAULT;
		pr_err("%s copy_from_user fail\n", __func__);
		goto copy_addr_failed;
	}

	reg_values = kzalloc(addrs_size, GFP_KERNEL);
	if (!reg_values) {
		rc = -ENOMEM;
		goto copy_addr_failed;
	}

	for (i = 0 ; i < reg_list->reg_num; ++i) {
		reg_values[i] = msm_camera_io_r(base + reg_addrs[i]);
		pr_debug("reg 0x%x 0x%x\n",
				reg_addrs[i],
				reg_values[i]);
	}

	if (copy_to_user(reg_list->value_list, reg_values,
		sizeof(uint32_t) * reg_list->reg_num)) {
		rc = -EFAULT;
		pr_err("%s copy_to_user fail %u\n",
			__func__,
			reg_list->reg_num);
		goto copy_value_failed;
	}

copy_value_failed:
	kfree(reg_values);

copy_addr_failed:
	kfree(reg_addrs);

alloc_addr_failed:

	return rc;
}

int msm_camera_diag_init(void)
{
	s_diag_clk_list.clk_num = 0;
	s_diag_clk_list.clk_capacity = MAX_CLK_NUM;
	s_diag_clk_list.clk_infolist = kzalloc(
				sizeof(struct msm_ais_diag_clk_info_t) *
				s_diag_clk_list.clk_capacity,
				GFP_KERNEL);

	if (!s_diag_clk_list.clk_infolist)
		return -ENOMEM;

	s_diag_clk_list.ppclk = kzalloc(sizeof(struct clk *) *
				s_diag_clk_list.clk_capacity,
				GFP_KERNEL);
	if (!s_diag_clk_list.ppclk) {
		kfree(s_diag_clk_list.clk_infolist);
		return -ENOMEM;
	}

	mutex_init(&s_diag_clk_list.lock);
	mutex_init(&s_ddrbw.lock);
	return 0;
}

int msm_camera_diag_uninit(void)
{
	mutex_destroy(&s_ddrbw.lock);
	mutex_destroy(&s_diag_clk_list.lock);

	kfree(s_diag_clk_list.clk_infolist);
	s_diag_clk_list.clk_infolist = NULL;

	kfree(s_diag_clk_list.ppclk);
	s_diag_clk_list.ppclk = NULL;

	return 0;
}

static uint32_t msm_camera_diag_find_clk_idx(
				struct msm_cam_clk_info *clk_info,
				struct clk *clk_ptr)
{
	uint32_t i = 0;

	for (; i < s_diag_clk_list.clk_num; ++i) {
		if (clk_ptr == s_diag_clk_list.ppclk[i])
			return i;
	}

	return s_diag_clk_list.clk_capacity;
}

int msm_camera_diag_update_clklist(
		struct msm_cam_clk_info *clk_info,
		struct clk **clk_ptr, int num_clk, int enable)
{
	uint32_t i = 0;
	uint32_t idx = 0;
	uint32_t actual_idx = 0;
	struct msm_ais_diag_clk_info_t *pclk_info = NULL;

	mutex_lock(&s_diag_clk_list.lock);
	for (; i < num_clk; ++i) {
		idx = msm_camera_diag_find_clk_idx(&clk_info[i], clk_ptr[i]);
		if (idx < s_diag_clk_list.clk_num) {
			actual_idx = idx;
			pclk_info =
				&s_diag_clk_list.clk_infolist[actual_idx];
		} else if (s_diag_clk_list.clk_num <
					s_diag_clk_list.clk_capacity) {
			actual_idx = s_diag_clk_list.clk_num++;
			memset(&s_diag_clk_list.clk_infolist[actual_idx],
					0,
					sizeof(struct msm_ais_diag_clk_info_t));
			pclk_info =
				&s_diag_clk_list.clk_infolist[actual_idx];
			memcpy(pclk_info->clk_name,
				clk_info[i].clk_name,
				sizeof(pclk_info->clk_name));
			s_diag_clk_list.ppclk[actual_idx] = clk_ptr[i];
			pr_debug("%s new clk %s clk_num %u\n",
					__func__,
					clk_info[i].clk_name,
					s_diag_clk_list.clk_num);
		} else {
			pr_err("%s too many clks\n", __func__);
			continue;
		}

		pclk_info->clk_rate = clk_get_rate(clk_ptr[i]);
		if (enable) {
			++pclk_info->enable;
		} else {
			int cnt = pclk_info->enable;

			if (cnt > 0)
				--pclk_info->enable;
		}
	}

	mutex_unlock(&s_diag_clk_list.lock);
	return 0;
}

int msm_camera_diag_get_clk_list(
	struct msm_ais_diag_clk_list_t *clk_infolist)
{
	int rc = 0;

	mutex_lock(&s_diag_clk_list.lock);
	clk_infolist->clk_num = s_diag_clk_list.clk_num;
	if (copy_to_user(clk_infolist->clk_info,
			s_diag_clk_list.clk_infolist,
			sizeof(struct msm_ais_diag_clk_info_t) *
			s_diag_clk_list.clk_num)) {
		rc = -EFAULT;
	}
	mutex_unlock(&s_diag_clk_list.lock);
	return rc;
}

int msm_camera_diag_get_gpio_list(
		struct msm_ais_diag_gpio_list_t *gpio_list)
{
	int rc = 0;
	uint32_t gpio_num = gpio_list->gpio_num;
	uint32_t i = 0;
	int32_t *vals = NULL;
	uint32_t idxs_size = sizeof(uint32_t) * gpio_num;
	uint32_t vals_size = sizeof(int32_t) * gpio_num;
	uint32_t *idxs = kzalloc(idxs_size, GFP_KERNEL);

	if (!idxs) {
		rc = -ENOMEM;
		goto alloc_idxs_failed;
	}

	if (copy_from_user(idxs,
		(void __user *)(gpio_list->gpio_idx_list),
		idxs_size)) {
		rc = -EFAULT;
		pr_err("%s copy_from_user fail\n", __func__);
		goto copy_idxs_failed;
	}

	vals = kzalloc(vals_size, GFP_KERNEL);
	if (!vals) {
		rc = -ENOMEM;
		goto copy_idxs_failed;
	}

	for (; i < gpio_num; ++i)
		vals[i] =
				gpio_get_value(idxs[i]);

	if (copy_to_user(gpio_list->gpio_val_list, vals,
			vals_size)) {
		rc = -EFAULT;
		pr_err("%s copy_to_user fail %u\n",
				__func__,
				gpio_num);
	}

	kfree(vals);

copy_idxs_failed:
	kfree(idxs);

alloc_idxs_failed:
	return rc;
}

int msm_camera_diag_set_gpio_list(
		struct msm_ais_diag_gpio_list_t *gpio_list)
{
	int rc = 0;
	uint32_t gpio_num = gpio_list->gpio_num;
	uint32_t i = 0;
	int32_t val;
	int32_t *vals = NULL;
	uint32_t idxs_size = sizeof(uint32_t) * gpio_num;
	uint32_t vals_size = sizeof(int32_t) * gpio_num;
	uint32_t *idxs = kzalloc(idxs_size, GFP_KERNEL);

	if (!idxs) {
		rc = -ENOMEM;
		goto alloc_idxs_failed;
	}

	if (copy_from_user(idxs,
			(void __user *)(gpio_list->gpio_idx_list),
			idxs_size)) {
		rc = -EFAULT;
		pr_err("%s copy_from_user fail\n", __func__);
		goto copy_idxs_failed;
	}

	vals = kzalloc(vals_size, GFP_KERNEL);
	if (!vals) {
		rc = -ENOMEM;
		goto copy_idxs_failed;
	}

	if (copy_from_user(vals,
			(void __user *)(gpio_list->gpio_val_list),
			vals_size)) {
		rc = -EFAULT;
		pr_err("%s copy_from_user fail\n", __func__);
		goto copy_vals_failed;
	}

	for (; i < gpio_num; ++i) {
		gpio_set_value(idxs[i], vals[i]);
		val = gpio_get_value(idxs[i]);
		pr_debug("val set %d after %d\n", vals[i], val);
	}

copy_vals_failed:
	kfree(vals);

copy_idxs_failed:
	kfree(idxs);

alloc_idxs_failed:
	return rc;
}

int msm_camera_diag_update_ahb_state(enum cam_ahb_clk_vote vote)
{
	mutex_lock(&s_ddrbw.lock);
	s_ddrbw.bus_info.ahb_clk_vote_state = vote;
	mutex_unlock(&s_ddrbw.lock);
	return 0;
}

int msm_camera_diag_update_isp_state(
			uint32_t isp_bus_vector_idx,
			uint64_t isp_ab, uint64_t isp_ib)
{
	mutex_lock(&s_ddrbw.lock);
	s_ddrbw.bus_info.isp_bus_vector_idx = isp_bus_vector_idx;
	s_ddrbw.bus_info.isp_ab = isp_ab;
	s_ddrbw.bus_info.isp_ib = isp_ib;
	mutex_unlock(&s_ddrbw.lock);
	return 0;
}

int msm_camera_diag_get_ddrbw(struct msm_ais_diag_bus_info_t *info)
{
	int rc = 0;

	mutex_lock(&s_ddrbw.lock);
	info->ahb_clk_vote_state = s_ddrbw.bus_info.ahb_clk_vote_state;
	info->isp_bus_vector_idx = s_ddrbw.bus_info.isp_bus_vector_idx;
	info->isp_ab = s_ddrbw.bus_info.isp_ab;
	info->isp_ib = s_ddrbw.bus_info.isp_ib;

	mutex_unlock(&s_ddrbw.lock);
	return rc;
}


