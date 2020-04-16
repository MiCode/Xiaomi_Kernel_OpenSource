// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/sched/debug.h>
#include <linux/uaccess.h>

#include <mt-plat/aee.h>
#include <mt-plat/devapc_public.h>
#include <mt-plat/mtk_secure_api.h>
#include "devapc-mtk-multi-3.h"

static struct mtk_devapc_context {
	struct clk *devapc_infra_clk;
	uint32_t devapc_irq;

	/* HW reg mapped addr */
	void __iomem *devapc_pd_base[SLAVE_TYPE_NUM];
	void __iomem *devapc_infra_ao_base;
	void __iomem *sramrom_base;

	struct mtk_devapc_soc *soc;
	struct mutex viocb_list_lock;
} mtk_devapc_ctx[1];

static LIST_HEAD(viocb_list);
static DEFINE_SPINLOCK(devapc_lock);

static void devapc_test_cb(void)
{
	pr_info(PFX "%s success !\n", __func__);
}

static struct devapc_vio_callbacks devapc_test_handle = {
	.id = DEVAPC_SUBSYS_TEST,
	.debug_dump = devapc_test_cb,
};

/*
 * mtk_devapc_pd_get - get devapc pd_types of register address.
 *
 * Returns the value of reg addr
 */
static void __iomem *mtk_devapc_pd_get(enum DEVAPC_SLAVE_TYPE slave_type,
				       enum DEVAPC_PD_REG_TYPE pd_reg_type,
				       uint32_t index)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	const uint32_t *devapc_pds = mtk_devapc_ctx->soc->devapc_pds;
	void __iomem *reg;

	if (unlikely(devapc_pds == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return NULL;
	}

	if (((slave_type == SLAVE_TYPE_INFRA &&
			index < vio_info->vio_mask_sta_num_infra) ||
			(slave_type == SLAVE_TYPE_PERI &&
			 index < vio_info->vio_mask_sta_num_peri) ||
			(slave_type == SLAVE_TYPE_PERI2 &&
			 index < vio_info->vio_mask_sta_num_peri2)) &&
			(pd_reg_type < PD_REG_TYPE_NUM)) {

		reg = mtk_devapc_ctx->devapc_pd_base[slave_type] +
			devapc_pds[pd_reg_type];

		if (pd_reg_type == VIO_MASK || pd_reg_type == VIO_STA)
			reg += 0x4 * index;

	} else {
		pr_err(PFX "%s:0x%x or %s:0x%x or %s:0x%x is out of boundary\n",
				"slave_type", slave_type,
				"pd_reg_type", pd_reg_type,
				"index", index);
		return NULL;
	}

	return reg;
}

/*
 * sramrom_vio_handler - clean sramrom violation & print violation information
 *			for debugging.
 */
static void sramrom_vio_handler(void)
{
	const struct mtk_sramrom_sec_vio_desc *sramrom_vios;
	size_t sramrom_vio_sta, sramrom_vio_addr;
	uint32_t master_id, domain_id, rw;
	struct arm_smccc_res res;
	int sramrom_vio;

	sramrom_vios = mtk_devapc_ctx->soc->sramrom_sec_vios;

	arm_smccc_smc(MTK_SIP_KERNEL_CLR_SRAMROM_VIO,
			0, 0, 0, 0, 0, 0, 0, &res);

	sramrom_vio = res.a0;
	sramrom_vio_sta = res.a1;
	sramrom_vio_addr = res.a2;

	if (sramrom_vio == SRAM_VIOLATION)
		pr_info(PFX "%s, SRAM violation is triggered\n", __func__);
	else if (sramrom_vio == ROM_VIOLATION)
		pr_info(PFX "%s, ROM violation is triggered\n", __func__);
	else {
		pr_info(PFX "SRAMROM violation is not triggered\n");
		return;
	}

	master_id = (sramrom_vio_sta & sramrom_vios->vio_id_mask) >>
			sramrom_vios->vio_id_shift;
	domain_id = (sramrom_vio_sta & sramrom_vios->vio_domain_mask) >>
			sramrom_vios->vio_domain_shift;
	rw = (sramrom_vio_sta & sramrom_vios->vio_rw_mask) >>
			sramrom_vios->vio_rw_shift;

	pr_info(PFX "%s: %s:0x%x, %s:0x%x, %s:%s, %s:0x%x\n",
		__func__, "master_id", master_id,
		"domain_id", domain_id,
		"rw", rw ? "Write" : "Read",
		"vio_addr", sramrom_vio_addr);
}

