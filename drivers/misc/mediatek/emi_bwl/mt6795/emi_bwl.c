#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/xlog.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "mach/mt_reg_base.h"
#include "mach/sync_write.h"
#include "emi_bwl.h"

static void __iomem *DRAMCAO_BASE_ADDR;
static void __iomem *DDRPHY_BASE_ADDR;
static void __iomem *DRAMCNAO_BASE_ADDR;
static void __iomem *EMI_BASE_ADDR;

DEFINE_SEMAPHORE(emi_bwl_sem);

static struct platform_driver mem_bw_ctrl = {
	.driver = {
		   .name = "mem_bw_ctrl",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_driver ddr_type = {
	.driver = {
		   .name = "ddr_type",
		   .owner = THIS_MODULE,
		   },
};

extern u32 get_dram_data_rate(void);
unsigned int fixup_emi_setting_vss(unsigned int cur_dram_freq, int skip);
#if 0

static struct platform_driver dramc_high = {
	.driver = {
		   .name = "dramc_high",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_driver mem_bw_finetune_md = {
	.driver = {
		   .name = "mem_bw_finetune_md",
		   .owner = THIS_MODULE,
		   },
};

static struct platform_driver mem_bw_finetune_mm = {
	.driver = {
		   .name = "mem_bw_finetune_mm",
		   .owner = THIS_MODULE,
		   },
};
#endif

/* define EMI bandwiwth limiter control table */
static struct emi_bwl_ctrl ctrl_tbl[NR_CON_SCE];

/* current concurrency scenario */
static int cur_con_sce = 0x0FFFFFFF;

/* define concurrency scenario strings */
static const char *con_sce_str[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) #con_sce,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};



/****************** For LPDDR3-1866 ******************/

static const unsigned int emi_arba_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_lpddr3_1866_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "con_sce_lpddr3_1866.h"
#undef X_CON_SCE
};


/****************** For 2 x LPDDR3-1600******************/

static const unsigned int emi_arba_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arba,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbb_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbb,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbc_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbc,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbd_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbd,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbe_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbe,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbf_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbf,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

static const unsigned int emi_arbg_2xlpddr3_1600_val[] = {
#define X_CON_SCE(con_sce, arba, arbb, arbc, arbd, arbe, arbf, arbg) arbg,
#include "con_sce_2xlpddr3_1600.h"
#undef X_CON_SCE
};

int get_ddr_type(void)
{
	unsigned int value;

	value = DRAMC_READ(DRAMC_LPDDR2);
	if ((value >> 28) & 0x1)	//check LPDDR2_EN
	{
		return LPDDR2;
	}
#if 0
	value = DRAMC_READ(DRAMC_PADCTL4);
	if ((value >> 7) & 0x1)	//check DDR3_EN
	{
		if (DRAMC_READ(DRAMC_CONF1) & 0x1) {
			return DDR3_32;
		} else {
			return DDR3_16;
		}
	}
#endif

	value = DRAMC_READ(DRAMC_ACTIM1);
	if ((value >> 28) & 0x1)	//check LPDDR3_EN
	{

		value = readl(IOMEM(EMI_CONA));
		if (value & 0x01)	//support 2 channel
		{
			return DUAL_LPDDR3_1600;
		} else {
			return LPDDR3_1866;
		}
	}

	return mDDR;
}

/*
 * mtk_mem_bw_ctrl: set EMI bandwidth limiter for memory bandwidth control
 * @sce: concurrency scenario ID
 * @op: either ENABLE_CON_SCE or DISABLE_CON_SCE
 * Return 0 for success; return negative values for failure.
 */
int mtk_mem_bw_ctrl(int sce, int op)
{
	int i, highest;

	if (sce >= NR_CON_SCE) {
		return -1;
	}

	if (op != ENABLE_CON_SCE && op != DISABLE_CON_SCE) {
		return -1;
	}
	if (in_interrupt()) {
		return -1;
	}

	down(&emi_bwl_sem);

	if (op == ENABLE_CON_SCE) {
		ctrl_tbl[sce].ref_cnt++;
	} else if (op == DISABLE_CON_SCE) {
		if (ctrl_tbl[sce].ref_cnt != 0) {
			ctrl_tbl[sce].ref_cnt--;
		}
	}

	/* find the scenario with the highest priority */
	highest = -1;
	for (i = 0; i < NR_CON_SCE; i++) {
		if (ctrl_tbl[i].ref_cnt != 0) {
			highest = i;
			break;
		}
	}
	if (highest == -1) {
		highest = CON_SCE_NORMAL;
	}

	/* set new EMI bandwidth limiter value */
	if (highest != cur_con_sce) {

		if (get_ddr_type() == DUAL_LPDDR3_1600) {
			mt_reg_sync_writel(emi_arba_2xlpddr3_1600_val[highest], EMI_ARBA);
			//mt_reg_sync_writel(emi_arbb_2xlpddr3_1600_val[highest], EMI_ARBB);
			mt_reg_sync_writel(emi_arbc_2xlpddr3_1600_val[highest], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_2xlpddr3_1600_val[highest], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_2xlpddr3_1600_val[highest], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_2xlpddr3_1600_val[highest], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_2xlpddr3_1600_val[highest], EMI_ARBG);
		} else if (get_ddr_type() == LPDDR3_1866) {
			mt_reg_sync_writel(emi_arba_lpddr3_1866_val[highest], EMI_ARBA);
			//mt_reg_sync_writel(emi_arbb_lpddr3_1866_val[highest], EMI_ARBB);
			mt_reg_sync_writel(emi_arbc_lpddr3_1866_val[highest], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_lpddr3_1866_val[highest], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_lpddr3_1866_val[highest], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_lpddr3_1866_val[highest], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_lpddr3_1866_val[highest], EMI_ARBG);
		}

		cur_con_sce = highest;
		/* fix up CON_SCE_VSS setting when dram freq == 1333*/
		fixup_emi_setting_vss(get_dram_data_rate(),1);
	}
	up(&emi_bwl_sem);

	return 0;
}
unsigned int fixup_emi_setting_vss(u32 cur_dram_freq, int skip)
{
	if (CON_SCE_VSS == cur_con_sce){
		if (1333 == cur_dram_freq){
			//dram frequence: 1333
			mt_reg_sync_writel(0x40107a45, EMI_ARBA);
			mt_reg_sync_writel(0x808070cf, EMI_ARBC);
			mt_reg_sync_writel(0x08107048, EMI_ARBD);
			mt_reg_sync_writel(0x30406048, EMI_ARBE);
			mt_reg_sync_writel(0x808070d3, EMI_ARBF);
			mt_reg_sync_writel(0xffff7049, EMI_ARBG);
			return 0;
		}else if (1 != skip){
			//other dram frequence: 1466,1600
			//not skip adjust
			mt_reg_sync_writel(emi_arba_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBA);
			mt_reg_sync_writel(emi_arbc_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBC);
			mt_reg_sync_writel(emi_arbd_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBD);
			mt_reg_sync_writel(emi_arbe_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBE);
			mt_reg_sync_writel(emi_arbf_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBF);
			mt_reg_sync_writel(emi_arbg_2xlpddr3_1600_val[CON_SCE_VSS], EMI_ARBG);
			return 0;
		}
	}
	return 1;
}
/*
 * ddr_type_show: sysfs ddr_type file show function.
 * @driver:
 * @buf: the string of ddr type
 * Return the number of read bytes.
 */
static ssize_t ddr_type_show(struct device_driver *driver, char *buf)
{
	if (get_ddr_type() == LPDDR2) {
		sprintf(buf, "LPDDR2\n");
	} else if (get_ddr_type() == DDR3_16) {
		sprintf(buf, "DDR3_16\n");
	} else if (get_ddr_type() == DDR3_32) {
		sprintf(buf, "DDR3_32\n");
	} else if (get_ddr_type() == DUAL_LPDDR3_1600) {
		sprintf(buf, "LPDDR3\n");
	} else if (get_ddr_type() == LPDDR3_1866) {
		sprintf(buf, "LPDDR3\n");
	} else {
		sprintf(buf, "mDDR\n");
	}

	return strlen(buf);
}

/*
 * ddr_type_store: sysfs ddr_type file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t ddr_type_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*do nothing */
	return count;
}

DRIVER_ATTR(ddr_type, 0644, ddr_type_show, ddr_type_store);

/*
 * con_sce_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t con_sce_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	int i = 0;
	if (cur_con_sce >= NR_CON_SCE) {
		ptr += sprintf(ptr, "none\n");
	} else {
		ptr += sprintf(ptr, "current scenario: %s\n", con_sce_str[cur_con_sce]);
	}
#if 1
	ptr += sprintf(ptr, "%s\n", con_sce_str[cur_con_sce]);
	ptr += sprintf(ptr, "EMI_ARBA = 0x%x \n",  readl(IOMEM(EMI_ARBA)));
	ptr += sprintf(ptr, "EMI_ARBC = 0x%x \n",  readl(IOMEM(EMI_ARBC)));
	ptr += sprintf(ptr, "EMI_ARBD = 0x%x \n",  readl(IOMEM(EMI_ARBD)));
	ptr += sprintf(ptr, "EMI_ARBE = 0x%x \n",  readl(IOMEM(EMI_ARBE)));
	ptr += sprintf(ptr, "EMI_ARBF = 0x%x \n",  readl(IOMEM(EMI_ARBF)));
	ptr += sprintf(ptr, "EMI_ARBG = 0x%x \n",  readl(IOMEM(EMI_ARBG)));
	for (i = 0; i < NR_CON_SCE; i++){
		ptr += sprintf(ptr, "%s = 0x%x \n", con_sce_str[i], ctrl_tbl[i].ref_cnt);
	}
	pr_notice("[EMI BWL] EMI_ARBA = 0x%x \n", readl(IOMEM(EMI_ARBA)));
	pr_notice("[EMI BWL] EMI_ARBC = 0x%x \n", readl(IOMEM(EMI_ARBC)));
	pr_notice("[EMI BWL] EMI_ARBD = 0x%x \n", readl(IOMEM(EMI_ARBD)));
	pr_notice("[EMI BWL] EMI_ARBE = 0x%x \n", readl(IOMEM(EMI_ARBE)));
	pr_notice("[EMI BWL] EMI_ARBF = 0x%x \n", readl(IOMEM(EMI_ARBF)));
	pr_notice("[EMI BWL] EMI_ARBG = 0x%x \n", readl(IOMEM(EMI_ARBG)));
#endif

	return strlen(buf);
}

/*
 * con_sce_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t con_sce_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;

	for (i = 0; i < NR_CON_SCE; i++) {
		if (!strncmp(buf, con_sce_str[i], strlen(con_sce_str[i]))) {
			if (!strncmp
			    (buf + strlen(con_sce_str[i]) + 1, EN_CON_SCE_STR,
			     strlen(EN_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, ENABLE_CON_SCE);
				pr_notice("concurrency scenario %s ON\n", con_sce_str[i]);
				break;
			} else
			    if (!strncmp
				(buf + strlen(con_sce_str[i]) + 1, DIS_CON_SCE_STR,
				 strlen(DIS_CON_SCE_STR))) {
				mtk_mem_bw_ctrl(i, DISABLE_CON_SCE);
				pr_notice("concurrency scenario %s OFF\n", con_sce_str[i]);
				break;
			}
		}
	}

	return count;
}

DRIVER_ATTR(concurrency_scenario, 0644, con_sce_show, con_sce_store);


/*
 * finetune_md_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t finetune_md_show(struct device_driver *driver, char *buf)
{
	unsigned int dram_type;

	dram_type = get_ddr_type();

	if (dram_type == LPDDR2) {	/*LPDDR2. FIXME */
		switch (cur_con_sce) {
		case CON_SCE_VR:
			sprintf(buf, "true");
			break;
		default:
			sprintf(buf, "false");
			break;
		}
	} else if (dram_type == DDR3_16) {	/*DDR3-16bit. FIXME */
	 /*TBD*/} else if (dram_type == DDR3_32) {	/*DDR3-32bit. FIXME */
	 /*TBD*/} else if (dram_type == DUAL_LPDDR3_1600) {	/*2XLPDDR3-1600. FIXME */
	 /*TBD*/} else if (dram_type == LPDDR3_1866) {	/*LPDDR3-1866. FIXME */
	 /*TBD*/} else if (dram_type == mDDR) {	/*mDDR. FIXME */
	 /*TBD*/} else {
		/*unkown dram type */
		sprintf(buf, "ERROR: unkown dram type!");
	}

