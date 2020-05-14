/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

#include <linux/neuron.h>

/* Device's driver data */
struct neuron_service {
	struct neuron_protocol *protocol;
	struct neuron_application *application;
	unsigned int channel_count;
	struct neuron_channel *channels[];
};