static void mask_module_irq(enum DEVAPC_SLAVE_TYPE slave_type,
			    uint32_t module, bool mask)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if ((slave_type == SLAVE_TYPE_INFRA &&
		apc_register_index < vio_info->vio_mask_sta_num_infra) ||
		(slave_type == SLAVE_TYPE_PERI &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri) ||
		(slave_type == SLAVE_TYPE_PERI2 &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri2)) {

		reg = mtk_devapc_pd_get(slave_type, VIO_MASK,
				apc_register_index);

		if (mask)
			writel(readl(reg) | (1 << apc_set_index), reg);
		else
			writel(readl(reg) & (~(1 << apc_set_index)), reg);

	} else
		pr_err(PFX "%s: %s, %s:0x%x, %s:0x%x, %s:%s\n",
				__func__, "out of boundary",
				"slave_type", slave_type,
				"module_index", module,
				"mask", mask ? "true" : "false");
}

static int32_t check_vio_status(enum DEVAPC_SLAVE_TYPE slave_type,
				uint32_t module)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if ((slave_type == SLAVE_TYPE_INFRA &&
		apc_register_index < vio_info->vio_mask_sta_num_infra) ||
		(slave_type == SLAVE_TYPE_PERI &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri) ||
		(slave_type == SLAVE_TYPE_PERI2 &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri2)) {

		reg = mtk_devapc_pd_get(slave_type, VIO_STA,
				apc_register_index);

	} else {
		pr_err(PFX "%s: %s, %s:0x%x, %s:0x%x\n",
				__func__, "out of boundary",
				"slave_type", slave_type,
				"module_index", module);
		return -EOVERFLOW;
	}

	if (readl(reg) & (0x1 << apc_set_index))
		return VIOLATION_TRIGGERED;
	else
		return 0;
}

static int32_t clear_vio_status(enum DEVAPC_SLAVE_TYPE slave_type,
				uint32_t module)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	int sramrom_vio_idx;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	/* Clear SRAMROM violation first */
	sramrom_vio_idx = mtk_devapc_ctx->soc->vio_info->sramrom_vio_idx;
	if (slave_type == SLAVE_TYPE_INFRA && module == sramrom_vio_idx)
		sramrom_vio_handler();

	if ((slave_type == SLAVE_TYPE_INFRA &&
		apc_register_index < vio_info->vio_mask_sta_num_infra) ||
		(slave_type == SLAVE_TYPE_PERI &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri) ||
		(slave_type == SLAVE_TYPE_PERI2 &&
		 apc_register_index < vio_info->vio_mask_sta_num_peri2)) {

		reg = mtk_devapc_pd_get(slave_type, VIO_STA,
				apc_register_index);
		writel(0x1 << apc_set_index, reg);

	} else {
		pr_err(PFX "%s: %s, %s:0x%x, %s:0x%x\n",
				__func__, "out of boundary",
				"slave_type", slave_type,
				"module_index", module);
		return -EOVERFLOW;
	}

	if (check_vio_status(slave_type, module))
		return -EIO;

	return 0;
}

static void print_vio_mask_sta(void)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	int i;

	for (i = 0; i < vio_info->vio_mask_sta_num_infra; i++)
		pr_debug(PFX "%s_%d: 0x%x, %s_%d: 0x%x\n",
				"INFRA VIO_MASK", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_INFRA,
						VIO_MASK, i)),
				"INFRA VIO_STA", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_INFRA,
						VIO_STA, i))
			);

	for (i = 0; i < vio_info->vio_mask_sta_num_peri; i++)
		pr_debug(PFX "%s_%d: 0x%x, %s_%d: 0x%x\n",
				"PERI VIO_MASK", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_PERI,
						VIO_MASK, i)),
				"PERI VIO_STA", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_PERI,
						VIO_STA, i))
			);

	for (i = 0; i < vio_info->vio_mask_sta_num_peri2; i++)
		pr_debug(PFX "%s_%d: 0x%x, %s_%d: 0x%x\n",
				"PERI2 VIO_MASK", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_PERI2,
						VIO_MASK, i)),
				"PERI2 VIO_STA", i,
				readl(mtk_devapc_pd_get(SLAVE_TYPE_PERI2,
						VIO_STA, i))
			);
}

/*
 * start_devapc - initialize devapc status and start receiving interrupt
 *		  while devapc violation is triggered.
 */
