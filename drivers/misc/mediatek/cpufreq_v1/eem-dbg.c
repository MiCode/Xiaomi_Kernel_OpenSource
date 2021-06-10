// SPDX-License-Identifier: GPL-2.0
/*
 * eem-dbg.c - eem debug Driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Chienwei Chang <chienwei.chang@mediatek.com>
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "mtk_cpu_dbg.h"
#include "eem-dbg-v1.h"
#include "../mcupm/include/mcupm_driver.h"
#include "../mcupm/include/mcupm_ipi_id.h"
#define EEM_LOG_ENABLE 1

unsigned int sn_mcysys_reg_base[NUM_SN_CPU] = {
	MCUSYS_CPU0_BASEADDR,
	MCUSYS_CPU1_BASEADDR,
	MCUSYS_CPU2_BASEADDR,
	MCUSYS_CPU3_BASEADDR,
	MCUSYS_CPU4_BASEADDR,
	MCUSYS_CPU5_BASEADDR,
	MCUSYS_CPU6_BASEADDR,
	MCUSYS_CPU7_BASEADDR
};

unsigned short sn_mcysys_reg_dump_off[SIZE_SN_MCUSYS_REG] = {
	0x520,
	0x524,
	0x528,
	0x52C,
	0x530,
	0x534,
	0x538,
	0x53C,
	0x540,
	0x544
};

static struct eemsn_devinfo eem_devinfo;
int ipi_ackdata;
unsigned int eem_seq;
static struct eemsn_dbg_log *eemsn_log;
phys_addr_t picachu_sn_mem_base_phys;
phys_addr_t eem_log_phy_addr, eem_log_virt_addr;
uint32_t eem_log_size;
static int eem_aging_dump_proc_show(struct seq_file *m, void *v);
void __iomem *eem_base;
void __iomem *eem_csram_base;
void __iomem *sn_base;

#define for_each_det(det) \
		for (det = eemsn_detectors; \
		det < (eemsn_detectors + ARRAY_SIZE(eemsn_detectors)); \
		det++)

static unsigned int eem_to_cpueb(unsigned int cmd,
	struct eem_ipi_data *eem_data)
{
	unsigned int ret;

	eem_data->cmd = cmd;
	ret = mtk_ipi_send_compl(get_mcupm_ipidev(), CH_S_EEMSN,
		/*IPI_SEND_WAIT*/IPI_SEND_POLLING, eem_data,
		sizeof(struct eem_ipi_data)/MBOX_SLOT_SIZE, 2000);
	return ret;
}

int get_volt_cpu(struct eemsn_det *det)
{
	unsigned int value = 0;
	enum eemsn_det_id cpudvfsindex;

	/* unit mv * 100 = 10uv */
	cpudvfsindex = detid_to_dvfsid(det);

	pr_debug("proc voltage = %d~~~\n", value);
	return value;
}

void get_freq_table_cpu(struct eemsn_det *det)
{
#ifdef CONFIG_MTK_CPU_FREQ
	int i = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] =
			mt_cpufreq_get_freq_by_idx(cpudvfsindex, i) / 1000;

		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;

#endif
}


/* get original volt from cpu dvfs, and apply this table to dvfs
 *   when ptp need to restore volt
 */
void get_orig_volt_table_cpu(struct eemsn_det *det)
{
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_cpufreq_get_volt_by_idx(cpudvfsindex, i);

		det->volt_tbl_orig[i] =
			(unsigned char)base_ops_volt_2_pmic(det, volt);


	}

#endif
}

int base_ops_volt_2_pmic(struct eemsn_det *det, int volt)
{
	return (((volt) - det->pmic_base +
		det->pmic_step - 1) / det->pmic_step);
}

int base_ops_volt_2_eem(struct eemsn_det *det, int volt)
{
	return (((volt) - det->eemsn_v_base +
		det->eemsn_step - 1) / det->eemsn_step);
}

