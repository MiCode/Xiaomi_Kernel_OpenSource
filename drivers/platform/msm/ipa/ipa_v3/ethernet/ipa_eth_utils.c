/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include "ipa_eth_i.h"

static const char * const ipa_eth_device_events[IPA_ETH_DEV_EVENT_COUNT] = {
	[IPA_ETH_DEV_RESET_PREPARE] = "RESET_PREPARE",
	[IPA_ETH_DEV_RESET_COMPLETE] = "RESET_COMPLETE",
};

const char *ipa_eth_device_event_name(enum ipa_eth_device_event event)
{
	const char *name = "<unknown>";

	if (event < IPA_ETH_DEV_EVENT_COUNT && ipa_eth_device_events[event])
		name = ipa_eth_device_events[event];

	return name;
}