static void start_devapc(void)
{
	const struct mtk_device_info *device_info[SLAVE_TYPE_NUM];
	const struct mtk_device_num *ndevices;
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_apc_con_reg;
	uint32_t vio_shift_sta;
	int slave_type, i;

	print_vio_mask_sta();
	ndevices = mtk_devapc_ctx->soc->ndevices;

	device_info[SLAVE_TYPE_INFRA] = mtk_devapc_ctx->soc->device_info_infra;
	device_info[SLAVE_TYPE_PERI] = mtk_devapc_ctx->soc->device_info_peri;
	device_info[SLAVE_TYPE_PERI2] = mtk_devapc_ctx->soc->device_info_peri2;

	for (slave_type = 0; slave_type < SLAVE_TYPE_NUM; slave_type++) {

		pd_apc_con_reg = mtk_devapc_pd_get(slave_type, APC_CON, 0);
		pd_vio_shift_sta_reg = mtk_devapc_pd_get(
				slave_type, VIO_SHIFT_STA, 0);

		if (unlikely(pd_apc_con_reg == NULL ||
			     pd_vio_shift_sta_reg == NULL ||
			     device_info == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		/* Clear DEVAPC violation status */
		writel(BIT(31), pd_apc_con_reg);

		/* Clear violation shift status */
		vio_shift_sta = readl(pd_vio_shift_sta_reg);
		if (vio_shift_sta) {
			writel(vio_shift_sta, pd_vio_shift_sta_reg);
			pr_info(PFX "clear %s:0x%x %s:0x%x to 0x%x\n",
					"slave_type", slave_type,
					"VIO_SHIFT_STA", vio_shift_sta,
					readl(pd_vio_shift_sta_reg));

		}

		/* Clear violation status */
		for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {
			if (!device_info[slave_type][i].enable_vio_irq)
				continue;

			if ((check_vio_status(slave_type, i) ==
					VIOLATION_TRIGGERED) &&
					clear_vio_status(slave_type, i))
				pr_warn(PFX "clear vio status failed\n");

			mask_module_irq(slave_type, i, false);
		}
	}

	print_vio_mask_sta();

	/* register subsys test cb */
	register_devapc_vio_callback(&devapc_test_handle);
}

/*
 * sync_vio_dbg - start to get violation information by selecting violation
 *		  group and enable violation shift.
 *
 * Returns sync done or not
 */
static uint32_t sync_vio_dbg(enum DEVAPC_SLAVE_TYPE slave_type,
			     uint32_t shift_bit)
{
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_vio_shift_sel_reg;
	void __iomem *pd_vio_shift_con_reg;
	uint32_t shift_count;
	uint32_t sync_done;

	if (slave_type >= SLAVE_TYPE_NUM ||
			shift_bit >= (MOD_NO_IN_1_DEVAPC * 2)) {
		pr_err(PFX "%s: param check failed, %s:0x%x, %s:0x%x\n",
				"slave_type", slave_type,
				"shift_bit", shift_bit);
		return 0;
	}

	pd_vio_shift_sta_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_STA, 0);
	pd_vio_shift_sel_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_SEL, 0);
	pd_vio_shift_con_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_CON, 0);

	pr_info(PFX "%s:0x%x %s:0x%x\n",
			"slave_type", slave_type,
			"VIO_SHIFT_STA", readl(pd_vio_shift_sta_reg));

	writel(0x1 << shift_bit, pd_vio_shift_sel_reg);
	writel(0x1, pd_vio_shift_con_reg);

	for (shift_count = 0; (shift_count < 100) &&
			((readl(pd_vio_shift_con_reg) & 0x3) != 0x3);
			++shift_count)
		NULL;

	if ((readl(pd_vio_shift_con_reg) & 0x3) == 0x3)
		sync_done = 1;
	else {
		sync_done = 0;
		pr_info(PFX "sync failed, shift_bit:0x%x\n", shift_bit);
	}

	/* Disable shift mechanism */
	writel(0x0, pd_vio_shift_con_reg);
	writel(0x0, pd_vio_shift_sel_reg);
	writel(0x1 << shift_bit, pd_vio_shift_sta_reg);

	pr_debug(PFX "(Post) %s:0x%x, %s:0x%x, %s:0x%x\n",
			"VIO_SHIFT_STA",
			readl(pd_vio_shift_sta_reg),
			"VIO_SHIFT_SEL",
			readl(pd_vio_shift_sel_reg),
			"VIO_SHIFT_CON",
			readl(pd_vio_shift_con_reg));

	return sync_done;
}

