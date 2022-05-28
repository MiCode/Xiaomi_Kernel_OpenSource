/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DVFS_QOS_RAW_H
#define __MTK_CAM_DVFS_QOS_RAW_H
enum QOS_RAW_PORT_ID {
	imgo_r1 = 0,
	cqi_r1,
	cqi_r2,
	bpci_r1,
	lsci_r1,
	rawi_r2,/* 5 */
	rawi_r3,
	ufdi_r2,
	ufdi_r3,
	rawi_r4,
	rawi_r5,/* 10 */
	aai_r1,
	ufdi_r5,
	fho_r1,
	aao_r1,
	tsfso_r1,/* 15 */
	flko_r1,
	yuvo_r1,
	yuvo_r3,
	yuvco_r1,
	yuvo_r2,/* 20 */
	rzh1n2to_r1,
	drzs4no_r1,
	tncso_r1,
};

/* note: may be platform dependent */
static int qos_raw_ids[] = {
	imgo_r1,
	cqi_r1,
	cqi_r2,
	bpci_r1,
	lsci_r1,
	rawi_r2,
	rawi_r3,
	ufdi_r2,
	ufdi_r3,
	rawi_r4,
	rawi_r5,
	aai_r1,
	ufdi_r5,
	fho_r1,
	aao_r1,
	tsfso_r1,
	flko_r1,
};

static int qos_yuv_ids[] = {
	yuvo_r1,
	yuvo_r3,
	yuvco_r1,
	yuvo_r2,
	rzh1n2to_r1,
	drzs4no_r1,
	tncso_r1,
};

#endif /* __MTK_CAM_DVFS_QOS_RAW_H */
