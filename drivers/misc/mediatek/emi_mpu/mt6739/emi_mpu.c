/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#include <linux/timer.h>
#include <linux/workqueue.h>

#include <mt-plat/mtk_device_apc.h>
#include <mt-plat/sync_write.h>
/* #include "mach/irqs.h" */
#include <mt-plat/dma.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mt-plat/mtk_io.h>
#include "emi_mpu.h"
#include "emi_module.h"
#include <mt-plat/mtk_ccci_common.h>
#include <linux/delay.h>
#include <emi_bwl.h>
#include <linux/memblock.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> 
static int Violation_PortID = MASTER_ALL;

#define EMI_MPU_TEST	0
#if EMI_MPU_TEST
char mpu_test_buffer[0x20000] __aligned(PAGE_SIZE);
#endif

#define MAX_EMI_MPU_STORE_CMD_LEN 128
#define AXI_VIO_MONITOR_TIME	(1 * HZ)

#define DRIVER_ATTR(_name, _mode, _show, _store) \
 struct driver_attribute driver_attr_##_name = \
 __ATTR(_name, _mode, _show, _store)

static void __iomem *CEN_EMI_BASE;
static void __iomem *EMI_MPU_BASE;
static unsigned long long vio_addr;
static unsigned int emi_physical_offset;

static const char *UNKNOWN_MASTER = "unknown";
static DEFINE_SPINLOCK(emi_mpu_lock);


static int __match_id(u32 axi_id, int tbl_idx, u32 port_ID)
{
	int found = 0;

	if (((axi_id & mst_tbl[tbl_idx].id_mask) ==
	      mst_tbl[tbl_idx].id_val)) {
		if (port_ID == mst_tbl[tbl_idx].port) {
			pr_debug("Violation master name is %s.\n",
			       mst_tbl[tbl_idx].name);
			found += 1;
		}
	}

	return found;
}

static int isetocunt;
static void emi_mpu_set_violation_port(int portID)
{
	if (isetocunt == 0) {
		Violation_PortID = portID;
		isetocunt = 1;
	} else if (isetocunt == 2) {
		isetocunt = 0;
	}
}

int emi_mpu_get_violation_port(void)
{
	int ret;

	pr_info("[EMI MPU] emi_mpu_get_violation_port\n");

	if (isetocunt == 0) {
		pr_info("[EMI MPU] EMI did not set Port ID\n");
		isetocunt = 2;
		return MASTER_ALL;
	}

	ret = Violation_PortID;
	isetocunt = 0;

	if (ret >= MASTER_ALL)
		pr_info("[EMI MPU] Port ID: %d is an invalid Port ID\n", ret);
	else
		Violation_PortID = MASTER_ALL;

	return ret;
}

static const char *__id2name(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x00001FFF;
	port_ID = id & 0x00000007;
	emi_mpu_set_violation_port(port_ID);
	pr_info("[EMI MPU] axi_id = %x, port_id = %x\n", axi_ID, port_ID);

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}

static void __clear_emi_mpu_vio(void)
{
	u32 dbg_s, dbg_t, i;

	/* clear violation status */
	for (i = 0; i < EMI_MPU_DOMAIN_NUMBER; i++) {
		mt_reg_sync_writel(0xFFFFFFFF, EMI_MPUD_ST(i));  /* Region abort violation */
		mt_reg_sync_writel(0x3, EMI_MPUD_ST2(i));  /* Out of region abort violation */
	}

	/* clear debug info */
	mt_reg_sync_writel(0x80000000, EMI_MPUS);
	dbg_s = readl(IOMEM(EMI_MPUS));
	dbg_t = readl(IOMEM(EMI_MPUT));

	if (dbg_s) {
		pr_info("Fail to clear EMI MPU violation\n");
		pr_info("EMI_MPUS = %x, EMI_MPUT = %x", dbg_s, dbg_t);
	}
}