static const char * const perm_to_str[] = {
	"NO_PROTECTION",
	"SECURE_RW_ONLY",
	"SECURE_RW_NS_R_ONLY",
	"FORBIDDEN",
	"NO_PERM_CTRL"
};

static const char *perm_to_string(uint8_t perm)
{
	if (perm < 4)
		return perm_to_str[perm];
	else
		return perm_to_str[4];
}

/*
 * get_permission - get slave's access permission of domain id.
 *
 * Returns the value of access permission
 */
static uint8_t get_permission(enum DEVAPC_SLAVE_TYPE slave_type,
			      int vio_index, int domain)
{
	const struct mtk_device_info *device_info[SLAVE_TYPE_NUM];
	uint32_t sys_index, ctrl_index, apc_set_index;
	const struct mtk_device_num *ndevices;
	struct arm_smccc_res res;
	uint32_t ret;
	int i;

	if (slave_type >= SLAVE_TYPE_NUM ||
			vio_index >= ndevices[slave_type].vio_slave_num) {
		pr_err(PFX "%s: param check failed, %s:0x%x, %s:0x%xn",
				"slave_type", slave_type,
				"vio_index", vio_index);
		return 0xFF;
	}

	device_info[SLAVE_TYPE_INFRA] = mtk_devapc_ctx->soc->device_info_infra;
	device_info[SLAVE_TYPE_PERI] = mtk_devapc_ctx->soc->device_info_peri;
	device_info[SLAVE_TYPE_PERI2] = mtk_devapc_ctx->soc->device_info_peri2;
	sys_index = -1;
	ctrl_index = -1;

	for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {

		if (device_info[slave_type][i].vio_index == vio_index) {
			sys_index =
				device_info[slave_type][vio_index].sys_index;
			ctrl_index =
				device_info[slave_type][vio_index].ctrl_index;
			break;
		}
	}

	if (sys_index == -1 || ctrl_index == -1) {
		pr_err(PFX "%s: cannot get sys_index & ctrl_index\n",
				__func__);
		return 0xFF;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_DUMP, slave_type, sys_index, domain,
			ctrl_index, 0, 0, 0, &res);
	ret = res.a0;

	if (ret == DEAD || ret == SIP_SVC_E_NOT_SUPPORTED) {
		pr_err(PFX "%s: SMC call failed, ret:0x%x\n",
				__func__, ret);
		return 0xFF;
	}

	apc_set_index = ctrl_index % MOD_NO_IN_1_DEVAPC;
	ret = (ret & (0x3 << (apc_set_index * 2))) >> (apc_set_index * 2);

	return (ret & 0x3);
}

/*
 * mtk_devapc_vio_check - check violation shift status is raised or not.
 *
 * Returns the value of violation shift status reg
 */
static uint32_t mtk_devapc_vio_check(enum DEVAPC_SLAVE_TYPE slave_type)
{
	if (slave_type < SLAVE_TYPE_NUM)
		return readl(mtk_devapc_pd_get(slave_type, VIO_SHIFT_STA, 0));

	pr_err(PFX "%s: param check failed, %s:0x%x\n",
			__func__, "slave_type", slave_type);

	return 0;
}

/*
 * mtk_devapc_dump_vio_dbg - shift & dump the violation debug information.
 */
