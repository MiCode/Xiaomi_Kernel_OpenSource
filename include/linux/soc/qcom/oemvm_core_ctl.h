/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef __OEMVM_CORE_CTL_H
#define __OEMVM_CORE_CTL_H

#include <linux/types.h>
#include <linux/gunyah/gh_common.h>

int oemvm_core_ctl_yield(int vcpu);
int oemvm_core_ctl_restore_vcpu(int vcpu);

#endif
