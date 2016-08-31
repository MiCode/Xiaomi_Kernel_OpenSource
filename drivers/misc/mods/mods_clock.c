/*
 * mods_clock.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"
#include <linux/clk.h>
#include <mach/clk.h>
#include <../arch/arm/mach-tegra/clock.h>

static struct list_head mods_clock_handles;
static spinlock_t mods_clock_lock;
static NvU32 last_handle;

struct clock_entry {
	struct clk *pclk;
	NvU32 handle;
	struct list_head list;
};

void mods_init_clock_api(void)
{
	spin_lock_init(&mods_clock_lock);
	INIT_LIST_HEAD(&mods_clock_handles);
	last_handle = 0;
}

void mods_shutdown_clock_api(void)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct list_head *tmp;

	spin_lock(&mods_clock_lock);

	list_for_each_safe(iter, tmp, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		list_del(iter);
		MEMDBG_FREE(entry);
	}

	spin_unlock(&mods_clock_lock);
}

static NvU32 mods_get_clock_handle(struct clk *pclk)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct clock_entry *entry = 0;
	NvU32 handle = 0;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *cur
			= list_entry(iter, struct clock_entry, list);
		if (cur->pclk == pclk) {
			entry = cur;
			handle = cur->handle;
			break;
		}
	}

	if (!entry) {
		MEMDBG_ALLOC(entry, sizeof(*entry));
		if (!unlikely(!entry)) {
			entry->pclk = pclk;
			entry->handle = ++last_handle;
			handle = entry->handle;
			list_add(&entry->list, &mods_clock_handles);
		}
	}

	spin_unlock(&mods_clock_lock);

	return handle;
}

static struct clk *mods_get_clock(NvU32 handle)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct clk *pclk = 0;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		if (entry->handle == handle) {
			pclk = entry->pclk;
			break;
		}
	}

	spin_unlock(&mods_clock_lock);

	return pclk;
}

int esc_mods_get_clock_handle(struct file *pfile,
			      struct MODS_GET_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	p->device_name[sizeof(p->device_name)-1] = 0;
	p->controller_name[sizeof(p->controller_name)-1] = 0;
	pclk = clk_get_sys(p->device_name, p->controller_name);

	if (IS_ERR(pclk)) {
		mods_error_printk("invalid clock specified: dev=%s, ctx=%s\n",
				  p->device_name, p->controller_name);
	} else {
		p->clock_handle = mods_get_clock_handle(pclk);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		ret = clk_set_rate(pclk, p->clock_rate_hz);
		if (ret) {
			mods_error_printk(
				"unable to set rate %lluHz on clock 0x%x\n",
				p->clock_rate_hz, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
				  "successfuly set rate %lluHz on clock 0x%x\n",
				  p->clock_rate_hz, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		p->clock_rate_hz = clk_get_rate(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x has rate %lluHz\n",
				  p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_max_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else if (!pclk->ops || !pclk->ops->round_rate) {
		mods_error_printk(
			"unable to detect max rate for clock handle 0x%x\n",
			p->clock_handle);
	} else {
		long rate = pclk->ops->round_rate(pclk, pclk->max_rate);
		p->clock_rate_hz = rate < 0 ? pclk->max_rate
					    : (unsigned long)rate;
		mods_debug_printk(DEBUG_CLOCK,
				  "clock 0x%x has max rate %lluHz\n",
				  p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_max_rate(struct file *pfile, struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
#ifdef CONFIG_TEGRA_CLOCK_DEBUG_FUNC
		ret = tegra_clk_set_max(pclk, p->clock_rate_hz);
		if (ret) {
			mods_error_printk(
		"unable to override max clock rate %lluHz on clock 0x%x\n",
					  p->clock_rate_hz, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
			  "successfuly set max rate %lluHz on clock 0x%x\n",
					  p->clock_rate_hz, p->clock_handle);
		}
#else
		mods_error_printk("unable to override max clock rate\n");
		mods_error_printk(
		"reconfigure kernel with CONFIG_TEGRA_CLOCK_DEBUG_FUNC=y\n");
		ret = -ENOSYS;
#endif
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_parent(struct file *pfile, struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = 0;
	struct clk *pparent = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);
	pparent = mods_get_clock(p->clock_parent_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else if (!pparent) {
		mods_error_printk("unrecognized parent clock handle: 0x%x\n",
				  p->clock_parent_handle);
	} else {
		ret = clk_set_parent(pclk, pparent);
		if (ret) {
			mods_error_printk(
			    "unable to make clock 0x%x parent of clock 0x%x\n",
			    p->clock_parent_handle, p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK,
			  "successfuly made clock 0x%x parent of clock 0x%x\n",
			  p->clock_parent_handle, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_parent(struct file *pfile, struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		struct clk *pparent = clk_get_parent(pclk);
		p->clock_parent_handle = mods_get_clock_handle(pparent);
		mods_debug_printk(DEBUG_CLOCK,
				  "clock 0x%x is parent of clock 0x%x\n",
				  p->clock_parent_handle, p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_enable_clock(struct file *pfile, struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		ret = clk_enable(pclk);
		if (ret) {
			mods_error_printk("unable to enable clock 0x%x\n",
					  p->clock_handle);
		} else {
			mods_debug_printk(DEBUG_CLOCK, "clock 0x%x enabled\n",
					  p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_disable_clock(struct file *pfile, struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		clk_disable(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x disabled\n",
				  p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_is_clock_enabled(struct file *pfile, struct MODS_CLOCK_ENABLED *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		p->enable_count = pclk->refcnt;
		mods_debug_printk(DEBUG_CLOCK,
				  "clock 0x%x enable count is %u\n",
				  p->clock_handle, p->enable_count);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_clock_reset_assert(struct file *pfile,
				struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		tegra_periph_reset_assert(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x reset asserted\n",
				  p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_clock_reset_deassert(struct file *pfile,
				  struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = 0;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		mods_error_printk("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		tegra_periph_reset_deassert(pclk);
		mods_debug_printk(DEBUG_CLOCK, "clock 0x%x reset deasserted\n",
				  p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}