static void mtk_devapc_dump_vio_dbg(enum DEVAPC_SLAVE_TYPE slave_type)
{
	void __iomem *vio_dbg0_reg, *vio_dbg1_reg, *vio_dbg2_reg;
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	struct mtk_devapc_vio_info *vio_info;
	uint32_t dbg0;
	int i;

	if (slave_type >= SLAVE_TYPE_NUM) {
		pr_err(PFX "%s: param check failed, %s:0x%x\n",
				__func__, "slave_type", slave_type);
		return;
	}

	vio_dbg0_reg = mtk_devapc_pd_get(slave_type, VIO_DBG0, 0);
	vio_dbg1_reg = mtk_devapc_pd_get(slave_type, VIO_DBG1, 0);
	vio_dbg2_reg = mtk_devapc_pd_get(slave_type, VIO_DBG2, 0);

	vio_dbgs = mtk_devapc_ctx->soc->vio_dbgs;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	for (i = 0; i < MOD_NO_IN_1_DEVAPC * 2; ++i) {
		if (mtk_devapc_vio_check(slave_type) & (0x1 << i)) {

			if (!sync_vio_dbg(slave_type, i))
				continue;

			/* Extract violation information */
			dbg0 = readl(vio_dbg0_reg);
			vio_info->master_id = readl(vio_dbg1_reg);
			vio_info->vio_addr = readl(vio_dbg2_reg);

			vio_info->domain_id = (dbg0 & vio_dbgs->vio_dbg_dmnid)
				>> vio_dbgs->vio_dbg_dmnid_start_bit;
			vio_info->write = (bool)(dbg0 & vio_dbgs->vio_dbg_w_vio)
				>> vio_dbgs->vio_dbg_w_vio_start_bit;
			vio_info->read = (bool)(dbg0 & vio_dbgs->vio_dbg_r_vio)
				>> vio_dbgs->vio_dbg_r_vio_start_bit;
			vio_info->vio_addr_high =
				(dbg0 & vio_dbgs->vio_addr_high)
				>> vio_dbgs->vio_addr_high_start_bit;

			/* Print violation information */
			pr_info(PFX "%s(%s) %s:%x %s:%x, %s:%x, %:%x\n",
					"Violation",
					vio_info->read ? "R" : "W",
					"Vio Addr:", vio_info->vio_addr,
					"High:", vio_info->vio_addr_high,
					"Bus ID:0x", vio_info->master_id,
					"Dom ID:0x", vio_info->domain_id);

			pr_info(PFX "%s - %s%s, %s%i\n",
					"Violation",
					"Current Process:", current->comm,
					"PID:", current->pid);
		}
	}
}

/*
 * devapc_extra_handler -
 * 1. trigger kernel exception/aee exception/kernel warning to increase devapc
 * violation severity level
 * 2. call subsys handler to get more debug information
 */
static void devapc_extra_handler(enum DEVAPC_SLAVE_TYPE slave_type,
		const char *vio_master, uint32_t vio_index)
{
	const struct mtk_device_info *device_info[SLAVE_TYPE_NUM];
	struct mtk_devapc_dbg_status *dbg_stat;
	struct devapc_vio_callbacks *viocb;
	char dispatch_key[48] = {0};
	enum infra_subsys_id id;

	device_info[SLAVE_TYPE_INFRA] = mtk_devapc_ctx->soc->device_info_infra;
	device_info[SLAVE_TYPE_PERI] = mtk_devapc_ctx->soc->device_info_peri;
	device_info[SLAVE_TYPE_PERI2] = mtk_devapc_ctx->soc->device_info_peri2;

	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;

	pr_info(PFX "%s:%d\n", "vio_trigger_times",
			mtk_devapc_ctx->soc->vio_info->vio_trigger_times++);

	/* Dispatch slave owner if APMCU access. Others, dispatch master */
	if (!strncmp(vio_master, "APMCU", 5))
		strncpy(dispatch_key, mtk_devapc_ctx->soc->subsys_get(
				slave_type, vio_index),
				sizeof(dispatch_key));
	else
		strncpy(dispatch_key, vio_master, sizeof(dispatch_key));

	dispatch_key[sizeof(dispatch_key) - 1] = '\0';

	/* Callback func for vio master */
	if (!strncasecmp(vio_master, "MD", 2))
		id = INFRA_SUBSYS_MD;
	else if (!strncasecmp(vio_master, "CONN", 4) ||
			!strncasecmp(dispatch_key, "CONN", 4))
		id = INFRA_SUBSYS_CONN;
	else if (!strncasecmp(vio_master, "TINYSYS", 7))
		id = INFRA_SUBSYS_ADSP;
	else if (!strncasecmp(vio_master, "GCE", 3))
		id = INFRA_SUBSYS_GCE;
	else if (!strncasecmp(vio_master, "APMCU", 5))
		id = INFRA_SUBSYS_APMCU;
	else
		id = DEVAPC_SUBSYS_RESERVED;

	/* enable_ut to test callback */
	if (dbg_stat->enable_ut)
		id = DEVAPC_SUBSYS_TEST;

	if (id != DEVAPC_SUBSYS_RESERVED) {
		list_for_each_entry(viocb, &viocb_list, list) {
			if (viocb->id == id && viocb->debug_dump)
				viocb->debug_dump();

			/* always call clkmgr cb if it's registered */
			if (viocb->id == DEVAPC_SUBSYS_CLKMGR &&
					viocb->debug_dump)
				viocb->debug_dump();
		}
	}

	/* Severity level */
	if (dbg_stat->enable_KE) {
		pr_info(PFX "Device APC Violation Issue/%s", dispatch_key);
		BUG_ON(id != INFRA_SUBSYS_CONN && id != INFRA_SUBSYS_MD);

	} else if (dbg_stat->enable_AEE) {

		/* call mtk aee_kernel_exception */
		aee_kernel_exception("[DEVAPC]",
				"%s %s %s %s\n%s%s\n",
				"Violation master:", vio_master,
				"access slave",
				device_info[slave_type][vio_index].device,
				"CRDISPATCH_KEY:Device APC Violation Issue/",
				dispatch_key
				);

	} else if (dbg_stat->enable_WARN) {
		WARN(1, "Violation master: %s access %s\n",
				vio_master,
				device_info[slave_type][vio_index].device);
	}
}

