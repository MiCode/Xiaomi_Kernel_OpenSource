/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LB_POLICY_SHIFT 16
#define LB_CPU_MASK ((1 << LB_POLICY_SHIFT) - 1)

#define LB_PREV          (0x0  << LB_POLICY_SHIFT)
#define LB_EAS           (0x1  << LB_POLICY_SHIFT)
#define LB_WAKE_AFFINE   (0x2  << LB_POLICY_SHIFT)
#define LB_IDLEST        (0x4  << LB_POLICY_SHIFT)
#define LB_IDLE_SIBLING  (0x8  << LB_POLICY_SHIFT)

#ifdef CONFIG_MTK_SCHED_INTEROP
extern bool is_rt_throttle(int cpu);
#endif