static int mpu_check_violation(void)
{
	u32 dbg_s, dbg_t, dbg_t_2nd, err;
	u32 master_ID, domain_ID, wr_vio, wr_oo_vio;
	s32 region;
	const char *master_name;

	dbg_s = readl(IOMEM(EMI_MPUS));
	dbg_t = readl(IOMEM(EMI_MPUT));
	dbg_t_2nd = readl(IOMEM(EMI_MPUT_2ND));
	vio_addr = (dbg_t + (((unsigned long long)(dbg_t_2nd & 0xF)) << 32) + emi_physical_offset);

	pr_info("Clear status.\n");

	master_ID = (dbg_s & 0x0000FFFF);
	domain_ID = (dbg_s >> 21) & 0x0000000F;
	wr_vio = (dbg_s >> 29) & 0x00000003;
	wr_oo_vio = (dbg_s >> 27) & 0x00000003;
	region = (dbg_s >> 16) & 0x1F;

	/*TBD: print the abort region*/

	pr_info("EMI MPU violation.\n");
	pr_info("[EMI MPU] Debug info start ----------------------------------------\n");

	pr_info("EMI_MPUS = %x, EMI_MPUT = %x, EMI_MPUT_2ND = %x.\n", dbg_s, dbg_t, dbg_t_2nd);
	pr_info("Current process is \"%s \" (pid: %i).\n", current->comm, current->pid);
	pr_info("Violation address is 0x%llx.\n", vio_addr);
	pr_info("Violation master ID is 0x%x.\n", master_ID);
	/*print out the murderer name*/
	master_name = __id2name(master_ID);
	pr_info("Violation domain ID is 0x%x.\n", domain_ID);
	if (wr_vio == 1)
		pr_info("Write violation.\n");
	else if (wr_vio == 2)
		pr_info("Read violation.\n");
	else
		pr_info("Strange write / read violation value = %d.\n", wr_vio);
	pr_info("Corrupted region is %d\n\r", region);
	if (wr_oo_vio == 1)
		pr_info("Write out of range violation.\n");
	else if (wr_oo_vio == 2)
		pr_info("Read out of range violation.\n");

	pr_info("[EMI MPU] Debug info end------------------------------------------\n");

#ifdef CONFIG_MTK_AEE_FEATURE
	if (wr_vio != 0) {
		if (((master_ID & 0x7) == MASTER_MDMCU) || ((master_ID & 0x7) == MASTER_MDDMA)) {
			char str[60] = "0";
			char *pstr = str;

			err = sprintf(pstr, "EMI_MPUS = 0x%x, ADDR = 0x%llx", dbg_s, vio_addr);

			exec_ccci_kern_func_by_md_id(0, ID_MD_MPU_ASSERT, str, strlen(str));

			if (err > 0)
				pr_info("[EMI MPU] MPU violation trigger MD str=%s strlen(str)=%d\n",
					str, (int)strlen(str));
		}

		aee_kernel_exception("EMI MPU",
"%sEMI_MPUS = 0x%x,EMI_MPUT = 0x%x,EMI_MPUT_2ND = 0x%x,vio_addr = 0x%llx\n%s%s\n",
		     "EMI MPU violation.\n",
		     dbg_s, dbg_t, dbg_t_2nd, vio_addr,
		     "CRDISPATCH_KEY:EMI MPU Violation Issue/",
		     master_name);
	}
#endif

	__clear_emi_mpu_vio();
	/* mt_devapc_clear_emi_violation(); */
	return 0;

}

/*EMI MPU violation handler*/
static irqreturn_t mpu_violation_irq(int irq, void *dev_id)
{
	pr_info("It's a MPU violation.\n");
	mpu_check_violation();
	return IRQ_HANDLED;
}