int base_ops_pmic_2_volt(struct eemsn_det *det, int pmic_val)
{
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eem_2_pmic(struct eemsn_det *det, int eem_val)
{
	return ((((eem_val) * det->eemsn_step) + det->eemsn_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}



struct eemsn_det_ops big_det_ops = {
	.get_volt		= get_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,

	/* interface to PMIC */
	.volt_2_pmic = base_ops_volt_2_pmic,
	.volt_2_eem = base_ops_volt_2_eem,
	.pmic_2_volt = base_ops_pmic_2_volt,
	.eem_2_pmic = base_ops_eem_2_pmic,
};

struct eemsn_det eemsn_detectors[NR_EEMSN_DET] = {
	[EEMSN_DET_L] = {
		.name			= "EEM_DET_L",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_L,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON | FEA_SEN,
		.volt_offset = 0,
		.max_freq_khz = L_FREQ_BASE,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,
	},
	[EEMSN_DET_B] = {
		.name			= "EEM_DET_B",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_B,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON | FEA_SEN,
		.max_freq_khz = B_FREQ_BASE,
		.mid_freq_khz = B_M_FREQ_BASE,
		.volt_offset = 0,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,
	},
	[EEMSN_DET_CCI] = {
		.name			= "EEM_DET_CCI",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_CCI,
		.features	= FEA_INIT02 | FEA_MON,
		.max_freq_khz = CCI_FREQ_BASE, /* 1248Mhz */
		.volt_offset = 0,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,
	},
};

unsigned int detid_to_dvfsid(struct eemsn_det *det)
{
	unsigned int cpudvfsindex;
	enum eemsn_det_id detid = det_to_id(det);

	if (detid == EEMSN_DET_L)
		cpudvfsindex = MT_CPU_DVFS_LL;
	else if (detid == EEMSN_DET_B)
		cpudvfsindex = MT_CPU_DVFS_L;
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;
	pr_debug("[%s] id:%d, cpudvfsindex:%d\n", __func__,
		detid, cpudvfsindex);
	return cpudvfsindex;
}

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */
/*
 * show current EEM stauts
 */
static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s\n",
		((char *)(det->name) + 8),
		det->disabled ? "disabled" : "enable"
		);
	return 0;
}
/*
 * set EEM status by procfs interface
 */
static ssize_t eem_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_det *det = (struct eemsn_det *)PDE_DATA(file_inode(file));
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	pr_debug("in eem debug proc write 2~~~~~~~~\n");
	buf[count] = '\0';
	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;
		pr_debug("in eem debug proc write 3~~~~~~~~\n");
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = enabled;
		ipi_ret = eem_to_cpueb(IPI_EEMSN_DEBUG_PROC_WRITE, &eem_data);
		det->disabled = enabled;
	} else
		ret = -EINVAL;
out:
	pr_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	return (ret < 0) ? ret : count;
}
/*
 * show current aging margin
 */
static int eem_setmargin_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;
	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] volt clamp:%d\n",
		   ((char *)(det->name) + 8),
		   det->volt_clamp);
	return 0;
}
/*
 * remove aging margin
 */
static ssize_t eem_setmargin_proc_write(struct file *file,
			const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int aging_val[2];
	int i = 0;
	int start_oft, end_oft;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eemsn_det *det = (struct eemsn_det *)PDE_DATA(file_inode(file));
	char *tok;
	char *cmd_str = NULL;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';
	cmd_str = strsep(&buf, " ");
	if (cmd_str == NULL)
		ret = -EINVAL;
	while ((tok = strsep(&buf, " ")) != NULL) {
		if (i == 3) {
			pr_info("number of arguments > 3!\n");
			goto out;
		}
		if (kstrtoint(tok, 10, &aging_val[i])) {
			pr_info("Invalid input: %s\n", tok);
			goto out;
		} else
			i++;
	}
	if (!strncmp(cmd_str, "aging", sizeof("aging"))) {
		start_oft = aging_val[0];
		end_oft = aging_val[1];
		ret = count;
	} else if (!strncmp(cmd_str, "clamp", sizeof("clamp"))) {
		if (aging_val[0] < 20)
			det->volt_clamp = aging_val[0];
		ret = count;
	} else {
		ret = -EINVAL;
		goto out;
	}
out:
	pr_debug("in eem debug proc write 4~~~~~~~~\n");
	free_page((unsigned long)buf);
	return ret;
}

