#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/aee.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_power_gs.h>

#include <mach/mt_pmic_wrap.h>
#include <mach/pmic_mt6331_6332_sw.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>

#ifndef CONFIG_ARM64
#define gs_read(addr) (*(volatile u32 *)(addr))
#endif

struct proc_dir_entry *mt_power_gs_dir = NULL;

static kal_uint16 gs6331_pmic_read(kal_uint16 reg)
{
	kal_uint32 ret = 0;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return (kal_uint16)reg_val;
}

static kal_uint16 gs6332_pmic_read(kal_uint16 reg)
{
	kal_uint32 ret = 0;
	kal_uint32 reg_val = 0;

	ret = pmic_read_interface(reg + 0x8000, &reg_val, 0xFFFF, 0x0);

	return (kal_uint16)reg_val;
}

static void mt_power_gs_compare_pll(char *scenario)
{
	int i;
	#define ID_NAME(n)	{n, __stringify(n)}

	struct {
		const int id;
		const char *name;
	} plls[NR_PLLS] = {
		ID_NAME(ARMCA15PLL),
		ID_NAME(ARMCA7PLL),
		ID_NAME(MAINPLL),
		ID_NAME(MSDCPLL),
		ID_NAME(UNIVPLL),
		ID_NAME(MMPLL),
		ID_NAME(VENCPLL),
		ID_NAME(TVDPLL),
		ID_NAME(MPLL),
		ID_NAME(VCODECPLL),
		ID_NAME(APLL1),
		ID_NAME(APLL2),
	};

	struct {
		const int id;
		const char *name;
	} subsyss[NR_SYSS] = {
		ID_NAME(SYS_MD1),
		ID_NAME(SYS_DIS),
//		ID_NAME(SYS_MFG_ASYNC),
//		ID_NAME(SYS_MFG_2D),
		ID_NAME(SYS_MFG),
		ID_NAME(SYS_ISP),
		ID_NAME(SYS_VDE),
		ID_NAME(SYS_MJC),
		ID_NAME(SYS_VEN),
		ID_NAME(SYS_AUD),
	};

	for (i = 3; i < NR_PLLS; i++) {
		if(i == 5 || i == 8)
			continue;
		if (pll_is_on(i)) {
			printk("%s: on\n", plls[i].name);
			if (!strcmp(scenario, "Suspend")) {
				printk("suspend warning: %s is on!!!\n", plls[i].name);
				printk("warning! warning! warning! it may cause resume fail\n");
			}
		}
	}

	for (i = 0; i < NR_SYSS; i++) {
		if (subsys_is_on(i)) {
			printk("%s: on\n", subsyss[i].name);
			if (!strcmp(scenario, "Suspend") && (i > SYS_MD1)) {
				//aee_kernel_warning("Suspend Warning","%s is on", subsyss[i].name);
				printk("suspend warning: %s is on!!!\n", subsyss[i].name);
				printk("warning! warning! warning! it may cause resume fail\n");
#ifdef CONFIG_CLKMGR_STAT
				if(i == SYS_DIS) {
					clk_stat_check(i);
				}
#endif      
			}	
		}	
	}
}

void mt_power_gs_diff_output(unsigned int val1, unsigned int val2)
{
	int i = 0;
	unsigned int diff = val1 ^ val2;

	while (diff != 0) {
		if ((diff % 2) != 0) printk("%d ", i);

		diff /= 2;
		i++;
	}

	printk("\n");
}