#if defined(CONFIG_MTK_PASR)
/* Acquire DRAM Setting for PASR/DPD */
static unsigned int get_segment_nr(unsigned int rk_index)
{
	unsigned int rank_size;
	unsigned int num_of_1;
	unsigned int i;

	rank_size = get_rank_size(rk_index) / get_ch_num();
	num_of_1 = 0;

	for (i = 0; i < 32; i++) {
		if (rank_size & 0x1)
			num_of_1++;
		rank_size = rank_size >> 1;
	}

	if (num_of_1 > 1)
		return 6;
	else
		return 8;
}
#if 0
void acquire_dram_setting(struct basic_dram_setting *pasrdpd)
{
	unsigned int ch, rk;

	pasrdpd->channel_nr = get_ch_num();

	for (ch = 0; ch < MAX_CHANNELS; ch++) {
		for (rk = 0; rk < MAX_RANKS; rk++) {
			if ((ch >= pasrdpd->channel_nr) || (rk >= get_rk_num())) {
				pasrdpd->channel[ch].rank[rk].valid_rank = false;
				pasrdpd->channel[ch].rank[rk].rank_size = 0;
				pasrdpd->channel[ch].rank[rk].segment_nr = 0;
				continue;
			}
			pasrdpd->channel[ch].rank[rk].valid_rank = true;
			pasrdpd->channel[ch].rank[rk].rank_size = get_rank_size(rk) / (pasrdpd->channel_nr);
			pasrdpd->channel[ch].rank[rk].segment_nr = get_segment_nr(rk);

		}
	}
}
#endif
#endif

/*
 * emi_mpu_set_region_protection: protect a region.
 * @start: start address of the region
 * @end: end address of the region
 * @region: EMI MPU region id
 * @access_permission: EMI MPU access permission
 * Return 0 for success, otherwise negative status code.
 */
int emi_mpu_set_region_protection(unsigned long long start,
unsigned long long end, int region, unsigned int access_permission)
{
	int ret = 0;
	unsigned int start_align;
	unsigned int end_align;
	unsigned long flags;
	struct arm_smccc_res res;
	
	access_permission = access_permission & 0x4FFFFFF;
	access_permission = access_permission | ((region & 0x1F)<<27);
	start_align = (unsigned int) (start >> 16);
	end_align = (unsigned int) (end >> 16);

	spin_lock_irqsave(&emi_mpu_lock, flags);
	arm_smccc_smc(MTK_SIP_KERNEL_EMIMPU_SET, start_align,end_align, access_permission, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(&emi_mpu_lock, flags);

	return ret;
}
EXPORT_SYMBOL(emi_mpu_set_region_protection);

static ssize_t emi_mpu_show(struct device_driver *driver, char *buf)
{
	ssize_t ret;
	unsigned long long start, end;
	unsigned int reg_value;
	int i, j, region;
	static const char *permission[7] = {
		"No",
		"S_RW",
		"S_RW_NS_R",
		"S_RW_NS_W",
		"S_R_NS_R",
		"FORBIDDEN",
		"S_R_NS_RW"
	};

	ret = 0;
	for (region = 0; region < EMI_MPU_REGION_NUMBER; region++) {
		start = ((unsigned long long)mt_emi_reg_read(EMI_MPU_SA(region)) << 16) + emi_physical_offset;
		end = ((unsigned long long)mt_emi_reg_read(EMI_MPU_EA(region)) << 16) + emi_physical_offset;
		if (ret >= PAGE_SIZE)
			break;
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "R%d-> 0x%llx to 0x%llx\n", region, start, end+0xFFFF);

		/* for(i=0; i<(EMI_MPU_DOMAIN_NUMBER/8); i++) { */
		/* only print first 8 domain */
		for (i = 0; i < 1; i++) {
			reg_value = mt_emi_reg_read(EMI_MPU_APC(region, i*8));
			for (j = 0; j < 8; j++) {
				if (ret < PAGE_SIZE) {
					ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s, ",
						permission[((reg_value >> (j*3)) & 0x7)]);
				}
				if (((j == 3) || (j == 7)) && (ret < PAGE_SIZE))
					ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
			}
		}
		if (ret < PAGE_SIZE)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}