static void dump_sndata_to_de(struct seq_file *m)
{
	int *val = (int *)&eem_devinfo;
	int i, j, addr;

	seq_printf(m,
	"[%d]=================Start EEMSN dump===================\n",
	eem_seq++);

	for (i = 0; i < sizeof(struct eemsn_devinfo) / sizeof(unsigned int);
		i++)
		seq_printf(m, "[%d]M_HW_RES%d\t= 0x%08X\n",
		eem_seq++, ((i >= IDX_HW_RES_SN) ? (i + 3) : i), val[i]);

	seq_printf(m, "[%d]Start dump_CPE:\n", eem_seq++);
	for (i = 0; i < MIN_SIZE_SN_DUMP_CPE; i++) {
		if (i == 5)
			addr = (int)SN_CPEIRQSTS;
		else if (i == 6)
			addr = (int)SN_CPEINTSTSRAW;
		else
			addr = (int)(SN_COMPAREDVOP + i * 4);

		seq_printf(m, "[%d]0x%x = 0x%x\n",
			eem_seq++, addr,
			eemsn_log->sn_log.reg_dump_cpe[i]);

	}
	seq_printf(m, "[%d]Start dump_sndata:\n", eem_seq++);
	for (i = 0; i < SIZE_SN_DUMP_SENSOR; i++) {
		seq_printf(m, "[%d]0x%x = 0x%x\n",
			eem_seq++, (SN_C0ASENSORDATA + i * 4),
			eemsn_log->sn_log.reg_dump_sndata[i]);
	}

	seq_printf(m, "[%d]start dump_sn_cpu:\n", eem_seq++);
	for (i = 0; i < NUM_SN_CPU; i++) {
		for (j = 0; j < SIZE_SN_MCUSYS_REG; j++) {
			seq_printf(m, "[%d]0x%x = 0x%x\n",
				eem_seq++, (sn_mcysys_reg_base[i] +
				sn_mcysys_reg_dump_off[j]),
				eemsn_log->sn_log.reg_dump_sn_cpu[i][j]);
		}
	}
	seq_printf(m,
	"[%d]=================End EEMSN dump===================\n",
	eem_seq++);


}

static int eem_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
	unsigned int locklimit = 0;
	unsigned char lock;
	enum sn_det_id i;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_DUMP_PROC_SHOW, &eem_data);
	seq_printf(m, "ipi_ret:%d\n", ipi_ret);

	seq_printf(m, "[%d]========Start sn_trigger_sensing!\n", eem_seq++);
	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}