/*
 * devapc_violation_irq - the devapc Interrupt Service Routine (ISR) will dump
 *			  violation information including which master violates
 *			  access slave.
 */
static irqreturn_t devapc_violation_irq(int irq_number, void *dev_id)
{
	const struct mtk_device_info *device_info[SLAVE_TYPE_NUM];
	const struct mtk_device_num *ndevices;
	struct mtk_devapc_vio_info *vio_info;
	const char *vio_master;
	unsigned long flags;
	int slave_type, i;
	uint8_t perm;

	spin_lock_irqsave(&devapc_lock, flags);

	print_vio_mask_sta();

	device_info[SLAVE_TYPE_INFRA] = mtk_devapc_ctx->soc->device_info_infra;
	device_info[SLAVE_TYPE_PERI] = mtk_devapc_ctx->soc->device_info_peri;
	device_info[SLAVE_TYPE_PERI2] = mtk_devapc_ctx->soc->device_info_peri2;

	vio_info = mtk_devapc_ctx->soc->vio_info;
	ndevices = mtk_devapc_ctx->soc->ndevices;

	/* There are multiple DEVAPC_PD */
	for (slave_type = 0; slave_type < SLAVE_TYPE_NUM; slave_type++) {

		mtk_devapc_dump_vio_dbg(slave_type);

		for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {
			if (!device_info[slave_type][i].enable_vio_irq)
				continue;

			if (!check_vio_status(slave_type, i))
				continue;

			mask_module_irq(slave_type, i, true);

			if (clear_vio_status(slave_type, i))
				pr_warn(PFX "clear vio status failed\n");

			perm = get_permission(slave_type, i,
					vio_info->domain_id);

			vio_master = mtk_devapc_ctx->soc->master_get(
					vio_info->master_id,
					vio_info->vio_addr);

			if (!vio_master) {
				pr_warn(PFX "master_get failed\n");
				vio_master = "UNKNOWN_MASTER";
			}

			pr_info(PFX "%s %s %s %s (%s:%d, %s:%d)\n",
				"Violation master:", vio_master,
				"access violation slave:",
				device_info[slave_type][i].device,
				"slave_type", slave_type,
				"vio_idx", i);

			if (vio_info->read ^ vio_info->write)
				pr_info(PFX, "%s violation\n", vio_info->read ?
						"Read" : "Write");

			pr_info(PFX "Permission setting: %s\n",
				perm_to_string(perm));

			devapc_extra_handler(slave_type, vio_master, i);

			mask_module_irq(slave_type, i, false);

			break;
		}
	}

	spin_unlock_irqrestore(&devapc_lock, flags);
	return IRQ_HANDLED;
}

void register_devapc_vio_callback(struct devapc_vio_callbacks *viocb)
{
	INIT_LIST_HEAD(&viocb->list);
	list_add_tail(&viocb->list, &viocb_list);
}
EXPORT_SYMBOL(register_devapc_vio_callback);

/*
 * devapc_ut - There are two UT commands to support
 * 1. test permission denied violation
 * 2. test sramrom decode error violation
 */
