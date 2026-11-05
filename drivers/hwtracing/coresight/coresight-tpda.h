/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CORESIGHT_CORESIGHT_TPDA_H
#define _CORESIGHT_CORESIGHT_TPDA_H

#define TPDA_CR			(0x000)
#define TPDA_Pn_CR(n)		(0x004 + (n * 4))
#define TPDA_FPID_CR		(0x084)
#define TPDA_FREQREQ_VAL	(0x088)
#define TPDA_SYNCR		(0x08C)
#define TPDA_FLUSH_CR		(0x090)
#define TPDA_FLUSH_SR		(0x094)
#define TPDA_FLUSH_ERR		(0x098)

#define TPDA_MAX_INPORTS	32

struct tpda_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct mutex		lock;
	bool			enable;
	uint32_t		atid;
	uint32_t		bc_esize[TPDA_MAX_INPORTS];
	uint32_t		tc_esize[TPDA_MAX_INPORTS];
	uint32_t		dsb_esize[TPDA_MAX_INPORTS];
	uint32_t		cmb_esize[TPDA_MAX_INPORTS];
	bool			trig_async;
	bool			trig_flag_ts;
	bool			trig_freq;
	bool			freq_ts;
	uint32_t		freq_req_val;
	bool			freq_req;
	bool			cmbchan_mode;
	struct clk		*atclk;
};


#endif  /* _CORESIGHT_CORESIGHT_TPDA_H */
