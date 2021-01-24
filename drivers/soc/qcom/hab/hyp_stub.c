// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */
#include "hab.h"

int hab_hypervisor_register(void)
{
	hab_driver.b_loopback = 1;

	return 0;
}

void hab_hypervisor_unregister(void)
{
	hab_hypervisor_unregister_common();
}