#ifdef EEM_LOG_ENABLE
	for (i = 0; i < NR_SN_DET; i++) {

		if (i == SN_DET_B)
			seq_printf(m, "[%d]T_SVT_HV_BCPU:%d %d %d %d\n",
				eem_seq++, eem_devinfo.T_SVT_HV_BCPU,
				eem_devinfo.T_SVT_LV_BCPU,
				eemsn_log->sn_cal_data[i].T_SVT_HV_RT,
				eemsn_log->sn_cal_data[i].T_SVT_LV_RT);
		else
			seq_printf(m, "[%d]T_SVT_HV_LCPU:%d %d %d %d\n",
				eem_seq, eem_devinfo.T_SVT_HV_LCPU,
				eem_devinfo.T_SVT_LV_LCPU,
				eemsn_log->sn_cal_data[i].T_SVT_HV_RT,
				eemsn_log->sn_cal_data[i].T_SVT_LV_RT);

		seq_printf(m, "[%d]id:%d, ATE_Temp_decode:%d, T_SVT_current:%d, ",
			eem_seq++, i, eem_devinfo.ATE_TEMP,
			eemsn_log->sn_log.sd[i].T_SVT_current);
		seq_printf(m, "Sensor_Volt_HT:%d, Sensor_Volt_RT:%d\n",
			eemsn_log->sn_log.sd[i].Sensor_Volt_HT,
			eemsn_log->sn_log.sd[i].Sensor_Volt_RT);
		seq_printf(m, "[%d]SN_Vmin:0x%x, CPE_Vmin:0x%x, init2[0]:0x%x, ",
			eem_seq++, eemsn_log->sn_log.sd[i].SN_Vmin,
			eemsn_log->sn_log.sd[i].CPE_Vmin,
			eemsn_log->det_vlog[i].volt_tbl_init2[0]);
		seq_printf(m, "sn_aging:%d, SN_temp:%d, CPE_temp:%d\n",
			eemsn_log->sn_cal_data[i].sn_aging,
			eemsn_log->sn_log.sd[i].SN_temp,
			eemsn_log->sn_log.sd[i].CPE_temp);

		seq_printf(m, "cur_opp:%d, dst_volt_pmic:0x%x, footprint:0x%x\n",
			eemsn_log->sn_log.sd[i].dst_volt_pmic,
			eemsn_log->sn_log.footprint[i]);
		seq_printf(m, "[%d]cur_volt:%d, new dst_volt_pmic:%d, cur temp:%d\n",
			eem_seq++, eemsn_log->sn_log.sd[i].cur_volt,
			eemsn_log->sn_log.sd[i].dst_volt_pmic * CPU_PMIC_STEP,
			eemsn_log->sn_log.sd[i].cur_temp);
	}
#endif
	seq_printf(m, "allfp:0x%x\n",
		eemsn_log->sn_log.allfp);
	dump_sndata_to_de(m);
	return 0;
}
static int eem_aging_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;
	unsigned char lock;
	enum sn_det_id i;
	unsigned int locklimit = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_AGING_DUMP_PROC_SHOW, &eem_data);
	seq_printf(m, "efuse_sv:0x%x\n", eemsn_log->efuse_sv);
#ifdef EEM_LOG_ENABLE
	seq_printf(m, "T_SVT_HV_LCPU:%d %d %d %d\n",
		eem_devinfo.T_SVT_HV_LCPU,
		eem_devinfo.T_SVT_LV_LCPU,
		eem_devinfo.T_SVT_HV_LCPU_RT,
		eem_devinfo.T_SVT_LV_LCPU_RT);
	seq_printf(m, "T_SVT_HV_BCPU:%d %d %d %d\n",
		eem_devinfo.T_SVT_HV_BCPU,
		eem_devinfo.T_SVT_LV_BCPU,
		eem_devinfo.T_SVT_HV_BCPU_RT,
		eem_devinfo.T_SVT_LV_BCPU_RT);
	seq_printf(m, "IN init_det, LCPU_A_T0_SVT:%d, LVT:%d, ",
		eem_devinfo.LCPU_A_T0_SVT,
		eem_devinfo.LCPU_A_T0_LVT);
	seq_printf(m, "ULVT:%d, DELTA_VC_LCPU:%d, ATE_TEMP:%d\n",
		eem_devinfo.LCPU_A_T0_ULVT,
		eem_devinfo.DELTA_VC_LCPU,
		eem_devinfo.ATE_TEMP);

	seq_printf(m, "IN init_det, BCPU_A_T0_SVT:%d, LVT:%d, ",
		eem_devinfo.BCPU_A_T0_SVT,
		eem_devinfo.BCPU_A_T0_LVT);
	seq_printf(m, "ULVT:%d, DELTA_VC_BCPU:%d\n",
		eem_devinfo.BCPU_A_T0_ULVT,
		eem_devinfo.DELTA_VC_BCPU);
