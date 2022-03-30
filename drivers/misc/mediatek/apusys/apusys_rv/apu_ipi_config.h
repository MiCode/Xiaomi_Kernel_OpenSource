/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_IPI_CONFIG_H
#define APU_IPI_CONFIG_H

#include "apu_ipi.h"

#define IPI_LOCKED		(1)
#define IPI_UNLOCKED		(0)

#define IPI_HOST_INITIATE	(0)
#define IPI_APU_INITIATE	(1)

#define IPI_WITH_ACK		(1)
#define IPI_WITHOUT_ACK		(0)

static const struct {
	char *name;
	unsigned int direction:1;
	unsigned int ack:1;
} ipi_attrs[APU_IPI_MAX] = {
	[APU_IPI_INIT] = { // 0
		.name = "init",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITHOUT_ACK,
	},
	[APU_IPI_NS_SERVICE] = { // 1
		.name = "name-service",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITHOUT_ACK,
	},
	[APU_IPI_DEEP_IDLE] = { // 2
		.name = "deep_idle",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_CTRL_RPMSG] = { // 3
		.name = "apu-ctrl-rpmsg",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_MIDDLEWARE] = { // 4
		.name = "apu-mdw",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_REVISER_RPMSG] = { // 5
		.name = "apu-reviser",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_PWR_TX] = { // 6
		.name = "apu-pwr-tx",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_PWR_RX] = { // 7
		.name = "apu-pwr-rx",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_MDLA_TX] = { // 8
		.name = "apu-mdla-tx",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_MDLA_RX] = { // 9
		.name = "apu-mdla-rx",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_TIMESYNC] = { // 10
		.name = "timesync",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_EDMA_TX] = { // 11
		.name = "apu-edma-rpmsg",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITHOUT_ACK,
	},
	[APU_IPI_MNOC_TX] = { // 12
		.name = "apu-mnoc-rpmsg",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITHOUT_ACK,
	},
	[APU_IPI_MVPU_TX] = { // 13
		.name = "apu-mvpu-tx",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_MVPU_RX] = { // 14
		.name = "apu-mvpu-rx",
		.direction = IPI_APU_INITIATE,
		.ack = IPI_WITH_ACK,
	},
	[APU_IPI_LOG_LEVEL] = { // 15
		.name = "apu-log-level",
		.direction = IPI_HOST_INITIATE,
		.ack = IPI_WITH_ACK,
	},
};


#endif /* APU_IPI_CONFIG_H */