void mt_power_gs_compare(char *scenario, \
             unsigned int *ap_analog_gs, unsigned int ap_analog_gs_len, \
			 unsigned int *ap_cg_gs, unsigned int ap_cg_gs_len, \
			 unsigned int *ap_dcm_gs, unsigned int ap_dcm_gs_len, \
			 unsigned int *pmic_6331_gs, unsigned int pmic_6331_gs_len, \
			 unsigned int *pmic_6332_gs, unsigned int pmic_6332_gs_len)
{
	unsigned int i, val1, val2;
	
#ifndef CONFIG_ARM64
    // AP Analog
    for (i = 0; i < ap_analog_gs_len; i += 3) {
		aee_sram_printk("%d\n", i);
		val1 = gs_read(ap_analog_gs[i]) & ap_analog_gs[i + 1];
		val2 = ap_analog_gs[i + 2] & ap_analog_gs[i + 1];

		if (val1 != val2) {
			printk("%s - AP ANALOG - 0x%x - 0x%x - 0x%x - 0x%x - ", \
			       scenario, ap_analog_gs[i], gs_read(ap_analog_gs[i]), ap_analog_gs[i + 1], ap_analog_gs[i + 2]);
			mt_power_gs_diff_output(val1, val2);
		}
	}

	// AP CG
	for (i = 0; i < ap_cg_gs_len; i += 3) {
		aee_sram_printk("%d\n", i);
		val1 = gs_read(ap_cg_gs[i]) & ap_cg_gs[i + 1];
		val2 = ap_cg_gs[i + 2] & ap_cg_gs[i + 1];

		if (val1 != val2) {
			printk("%s - AP CG - 0x%x - 0x%x - 0x%x - 0x%x - ", \
			       scenario, ap_cg_gs[i], gs_read(ap_cg_gs[i]), ap_cg_gs[i + 1], ap_cg_gs[i + 2]);
			mt_power_gs_diff_output(val1, val2);
		}
	}

	// AP DCM
	for (i = 0; i < ap_dcm_gs_len; i += 3) {
		aee_sram_printk("%d\n", i);
		val1 = gs_read(ap_dcm_gs[i]) & ap_dcm_gs[i + 1];
		val2 = ap_dcm_gs[i + 2] & ap_dcm_gs[i + 1];

		if (val1 != val2) {
			printk("%s - AP DCM - 0x%x - 0x%x - 0x%x - 0x%x - ", \
			       scenario, ap_dcm_gs[i], gs_read(ap_dcm_gs[i]), ap_dcm_gs[i + 1],
			       ap_dcm_gs[i + 2]);
			mt_power_gs_diff_output(val1, val2);
		}
	}
#endif

	// 6331
	for (i = 0; i < pmic_6331_gs_len; i += 3) {
		aee_sram_printk("%d\n", i);
		val1 = gs6331_pmic_read(pmic_6331_gs[i]) & pmic_6331_gs[i + 1];
		val2 = pmic_6331_gs[i + 2] & pmic_6331_gs[i + 1];

		if (val1 != val2) {
			printk("%s - 6331 - 0x%x - 0x%x - 0x%x - 0x%x - ", \
			       scenario, pmic_6331_gs[i], gs6331_pmic_read(pmic_6331_gs[i]),
			       pmic_6331_gs[i + 1], pmic_6331_gs[i + 2]);
			mt_power_gs_diff_output(val1, val2);
		}
	}

	// 6332
	for (i = 0; i < pmic_6332_gs_len; i += 3) {
		aee_sram_printk("%d\n", i);
		val1 = gs6332_pmic_read(pmic_6332_gs[i]) & pmic_6332_gs[i + 1];
		val2 = pmic_6332_gs[i + 2] & pmic_6332_gs[i + 1];

		if (val1 != val2) {
			printk("%s - 6332 - 0x%x - 0x%x - 0x%x - 0x%x - ", \
			       scenario, pmic_6332_gs[i], gs6332_pmic_read(pmic_6332_gs[i]),
			       pmic_6332_gs[i + 1], pmic_6332_gs[i + 2]);
			mt_power_gs_diff_output(val1, val2);
		}
	}

	mt_power_gs_compare_pll(scenario);
}
EXPORT_SYMBOL(mt_power_gs_compare);

static void __exit mt_power_gs_exit(void)
{
	//return 0;
}

static int __init mt_power_gs_init(void)
{
	mt_power_gs_dir = proc_mkdir("mt_power_gs", NULL);

	if (!mt_power_gs_dir)
		printk("[%s]: mkdir /proc/mt_power_gs failed\n", __FUNCTION__);

	return 0;
}

module_init(mt_power_gs_init);
module_exit(mt_power_gs_exit);

MODULE_DESCRIPTION("MT Low Power Golden Setting");