#endif
	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}
	for (i = 0; i < NR_SN_DET; i++) {
		seq_printf(m, "id:%d\n", i);
		seq_printf(m, "[cal_sn_aging]Param_temp:%d, SVT:%d, LVT:%d, ULVT:%d\n",
			eemsn_log->sn_cpu_param[i].Param_temp,
			eemsn_log->sn_cpu_param[i].Param_A_Tused_SVT,
			eemsn_log->sn_cpu_param[i].Param_A_Tused_LVT,
			eemsn_log->sn_cpu_param[i].Param_A_Tused_ULVT);
		seq_printf(m, "[INIT]delta_vc:%d, CPE_GB:%d, MSSV_GB:%d\n",
			eemsn_log->sn_cal_data[i].delta_vc,
			eemsn_log->sn_cpu_param[i].CPE_GB,
			eemsn_log->sn_cpu_param[i].MSSV_GB);
		seq_printf(m, "cal_sn_aging, atvt A_Tused_SVT:%d, LVT:%d, ",
			eemsn_log->sn_cal_data[i].atvt.A_Tused_SVT,
			eemsn_log->sn_cal_data[i].atvt.A_Tused_LVT);
		seq_printf(m, "ULVT:%d, cur temp:%d\n",
			eemsn_log->sn_cal_data[i].atvt.A_Tused_ULVT,
			eemsn_log->sn_cal_data[i].TEMP_CAL);
		seq_printf(m, "[cal_sn_aging]id:%d, cpe_init_aging:%llu, ",
			i, eemsn_log->sn_cal_data[i].cpe_init_aging);
		seq_printf(m, "delta_vc:%d, CPE_Aging:%d, sn_anging:%d\n",
			eemsn_log->sn_cal_data[i].delta_vc,
			eemsn_log->sn_cal_data[i].CPE_Aging,
			eemsn_log->sn_cal_data[i].sn_aging);
		seq_printf(m, "volt_cross:%d\n",
			eemsn_log->sn_cal_data[i].volt_cross);
	}
	if (ipi_ret != 0)
		seq_printf(m, "ipi_ret:%d\n", ipi_ret);
	return 0;
}

static int eem_sn_sram_proc_show(struct seq_file *m, void *v)
{
	phys_addr_t sn_mem_base_phys;
	phys_addr_t sn_mem_size;
	phys_addr_t sn_mem_base_virt = 0;
	void __iomem *addr_ptr;
	int counter = 0;

	/* sn_mem_size = NR_FREQ * 2; */
	sn_mem_size = OFFS_SN_VOLT_E_4B - OFFS_SN_VOLT_S_4B;
	sn_mem_base_phys = OFFS_SN_VOLT_S_4B;
	if ((void __iomem *)sn_mem_base_phys != NULL)
		sn_mem_base_virt =
		(phys_addr_t)(uintptr_t)ioremap_wc(
		sn_mem_base_phys,
		sn_mem_size);
#ifdef EEM_LOG_ENABLE
	pr_info("phys:0x%llx, size:%d, virt:0x%llx\n",
		(unsigned long long)sn_mem_base_phys,
		(unsigned long long)sn_mem_size,
		(unsigned long long)sn_mem_base_virt);
	pr_info("read base_virt:0x%x\n",
		__raw_readl((void __iomem *)sn_mem_base_virt));
#endif
	if ((void __iomem *)(sn_mem_base_virt) != NULL) {
		for (addr_ptr = (void __iomem *)(sn_mem_base_virt)
			, counter = 0; counter <=
			OFFS_SN_VOLT_E_4B - OFFS_SN_VOLT_S_4B;
			(addr_ptr += 4), counter += 4)
			seq_printf(m, "0x%08X\n",
				(unsigned int)__raw_readl(addr_ptr));
	}
	return 0;
}
static int eem_hrid_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eem_efuse_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eem_freq_proc_show(struct seq_file *m, void *v)
{
#ifdef EEM_LOG_ENABLE
	struct eemsn_det *det;
	unsigned int i;
	enum mt_cpu_dvfs_id cpudvfsindex;

	for_each_det(det) {
		cpudvfsindex = detid_to_dvfsid(det);
		for (i = 0; i < MAX_NR_FREQ; i++) {
			if (det->det_id <= EEMSN_DET_CCI) {
				seq_printf(m,
					"%s[DVFS][CPU_%s][OPP%d] volt:%d, freq:%d\n",
					EEM_TAG, cpu_name[cpudvfsindex], i,
					det->ops->pmic_2_volt(det,
					det->volt_tbl_orig[i]) * 10, 0);
			}
		}
	}
#endif
	return 0;
}
static int eem_mar_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s[CPU_BIG][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF, 0,
			HIGH_TEMP_OFF, AGING_VAL_CPU_B);
	seq_printf(m, "%s[CPU_BIG][MID] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF, 0,
			HIGH_TEMP_OFF, AGING_VAL_CPU_B);
	seq_printf(m, "%s[CPU_L][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF, 0,
			HIGH_TEMP_OFF, AGING_VAL_CPU);
	seq_printf(m, "%s[CPU_CCI][HIGH] 1:%d, 2:%d, 3:%d, 5:%d\n",
			EEM_TAG, LOW_TEMP_OFF, 0,
			HIGH_TEMP_OFF, AGING_VAL_CPU);
	return 0;
}
/*
 * show current voltage
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;
	u32 rdata = 0, i;

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);
	if (det->features != 0) {
		for (i = 0; i < MAX_NR_FREQ; i++)
			seq_printf(m, "[%d],eem = [%x], pmic = [%x]\n",
			i,
			eemsn_log->det_vlog[det->det_id].volt_tbl_init2[i],
			eemsn_log->det_vlog[det->det_id].volt_tbl_pmic[i]);
	}
	return 0;
}
/*
 * show current EEM status
 */
