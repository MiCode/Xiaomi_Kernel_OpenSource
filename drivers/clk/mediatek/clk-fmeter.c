// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "clk-fmeter.h"

static const struct fmeter_ops *fm_ops;

const struct fmeter_clk *mt_get_fmeter_clks(void)
{
	if (fm_ops == NULL || fm_ops->get_fmeter_clks == NULL)
		return NULL;

	return  fm_ops->get_fmeter_clks();
}
EXPORT_SYMBOL(mt_get_fmeter_clks);

unsigned int mt_get_ckgen_freq(unsigned int id)
{
	if (fm_ops == NULL || fm_ops->get_ckgen_freq == NULL)
		return 0;

	return  fm_ops->get_ckgen_freq(id);
}
EXPORT_SYMBOL(mt_get_ckgen_freq);

unsigned int mt_get_abist_freq(unsigned int id)
{
	if (fm_ops == NULL || fm_ops->get_abist_freq == NULL)
		return 0;

	return  fm_ops->get_abist_freq(id);
}
EXPORT_SYMBOL(mt_get_abist_freq);

unsigned int mt_get_abist2_freq(unsigned int id)
{
	if (fm_ops == NULL || fm_ops->get_abist2_freq == NULL)
		return 0;

	return  fm_ops->get_abist2_freq(id);
}
EXPORT_SYMBOL(mt_get_abist2_freq);

unsigned int mt_get_vlpck_freq(unsigned int id)
{
	if (fm_ops == NULL || fm_ops->get_vlpck_freq == NULL)
		return 0;

	return  fm_ops->get_vlpck_freq(id);
}
EXPORT_SYMBOL(mt_get_vlpck_freq);

unsigned int mt_get_subsys_freq(unsigned int id)
{
	if (fm_ops == NULL || fm_ops->get_subsys_freq == NULL)
		return 0;

	return  fm_ops->get_subsys_freq(id);
}
EXPORT_SYMBOL(mt_get_subsys_freq);

unsigned int mt_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (fm_ops == NULL || fm_ops->get_fmeter_freq == NULL)
		return 0;

	return  fm_ops->get_fmeter_freq(id, type);
}
EXPORT_SYMBOL(mt_get_fmeter_freq);

int mt_get_fmeter_id(enum FMETER_ID fid)
{
	if (fm_ops == NULL || fm_ops->get_fmeter_id == NULL)
		return FID_NULL;

	return  fm_ops->get_fmeter_id(fid);
}
EXPORT_SYMBOL(mt_get_fmeter_id);

int mt_subsys_freq_register(struct fm_subsys *fm, unsigned int size)
{
	if (fm_ops == NULL || fm_ops->subsys_freq_register == NULL)
		return -EINVAL;

	return  fm_ops->subsys_freq_register(fm, size);
}
EXPORT_SYMBOL(mt_subsys_freq_register);

void fmeter_set_ops(const struct fmeter_ops *ops)
{
	fm_ops = ops;
}
EXPORT_SYMBOL(fmeter_set_ops);

