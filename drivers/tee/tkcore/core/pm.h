/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TKCORE_PM_H
#define TKCORE_PM_H

int tkcore_stay_awake(void *fn, void *data);

int tkcore_tee_pm_init(void);

void tkcore_tee_pm_exit(void);

#endif