static int eem_status_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct eemsn_det *det = (struct eemsn_det *)m->private;

#ifdef EEM_LOG_ENABLE
	for (i = 0; i < MAX_NR_FREQ - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt(det,
					det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (",
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
#endif
	for (i = 0; i < MAX_NR_FREQ - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);
	return 0;
}
static int eem_force_sensing_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_FORCE_SN_SENSING, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);
	return 0;
}
static int eem_pull_data_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;
#if ENABLE_COUNT_SNTEMP
	unsigned int i;
	unsigned char lock;
	unsigned int locklimit = 0;
#endif

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_cpueb(IPI_EEMSN_PULL_DATA, &eem_data);
	seq_printf(m, "ret:%d\n", ipi_ret);
#if ENABLE_COUNT_SNTEMP
	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		/* pr_info("1 lock=0x%X\n", lock); */
		lock = eemsn_log->lock;
		/* pr_info("2 lock=0x%X\n", lock); */
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}
	for (i = 0; i < NR_SN_DET; i++) {
		seq_printf(m,
		"id:%d, sn_temp_cnt -1:%d, -2:%d, -3:%d, -4:%d, -5:%d\n",
		i,
		eemsn_log->sn_temp_cnt[i][0],
		eemsn_log->sn_temp_cnt[i][1],
		eemsn_log->sn_temp_cnt[i][2],
		eemsn_log->sn_temp_cnt[i][3],
		eemsn_log->sn_temp_cnt[i][4]);
	}
#endif
	return 0;
}
/*
 * show EEM offset
 */
static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eemsn_det *det = (struct eemsn_det *)m->private;

	seq_printf(m, "%d\n", det->volt_offset);
	return 0;
}
/*
 * set EEM offset by procfs
 */
static ssize_t eem_offset_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eemsn_det *det = (struct eemsn_det *)PDE_DATA(file_inode(file));
	unsigned int ipi_ret = 0;
	struct eem_ipi_data eem_data;

	if (!buf)
		return -ENOMEM;
	ret = -EINVAL;
	if (count >= PAGE_SIZE)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(buf, buffer, count))
		goto out;
	buf[count] = '\0';
	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = offset;
		ipi_ret = eem_to_cpueb(IPI_EEMSN_OFFSET_PROC_WRITE, &eem_data);
		/* to show in eem_offset_proc_show */
		det->volt_offset = (signed char)offset;
		pr_debug("set volt_offset %d(%d)\n", offset, det->volt_offset);
	} else {
		ret = -EINVAL;
		pr_debug("bad argument_1!! argument should be \"0\"\n");
	}
