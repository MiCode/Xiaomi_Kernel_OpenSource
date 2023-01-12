/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef DIGITAL_CDC_RSC_MGR_H
#define DIGITAL_CDC_RSC_MGR_H

#ifdef CONFIG_DIGITAL_CDC_RSC_MGR

int digital_cdc_rsc_mgr_hw_vote_enable(struct clk* vote_handle);
void digital_cdc_rsc_mgr_hw_vote_disable(struct clk* vote_handle);
void digital_cdc_rsc_mgr_hw_vote_reset(struct clk* vote_handle);

#else

static inline int digital_cdc_rsc_mgr_hw_vote_enable(struct clk* vote_handle)
{
	return 0;
}

static inline void digital_cdc_rsc_mgr_hw_vote_disable(struct clk* vote_handle)
{
}

static inline void digital_cdc_rsc_mgr_hw_vote_reset(struct clk* vote_handle)
{
}

#endif /* CONFIG_DIGITAL_CDC_RSC_MGR */

#endif /* DIGITAL_CDC_RSC_MGR_H */