	return strlen(buf);
}

/*
 * finetune_md_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t finetune_md_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*Do nothing */
	return count;
}

DRIVER_ATTR(finetune_md, 0644, finetune_md_show, finetune_md_store);


/*
 * finetune_mm_show: sysfs con_sce file show function.
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t finetune_mm_show(struct device_driver *driver, char *buf)
{
	unsigned int dram_type;

	dram_type = get_ddr_type();

	if (dram_type == LPDDR2) {	/*LPDDR2. FIXME */
		switch (cur_con_sce) {
		default:
			sprintf(buf, "false");
			break;
		}
	} else if (dram_type == DDR3_16) {	/*DDR3-16bit. FIXME */
	 /*TBD*/} else if (dram_type == DDR3_32) {	/*DDR3-32bit. FIXME */
	 /*TBD*/} else if (dram_type == DUAL_LPDDR3_1600) {	/*2XLPDDR3-1600. FIXME */
	 /*TBD*/} else if (dram_type == LPDDR3_1866) {	/*LPDDR3-1866. FIXME */
	 /*TBD*/} else if (dram_type == mDDR) {	/*mDDR. FIXME */
	 /*TBD*/} else {
		/*unkown dram type */
		sprintf(buf, "ERROR: unkown dram type!");
	}

	return strlen(buf);
}

