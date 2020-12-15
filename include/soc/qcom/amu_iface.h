/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <asm/cputype.h>
#include <asm/sysreg.h>
#include <linux/ratelimit.h>


/* Definitions for system register interface to AMU for Cortex A76 and A77 CPUs */
#define SYS_A76_AM_EL0(crm, op2)		sys_reg(3, 3, 15, (crm), (op2))
/*
 * Cortex A76 activity monitors:
 *                op0  op1  CRn   CRm       op2
 * Counter:       11   011  1111  1001       n
 * Type:          11   011  1111  1010       n
 * n: 0-4
 */

#define SYS_A76_AMEVCNTR_EL0(n)		SYS_A76_AM_EL0(9, (n))
#define SYS_A76_AMEVTYPE_EL0(n)		SYS_A76_AM_EL0(10, (n))

/* Definitions for system register interface to AMU for Cortex A78 CPUs */
#define SYS_A78_AM_EL0(crm, op2)		sys_reg(3, 3, 15, (crm), (op2))
/*
 * Cortex A78 Group 0 of activity monitors:
 *                op0  op1  CRn   CRm       op2
 * Counter:       11   011  1111  100       n
 * Type:          11   011  1111  110       n
 * n: 0-3
 *
 * Cortex A78 Group 1 of activity monitors (auxiliary):
 *                op0  op1  CRn   CRm       op2
 * Counter:       11   011  1111  1100      n
 * Type:          11   011  1111  1110      n
 * n: 0-2
 */

#define SYS_A78_AMEVCNTR0_EL0(n)		SYS_A78_AM_EL0(4, (n))
#define SYS_A78_AMEVTYPE0_EL0(n)		SYS_A78_AM_EL0(6, (n))
#define SYS_A78_AMEVCNTR1_EL0(n)		SYS_A78_AM_EL0(12, (n))
#define SYS_A78_AMEVTYPE1_EL0(n)		SYS_A78_AM_EL0(14, (n))

static u64 read_amevctr(int n)
{
	u32 midr = read_cpuid_id();
	struct midr_range a76_midr_range = MIDR_ALL_VERSIONS(MIDR_CORTEX_A76);
	struct midr_range a77_midr_range = MIDR_ALL_VERSIONS(MIDR_CORTEX_A77);
	struct midr_range a78_midr_range = MIDR_ALL_VERSIONS(MIDR_CORTEX_A78);
	struct midr_range kryo_4g_midr_range = MIDR_ALL_VERSIONS(MIDR_KRYO4G);

	WARN_ON(preemptible());

	if (n < 0 || n > 4) {
		pr_err("%s Invalid counter ID: %d\n", __func__, n);
		return 0;
	}

	if (is_midr_in_range(midr, &a76_midr_range) ||
	    is_midr_in_range(midr, &a77_midr_range) ||
		is_midr_in_range(midr, &kryo_4g_midr_range)) {
		switch (n) {
		case 0:
			return read_sysreg_s(SYS_A76_AMEVCNTR_EL0(0));
		case 1:
			return read_sysreg_s(SYS_A76_AMEVCNTR_EL0(1));
		case 2:
			return read_sysreg_s(SYS_A76_AMEVCNTR_EL0(2));
		case 3:
			return read_sysreg_s(SYS_A76_AMEVCNTR_EL0(3));
		case 4:
			return read_sysreg_s(SYS_A76_AMEVCNTR_EL0(4));
		default:
			pr_err("%s Invalid counter ID: %d\n", __func__, n);
			return 0;
		}
	}

	if (is_midr_in_range(midr, &a78_midr_range)) {
		switch (n) {
		case 0:
			return read_sysreg_s(SYS_A78_AMEVCNTR0_EL0(0));
		case 1:
			return read_sysreg_s(SYS_A78_AMEVCNTR0_EL0(1));
		case 2:
			return read_sysreg_s(SYS_A78_AMEVCNTR0_EL0(2));
		case 3:
			return read_sysreg_s(SYS_A78_AMEVCNTR0_EL0(3));
		default:
			pr_err("%s Invalid counter ID: %d\n", __func__, n);
			return 0;
		}
	}

	return 0;
}

/* Core frequency cycles */
static u64 __maybe_unused read_amevctr_core(void)
{
	return read_amevctr(0);
}

/* Const frequency cycles */
static u64 __maybe_unused read_amevctr_const(void)
{
	return read_amevctr(1);
}

/* Instructions retired */
static u64 __maybe_unused read_amevctr_instret(void)
{
	return read_amevctr(2);
}

/* Memory stall cycles */
static u64 __maybe_unused read_amevctr_memstall(void)
{
	return read_amevctr(3);
}

/* High activity */
static u64 __maybe_unused read_amevctr_highact(void)
{
	return read_amevctr(4);
}