static void devapc_ut(uint32_t cmd)
{
	void __iomem *devapc_ao_base;
	void __iomem *sramrom_base = mtk_devapc_ctx->sramrom_base;

	pr_info(PFX "%s, cmd:0x%x\n", __func__, cmd);

	devapc_ao_base = mtk_devapc_ctx->devapc_infra_ao_base;

	if (cmd == DEVAPC_UT_DAPC_VIO) {
		if (unlikely(devapc_ao_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		pr_info(PFX "%s, devapc_ao_infra_base:0x%x\n", __func__,
				readl(devapc_ao_base));

		pr_info(PFX "test done, it should generate violation!\n");

	} else if (cmd == DEVAPC_UT_SRAM_VIO) {
		if (unlikely(sramrom_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		pr_info(PFX "%s, sramrom_base:0x%x\n", __func__,
				readl(sramrom_base + RANDOM_OFFSET));

		pr_info(PFX "test done, it should generate violation!\n");

	} else {
		pr_info(PFX "%s, cmd(0x%x) not supported\n", __func__, cmd);
	}
}

/*
 * mtk_devapc_dbg_read - dump status of struct mtk_devapc_dbg_status.
 * Currently, we have 5 debug status:
 * 1. enable_ut: enable/disable devapc ut commands
 * 2~4. enable_KE/enable_AEE/enable_WARN
 * 5. enable_dapc: enable/disable dump access permission control
 *
 */
ssize_t mtk_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	struct mtk_devapc_dbg_status *dbg_stat = mtk_devapc_ctx->soc->dbg_stat;
	char msg_buf[1024] = {0};
	char *p = msg_buf;
	int len;

	devapc_log(p, msg_buf, "DEVAPC debug status:\n");
	devapc_log(p, msg_buf, "\tenable_ut = %d\n", dbg_stat->enable_ut);
	devapc_log(p, msg_buf, "\tenable_KE = %d\n", dbg_stat->enable_KE);
	devapc_log(p, msg_buf, "\tenable_AEE = %d\n", dbg_stat->enable_AEE);
	devapc_log(p, msg_buf, "\tenable_WARN = %d\n", dbg_stat->enable_WARN);
	devapc_log(p, msg_buf, "\tenable_dapc = %d\n", dbg_stat->enable_dapc);
	devapc_log(p, msg_buf, "\n");

	len = p - msg_buf;

	return simple_read_from_buffer(buffer, count, ppos, msg_buf, len);
}

/*
 * mtk_devapc_dbg_write - control status of struct mtk_devapc_dbg_status.
 * There are 7 nodes we can control:
 * 1. enable_ut
 * 2~4. enable_KE/enable_AEE/enable_WARN
 * 5. enable_dapc
 * 6. devapc_ut
 * 7. dump_apc
 */
ssize_t mtk_devapc_dbg_write(struct file *file, const char __user *buffer,
			     size_t count, loff_t *data)
{
	uint32_t slave_type, sys_index, domain, ctrl_index, apc_set_idx;
	struct mtk_devapc_dbg_status *dbg_stat;
	char *parm_str, *cmd_str, *pinput;
	struct arm_smccc_res res;
	char input[32] = {0};
	uint32_t param, ret;
	int err, len;

	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;
	if (unlikely(dbg_stat == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_err(PFX "copy from user failed!\n");
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");

	if (!cmd_str)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");

	if (!parm_str)
		return -EINVAL;

	err = kstrtol(parm_str, 10, (long *)&param);

	if (err)
		return err;

	if (!strncmp(cmd_str, "enable_ut", sizeof("enable_ut"))) {
		dbg_stat->enable_ut = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_ut = %s\n",
			dbg_stat->enable_ut ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_KE", sizeof("enable_KE"))) {
		dbg_stat->enable_KE = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_KE = %s\n",
			dbg_stat->enable_KE ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_AEE", sizeof("enable_AEE"))) {
		dbg_stat->enable_AEE = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_AEE = %s\n",
			dbg_stat->enable_AEE ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_WARN", sizeof("enable_WARN"))) {
		dbg_stat->enable_WARN = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_WARN = %s\n",
			dbg_stat->enable_WARN ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_dapc", sizeof("enable_dapc"))) {
		dbg_stat->enable_dapc = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_dapc = %s\n",
			dbg_stat->enable_dapc ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "devapc_ut", sizeof("devapc_ut"))) {
		if (dbg_stat->enable_ut)
			devapc_ut(param);
		else
			pr_info(PFX "devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "dump_apc", sizeof("dump_apc"))) {
		if (!dbg_stat->enable_dapc) {
			pr_info(PFX "dump_apc is not enabled\n");
			return -EINVAL;
		}

		/* slave_type is already parse before */
		slave_type = param;

		if (slave_type >= SLAVE_TYPE_NUM) {
			pr_err(PFX "Wrong slave type:0x%x\n", slave_type);
			return -EFAULT;
		}

		/* Parse sys_index */
		parm_str = strsep(&pinput, " ");
		if (parm_str)
			err = kstrtol(parm_str, 10, (long *)&sys_index);
		else
			sys_index = 0xFFFFFFFF;

		/* Parse domain id */
		parm_str = strsep(&pinput, " ");
		if (parm_str)
			err = kstrtol(parm_str, 10, (long *)&domain);
		else
			domain = DOMAIN_OTHERS;

		/* Parse ctrl_index */
		parm_str = strsep(&pinput, " ");
		if (parm_str != NULL)
			err = kstrtol(parm_str, 10, (long *)&ctrl_index);
		else
			ctrl_index = 0xFFFFFFFF;

		pr_info(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:0x%x\n",
				"slave_type", slave_type,
				"sys_index", sys_index,
				"domain_id", domain,
				"ctrl_index", ctrl_index);

		arm_smccc_smc(MTK_SIP_KERNEL_DAPC_DUMP, slave_type, sys_index,
				domain, ctrl_index, 0, 0, 0, &res);
		ret = res.a0;

		if (ret == DEAD || ret == SIP_SVC_E_NOT_SUPPORTED) {
			pr_err(PFX "%s, SMC call failed, ret: 0x%x\n",
					__func__, ret);
			return -EINVAL;
		}

		apc_set_idx = ctrl_index % MOD_NO_IN_1_DEVAPC;
		ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

		pr_info(PFX "Permission is %s\n",
			perm_to_string((ret & 0x3)));
		return count;
	} else
		return -EINVAL;

	return count;
}

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.write = mtk_devapc_dbg_write,
	.read = mtk_devapc_dbg_read,
};

int mtk_devapc_probe(struct platform_device *pdev,
		struct mtk_devapc_soc *soc)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;

	pr_info(PFX "driver registered\n");

	if (IS_ERR(node)) {
		pr_err(PFX "cannot find device node\n");
		return -ENODEV;
	}

	mtk_devapc_ctx->soc = soc;

	mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_INFRA] = of_iomap(node,
			DT_DEVAPC_INFRA_PD_IDX);
	if (unlikely(mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_INFRA]
				== NULL)) {
		pr_err(PFX "parse devapc_infra_pd_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI] = of_iomap(node,
			DT_DEVAPC_PERI_PD_IDX);
	if (unlikely(mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI]
				== NULL)) {
		pr_err(PFX "parse devapc_peri_pd_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI2] = of_iomap(node,
			DT_DEVAPC_PERI_PD2_IDX);
	if (unlikely(mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI2]
				== NULL)) {
		pr_err(PFX "parse devapc_peri_pd2_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_infra_ao_base = of_iomap(node,
			DT_DEVAPC_INFRA_AO_IDX);
	if (unlikely(mtk_devapc_ctx->devapc_infra_ao_base == NULL)) {
		pr_err(PFX "parse devapc_infra_ao_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_irq = irq_of_parse_and_map(node, 0);
	if (!mtk_devapc_ctx->devapc_irq) {
		pr_err(PFX "parse and map the interrupt failed\n");
		return -EINVAL;
	}

	pr_debug(PFX "%s:%p, %s:%p, %s:%p, IRQ:%d\n",
			"devapc_infra_pd_base",
			mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_INFRA],
			"devapc_peri_pd_base",
			mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI],
			"devapc_peri_pd2_base",
			mtk_devapc_ctx->devapc_pd_base[SLAVE_TYPE_PERI2],
			mtk_devapc_ctx->devapc_irq);

	ret = devm_request_irq(&pdev->dev, mtk_devapc_ctx->devapc_irq,
			(irq_handler_t)devapc_violation_irq,
			IRQF_TRIGGER_LOW, "devapc", NULL);
	if (ret) {
		pr_err(PFX "request devapc irq failed, ret:%d\n", ret);
		return ret;
	}

	/* CCF (Common Clock Framework) */
	mtk_devapc_ctx->devapc_infra_clk = devm_clk_get(&pdev->dev,
			"devapc-infra-clock");

	if (IS_ERR(mtk_devapc_ctx->devapc_infra_clk)) {
		pr_err(PFX "(Infra) Cannot get devapc clock from CCF\n");
		return -EINVAL;
	}

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);

	clk_prepare_enable(mtk_devapc_ctx->devapc_infra_clk);
	start_devapc();

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_devapc_probe);

int mtk_devapc_remove(struct platform_device *dev)
{
	clk_disable_unprepare(mtk_devapc_ctx->devapc_infra_clk);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_devapc_remove);

MODULE_DESCRIPTION("Mediatek Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