/*
 * finetune_md_store: sysfs con_sce file store function.
 * @driver:
 * @buf:
 * @count:
 * Return the number of write bytes.
 */
static ssize_t finetune_mm_store(struct device_driver *driver, const char *buf, size_t count)
{
	/*Do nothing */
	return count;
}

DRIVER_ATTR(finetune_mm, 0644, finetune_mm_show, finetune_mm_store);

#if 0
/*
 * dramc_high_show: show the status of DRAMC_HI_EN
 * @driver:
 * @buf:
 * Return the number of read bytes.
 */
static ssize_t dramc_high_show(struct device_driver *driver, char *buf)
{
	unsigned int dramc_hi;

	dramc_hi = (readl(EMI_TESTB) >> 12) & 0x1;
	if (dramc_hi == 1)
		return sprintf(buf, "DRAMC_HI is ON\n");
	else
		return sprintf(buf, "DRAMC_HI is OFF\n");
}

/*
 dramc_hign_store: enable/disable DRAMC untra high. WARNING: ONLY CAN BE ENABLED AT MD_STANDALONE!!!
 * @driver:
 * @buf: need to be "0" or "1"
 * @count:
 * Return the number of write bytes.
*/
static ssize_t dramc_high_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	unsigned int emi_testb;

	if (sscanf(buf, "%u", &value) != 1)
		return -EINVAL;

	emi_testb = readl(EMI_TESTB);

	if (value == 1) {
		emi_testb |= 0x1000;	/* Enable DRAM_HI */
		mt_reg_sync_writel(emi_testb, EMI_TESTB);
	} else if (value == 0) {
		emi_testb &= ~0x1000;	/* Disable DRAM_HI */
		mt_reg_sync_writel(emi_testb, EMI_TESTB);
	} else
		return -EINVAL;

	return count;
}