out:
	free_page((unsigned long)buf);
	return (ret < 0) ? ret : count;
}

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_status);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RO(eem_aging_dump);
PROC_FOPS_RO(eem_sn_sram);
PROC_FOPS_RO(eem_hrid);
PROC_FOPS_RO(eem_efuse);
PROC_FOPS_RO(eem_freq);
PROC_FOPS_RO(eem_mar);
PROC_FOPS_RO(eem_force_sensing);
PROC_FOPS_RO(eem_pull_data);
PROC_FOPS_RW(eem_setmargin);

static int create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	struct eemsn_det *det;
	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_status),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
		PROC_ENTRY(eem_setmargin),
	};
	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_aging_dump),
		PROC_ENTRY(eem_sn_sram),
		PROC_ENTRY(eem_hrid),
		PROC_ENTRY(eem_efuse),
		PROC_ENTRY(eem_freq),
		PROC_ENTRY(eem_mar),
		PROC_ENTRY(eem_force_sensing),
		PROC_ENTRY(eem_pull_data),
	};

	eem_dir = proc_mkdir("eem", NULL);
	for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
		if (!proc_create(eem_entries[i].name, 0664,
					eem_dir, eem_entries[i].fops)) {
			pr_info("[%s]: create /proc/eem/%s failed\n",
					__func__,
					eem_entries[i].name);
		}
	}
	for_each_det(det) {
		if (det->features == 0)
			continue;
		det_dir = proc_mkdir(det->name, eem_dir);
		if (!det_dir) {
			pr_debug("[%s]: mkdir /proc/eem/%s failed\n"
					, __func__, det->name);
		}
		for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
			if (!proc_create_data(det_entries[i].name,
				0664,
				det_dir,
				det_entries[i].fops, det)) {
				pr_debug
		("[%s]: create /proc/eem/%s/%s failed\n", __func__,
		det->name, det_entries[i].name);
			}
		}
	}
	return 0;
}
static void init_mcl50_setting(void)
{
	struct eem_ipi_data eem_data;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 1;

	eem_to_cpueb(IPI_EEMSN_INIT, &eem_data);

}

int mtk_eem_init(void)
{
	struct eem_ipi_data eem_data;
	int err = 0;
	struct device_node *node = NULL;
	int enable;

	return -ENOENT; // have not implemented

	eem_base = ioremap(EEM_BASEADDR, EEM_BASESIZE);
	eem_csram_base = ioremap(EEMSN_CSRAM_BASE, EEMSN_CSRAM_SIZE);
	sn_base = ioremap(SN_BASEADDR, SN_BASESIZE);

	err = mtk_ipi_register(get_mcupm_ipidev(), CH_S_EEMSN, NULL, NULL,
		(void *)&ipi_ackdata);
	if (err != 0) {
		pr_info("%s error ret:%d\n", __func__, err);
		return 0;
	}

	eem_log_phy_addr =
		mcupm_reserve_mem_get_phys(MCUPM_EEMSN_MEM_ID);
	eem_log_virt_addr =
		mcupm_reserve_mem_get_virt(MCUPM_EEMSN_MEM_ID);
	eem_log_size = sizeof(struct eemsn_dbg_log);
	if (eem_log_virt_addr != 0) {
		eemsn_log =
			(struct eemsn_dbg_log *)eem_log_virt_addr;
	} else
		pr_info("mcupm_reserve_mem_get_virt fail\n");

	memset(eemsn_log, 0, sizeof(struct eemsn_dbg_log));
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = eem_log_phy_addr;
	eem_data.u.data.arg[1] = eem_log_size;
	eem_to_cpueb(IPI_EEMSN_SHARERAM_INIT, &eem_data);

	node = of_find_compatible_node(NULL, NULL, "mediatek,cpufreq-hw");
	if (node) {
		err = of_property_read_u32(node, "mcl50_load", &enable);
		if (!err && enable)
			init_mcl50_setting();
	}
	return create_debug_fs();
}

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver v0.1.1");
MODULE_AUTHOR("Chienwei Chang <chiewei.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