#if EMI_MPU_TEST
	{
		int temp;

		temp = (*((volatile unsigned int *)(mpu_test_buffer+0x10000)));
		pr_info("mpu_test_buffer+10000 = 0x%x\n", temp);
	}
#endif

	return strlen(buf);
}

static ssize_t emi_mpu_store(struct device_driver *driver,
const char *buf, size_t count)
{
	int i;
	unsigned long long start_addr = 0;
	unsigned long long end_addr = 0;
	unsigned long region = 0;
	unsigned long long access_permission = 0;
	char *command;
	char *ptr;
	char *token[6];
	ssize_t ret;

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_info("emi_mpu_store command overflow.");
		return count;
	}
	pr_info("emi_mpu_store: %s\n", buf);

	command = kmalloc((size_t) MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;

	strncpy(command, buf, (size_t) MAX_EMI_MPU_STORE_CMD_LEN);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_MPU_STR, strlen(EN_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_devel("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++)
			pr_devel("token[%d] = %s\n", i, token[i]);

		/* kernel standardization
		 * start_addr = simple_strtoul(token[1], &token[1], 16);
		 * end_addr = simple_strtoul(token[2], &token[2], 16);
		 * region = simple_strtoul(token[3], &token[3], 16);
		 * access_permission = simple_strtoul(token[4], &token[4], 16);
		 */
		ret = kstrtoull(token[1], 16, &start_addr);
		if (ret != 0)
			pr_info("kstrtoul fails to parse start_addr");
		ret = kstrtoull(token[2], 16, &end_addr);
		if (ret != 0)
			pr_info("kstrtoul fails to parse end_addr");
		ret = kstrtoul(token[3], 10, (unsigned long *)&region);
		if (ret != 0)
			pr_info("kstrtoul fails to parse region");
		ret = kstrtoull(token[4], 16, &access_permission);
		if (ret != 0)
			pr_info("kstrtoull fails to parse access_permission");
		emi_mpu_set_region_protection(start_addr, end_addr, region, access_permission);
		pr_info("Set EMI_MPU: start: 0x%llx, end: 0x%llx, region: %lx, permission: 0x%llx.\n",
		       start_addr, end_addr, region, access_permission);
	} else if (!strncmp(buf, DIS_MPU_STR, strlen(DIS_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_devel("token[%d] = %s\n", i, token[i]);
			i++;
		}
		/* kernel standardization
		 * start_addr = simple_strtoul(token[1], &token[1], 16);
		 * end_addr = simple_strtoul(token[2], &token[2], 16);
		 * region = simple_strtoul(token[3], &token[3], 16);
		 */
		ret = kstrtoull(token[1], 16, &start_addr);
		if (ret != 0)
			pr_info("kstrtoul fails to parse start_addr");
		ret = kstrtoull(token[2], 16, &end_addr);
		if (ret != 0)
			pr_info("kstrtoul fails to parse end_addr");
		ret = kstrtoul(token[3], 10, (unsigned long *)&region);
		if (ret != 0)
			pr_info("kstrtoul fails to parse region");

		emi_mpu_set_region_protection(0x0, 0x0, region,
		SET_ACCESS_PERMISSON(UNLOCK,
			NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
			NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));

		pr_info("set EMI MPU: start: 0x%x, end: 0x%x, region: %lx, permission: 0x%llx\n",
		0, 0, region,
		SET_ACCESS_PERMISSON(UNLOCK,
			NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
			NO_PROTECTION, NO_PROTECTION, NO_PROTECTION, NO_PROTECTION));
	} else {
		pr_info("Unknown emi_mpu command.\n");
	}

	kfree(command);

	return count;
}

DRIVER_ATTR(mpu_config, 0644, emi_mpu_show, emi_mpu_store);

#if ENABLE_AP_REGION
#define AP_REGION_ID   23
static void protect_ap_region(void)
{
	unsigned int ap_mem_mpu_id;
	unsigned int  ap_mem_mpu_attr;
	unsigned long long kernel_base;
	phys_addr_t dram_size;

	kernel_base = emi_physical_offset;
/*	dram_size = get_max_DRAM_size();*/
	dram_size = memblock_end_of_DRAM() - 1 - memblock_start_of_DRAM();

	ap_mem_mpu_id = AP_REGION_ID;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(LOCK,
		FORBIDDEN, SEC_R_NSEC_RW, FORBIDDEN, NO_PROTECTION,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);

	emi_mpu_set_region_protection(kernel_base,
		(kernel_base+dram_size-1), ap_mem_mpu_id, ap_mem_mpu_attr);

}
#endif

static struct platform_driver emi_mpu_ctrl = {
	.driver = {
		.name = "emi_mpu_ctrl",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
	.id_table = NULL,
};

static int __init emi_mpu_mod_init(void)
{
	int ret;
	unsigned int mpu_irq = 0;

#if EMI_MPU_TEST
	pr_info("[EMI_MPU] ptr_phyical: %llx\n", (unsigned long long)__pa(mpu_test_buffer));
	*((volatile unsigned int *)(mpu_test_buffer+0x10000)) = 0xdeaddead;
#endif

	pr_info("Initialize EMI MPU.\n");

	isetocunt = 0;

	/* DTS version */
	CEN_EMI_BASE = mt_cen_emi_base_get();
	EMI_MPU_BASE = mt_emi_mpu_base_get();

	if (CEN_EMI_BASE == NULL) {
		pr_info("[EMI MPU] can't get CEN_EMI_BASE\n");
		return -1;
	}

	if (EMI_MPU_BASE == NULL) {
		pr_info("[EMI MPU] can't get EMI_MPU_BASE\n");
		return -1;
	}

	mt_emi_reg_base_set(CEN_EMI_BASE);
	emi_physical_offset = 0x40000000;

	pr_info("[EMI MPU] EMI_MPUS = 0x%x\n", readl(IOMEM(EMI_MPUS)));
	pr_info("[EMI MPU] EMI_MPUT = 0x%x\n", readl(IOMEM(EMI_MPUT)));
	pr_info("[EMI MPU] EMI_MPUT_2ND = 0x%x\n", readl(IOMEM(EMI_MPUT_2ND)));

	if (readl(IOMEM(EMI_MPUS))) {
		pr_info("[EMI MPU] get MPU violation in driver init\n");
		mpu_check_violation();
	} else {
		__clear_emi_mpu_vio();
	}

	/*
	 * NoteXXX: Interrupts of violation (including SPC in SMI, or EMI MPU)
	 *		  are triggered by the device APC.
	 *		  Need to share the interrupt with the SPC driver.
	 */
	mpu_irq = mt_emi_mpu_irq_get();
	if (mpu_irq != 0) {
		ret = request_irq(mpu_irq,
			(irq_handler_t)mpu_violation_irq, IRQF_TRIGGER_LOW | IRQF_SHARED,
			"mt_emi_mpu", &emi_mpu_ctrl);
		if (ret != 0) {
			pr_info("Fail to request EMI_MPU interrupt. Error = %d.\n", ret);
			return ret;
		}
	}

#if ENABLE_AP_REGION
	protect_ap_region();
#endif

#if !defined(USER_BUILD_KERNEL)
	/* register driver and create sysfs files */
	ret = platform_driver_register(&emi_mpu_ctrl);
	if (ret)
		pr_info("Fail to register EMI_MPU driver.\n");

	ret = driver_create_file(&emi_mpu_ctrl.driver, &driver_attr_mpu_config);
	if (ret)
		pr_info("Fail to create MPU config sysfs file.\n");
#endif

	return 0;
}

static void __exit emi_mpu_mod_exit(void)
{
}

module_init(emi_mpu_mod_init);
module_exit(emi_mpu_mod_exit);