DRIVER_ATTR(dramc_high, 0644, dramc_high_show, dramc_high_store);
#endif
/*
 * emi_bwl_mod_init: module init function.
 */
static int __init emi_bwl_mod_init(void)
{
	int ret;
	struct device_node *node;
	
  /* DTS version */
	node = of_find_compatible_node(NULL, NULL, "mediatek,EMI");
	if (node) {
		  EMI_BASE_ADDR = of_iomap(node, 0);
		  printk("get EMI_BASE_ADDR @ %p\n", EMI_BASE_ADDR);
	} else {
		  printk("can't find compatible node\n");
		  return -1;
	}
	
	node = of_find_compatible_node(NULL, NULL, "mediatek,DRAMC0");
	if (node) {
      DRAMCAO_BASE_ADDR = of_iomap(node, 0);
      printk("get DRAMCAO_BASE_ADDR @ %p\n", DRAMCAO_BASE_ADDR);
  }
  else {
      printk("can't find DRAMC0 compatible node\n");
      return -1;
  }

  node = of_find_compatible_node(NULL, NULL, "mediatek,DDRPHY");
  if(node) {
      DDRPHY_BASE_ADDR = of_iomap(node, 0);
      printk("get DDRPHY_BASE_ADDR @ %p\n", DDRPHY_BASE_ADDR);
  }
  else {
      printk("can't find DDRPHY compatible node\n");
      return -1;
  }

  node = of_find_compatible_node(NULL, NULL, "mediatek,DRAMC_NAO");
  if(node) {
      DRAMCNAO_BASE_ADDR = of_iomap(node, 0);
      printk("get DRAMCNAO_BASE_ADDR @ %p\n", DRAMCNAO_BASE_ADDR);
  }
  else {
      printk("can't find DRAMCNAO compatible node\n");
		return -1;
	}

#if 0				//[CM Huang] TBD 20131115
	if (get_ddr_type() == LPDDR2)	//LPDDR2
	{
		/* apply co-sim result for LPDDR2. */
		mt_reg_sync_writel(0x14212836, EMI_CONB);
		mt_reg_sync_writel(0x0f131314, EMI_CONC);
		mt_reg_sync_writel(0x14212836, EMI_COND);
		mt_reg_sync_writel(0x0f131314, EMI_CONE);
		mt_reg_sync_writel(0x0f131428, EMI_CONG);
		mt_reg_sync_writel(0x0f131428, EMI_CONH);
		/* testing for MD2 timing fail */
		mt_reg_sync_writel(0x0c8f0ccd, EMI_SLCT);
		//mt_reg_sync_writel(0x088b08cd, EMI_SLCT);
		mt_reg_sync_writel(0x00720038, EMI_ARBK);
		mt_reg_sync_writel(0x00720038, EMI_ARBK_2ND);
		mt_reg_sync_writel(0x84462f2f, EMI_ARBJ);
		mt_reg_sync_writel(0x84462f2f, EMI_ARBJ_2ND);
		mt_reg_sync_writel(0x10202488, EMI_ARBI);
		mt_reg_sync_writel(0x10202488, EMI_ARBI_2ND);
		mt_reg_sync_writel(0x00070714, EMI_TESTB);
		//mt_reg_sync_writel(0x00070754, EMI_TESTB);
		//mt_reg_sync_writel(0x10000000, EMI_TESTD);
	}

	else if (get_ddr_type() == DDR3_16)	//DDR3-16bit
	{
		//write overhead value
		mt_reg_sync_writel(0x0B0B0E17, EMI_CONB);	//read  overhead for 4~1
		mt_reg_sync_writel(0x0B0B0B0B, EMI_CONC);	//read  overhead for 8~5
		mt_reg_sync_writel(0x1012161E, EMI_COND);	//write overhead for 4~1
		mt_reg_sync_writel(0x0B0B0D0E, EMI_CONE);	//write overhead for 8~5
	} else if (get_ddr_type() == DDR3_32) {
		/* apply co-sim result for LPDDR2. */
		mt_reg_sync_writel(0x14212836, EMI_CONB);
		mt_reg_sync_writel(0x0f131314, EMI_CONC);
		mt_reg_sync_writel(0x14212836, EMI_COND);
		mt_reg_sync_writel(0x0f131314, EMI_CONE);
		mt_reg_sync_writel(0x0f131428, EMI_CONG);
		mt_reg_sync_writel(0x0f131428, EMI_CONH);
		/* testing for MD2 timing fail */
		mt_reg_sync_writel(0x088b08cd, EMI_SLCT);
		mt_reg_sync_writel(0x00720038, EMI_ARBK);
		mt_reg_sync_writel(0x00720038, EMI_ARBK_2ND);
		mt_reg_sync_writel(0x84462f2f, EMI_ARBJ);
		mt_reg_sync_writel(0x84462f2f, EMI_ARBJ_2ND);
		mt_reg_sync_writel(0x10202488, EMI_ARBI);
		mt_reg_sync_writel(0x10202488, EMI_ARBI_2ND);
		mt_reg_sync_writel(0x00070714, EMI_TESTB);
		//mt_reg_sync_writel(0x00070754, EMI_TESTB);
		//mt_reg_sync_writel(0x10000000, EMI_TESTD);
	} else if (get_ddr_type() == LPDDR3) {
		//TBD
	}
	*/
	else			//mDDR
	{
		mt_reg_sync_writel(0x2B2C2C2E, EMI_CONB);	//read  overhead for 4~1
		mt_reg_sync_writel(0x2627292B, EMI_CONC);	//read  overhead for 8~5
		mt_reg_sync_writel(0x2B2C2C2E, EMI_COND);	//write overhead for 4~1
		mt_reg_sync_writel(0x2627292B, EMI_CONE);	//write overhead for 8~5
	}
#endif

	//write Filter Priority Encode
	//writel(0x01812488, EMI_ARBI); //TBD. need to set EMI_ARBI_2ND???

	ret = mtk_mem_bw_ctrl(CON_SCE_NORMAL, ENABLE_CON_SCE);
	if (ret) {
		pr_err("fail to set EMI bandwidth limiter\n");
	}

	/* Register BW ctrl interface */
	ret = platform_driver_register(&mem_bw_ctrl);
	if (ret) {
		pr_err("fail to register EMI_BW_LIMITER driver\n");
	}

	ret = driver_create_file(&mem_bw_ctrl.driver, &driver_attr_concurrency_scenario);
	if (ret) {
		pr_err("fail to create EMI_BW_LIMITER sysfs file\n");
	}

	/* Register DRAM type information interface */
	ret = platform_driver_register(&ddr_type);
	if (ret) {
		pr_err("fail to register DRAM_TYPE driver\n");
	}

	ret = driver_create_file(&ddr_type.driver, &driver_attr_ddr_type);
	if (ret) {
		pr_err("fail to create DRAM_TYPE sysfs file\n");
	}
#if 0
	/* Register DRAMC ultra high interface */
	ret = platform_driver_register(&dramc_high);
	ret = driver_create_file(&dramc_high.driver, &driver_attr_dramc_high);
	if (ret) {
		pr_err("fail to create DRAMC_HIGH sysfs file\n");
	}
#endif
#if 0
	/* Register MD feature fine-tune interface */
	ret = platform_driver_register(&mem_bw_finetune_md);
	if (ret) {
		pr_err("fail to register EMI_BW_FINETUNE_MD driver\n");
	}

	ret = driver_create_file(&mem_bw_finetune_md.driver, &driver_attr_finetune_md);
	if (ret) {
		pr_err("fail to create EMI_BW_FINETUNE_MD sysfs file\n");
	}

	/* Register MM feature fine-tune interface */
	ret = platform_driver_register(&mem_bw_finetune_mm);
	if (ret) {
		pr_err("fail to register EMI_BW_FINETUNE_MM driver\n");
	}

	ret = driver_create_file(&mem_bw_finetune_mm.driver, &driver_attr_finetune_mm);
	if (ret) {
		pr_err("fail to create EMI_BW_FINETUNE_MM sysfs file\n");
	}
#endif

	return 0;
}

/*
 * emi_bwl_mod_exit: module exit function.
 */
static void __exit emi_bwl_mod_exit(void)
{
}

EXPORT_SYMBOL(get_ddr_type);

module_init(emi_bwl_mod_init);
module_exit(emi_bwl_mod_exit);
