/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CORESIGHT_TMC_ETH_H
#define _CORESIGHT_TMC_ETH_H


#define ETR_ETH_SIZE 0x400

/*
 * @ tmcdrvdata: Etr driver data.
 */
struct tmc_eth_data {
	struct tmc_drvdata	*tmcdrvdata;
};

extern int tmc_eth_enable(struct tmc_eth_data *eth_data);
extern void tmc_eth_disable(struct tmc_eth_data *eth_data);

#endif
