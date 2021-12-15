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
#include "devapc-mtk-multi-ao.h"

static struct mtk_devapc_context {
	struct clk *devapc_infra_clk;
	uint32_t devapc_irq;

	/* HW reg mapped addr */
	void __iomem *devapc_pd_base[4];
	void __iomem *devapc_infra_ao_base;
	void __iomem *infracfg_base;
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

static enum devapc_cb_status devapc_test_adv_cb(uint32_t vio_addr)
{
	pr_info(PFX "%s success !\n", __func__);
	pr_info(PFX "vio_addr: 0x%x\n", vio_addr);

	return DEVAPC_NOT_KE;
}

static struct devapc_vio_callbacks devapc_test_handle = {
	.id = DEVAPC_SUBSYS_TEST,
	.debug_dump = devapc_test_cb,
	.debug_dump_adv = devapc_test_adv_cb,
};

/*
 * mtk_devapc_pd_get - get devapc pd_types of register address.
 *
 * Returns the value of reg addr
 */
static void __iomem *mtk_devapc_pd_get(int slave_type,
				       enum DEVAPC_PD_REG_TYPE pd_reg_type,
				       uint32_t index)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	const uint32_t *devapc_pds = mtk_devapc_ctx->soc->devapc_pds;
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	void __iomem *reg;

	if (unlikely(devapc_pds == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return NULL;
	}

	if ((slave_type < slave_type_num &&
			index < vio_info->vio_mask_sta_num[slave_type]) &&
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
	struct mtk_devapc_vio_info *vio_info;
	struct arm_smccc_res res;
	size_t sramrom_vio_sta;
	int sramrom_vio;
	uint32_t rw;

	sramrom_vios = mtk_devapc_ctx->soc->sramrom_sec_vios;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	arm_smccc_smc(MTK_SIP_KERNEL_CLR_SRAMROM_VIO,
			0, 0, 0, 0, 0, 0, 0, &res);

	sramrom_vio = res.a0;
	sramrom_vio_sta = res.a1;
	vio_info->vio_addr = res.a2;

	if (sramrom_vio == SRAM_VIOLATION)
		pr_info(PFX "%s, SRAM violation is triggered\n", __func__);
	else if (sramrom_vio == ROM_VIOLATION)
		pr_info(PFX "%s, ROM violation is triggered\n", __func__);
	else {
		pr_info(PFX "sramrom_vio:0x%x, sramrom_vio_sta:0x%zx, vio_addr:0x%x\n",
				sramrom_vio,
				sramrom_vio_sta,
				vio_info->vio_addr);
		pr_info(PFX "SRAMROM violation is not triggered\n");
		return;
	}

	vio_info->master_id = (sramrom_vio_sta & sramrom_vios->vio_id_mask)
			>> sramrom_vios->vio_id_shift;
	vio_info->domain_id = (sramrom_vio_sta & sramrom_vios->vio_domain_mask)
			>> sramrom_vios->vio_domain_shift;
	rw = (sramrom_vio_sta & sramrom_vios->vio_rw_mask) >>
			sramrom_vios->vio_rw_shift;

	if (rw)
		vio_info->write = 1;
	else
		vio_info->read = 1;

	pr_info(PFX "%s: %s:0x%x, %s:0x%x, %s:%s, %s:0x%x\n",
		__func__, "master_id", vio_info->master_id,
		"domain_id", vio_info->domain_id,
		"rw", rw ? "Write" : "Read",
		"vio_addr", vio_info->vio_addr);
}

static void mask_module_irq(int slave_type, uint32_t module, bool mask)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if ((slave_type < slave_type_num) &&
		(apc_register_index < vio_info->vio_mask_sta_num[slave_type])) {

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

static int32_t check_vio_status(int slave_type, uint32_t module)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if ((slave_type < slave_type_num) &&
		(apc_register_index < vio_info->vio_mask_sta_num[slave_type])) {

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

static int32_t clear_vio_status(int slave_type, uint32_t module)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	uint32_t apc_register_index;
	uint32_t apc_set_index;
	void __iomem *reg;

	apc_register_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_set_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	if ((slave_type < slave_type_num) &&
		(apc_register_index < vio_info->vio_mask_sta_num[slave_type])) {

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

static const char *slave_type_to_string(uint32_t slave_type)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	const char * const *slave_type_arr;

	slave_type_arr = mtk_devapc_ctx->soc->slave_type_arr;

	if (slave_type < slave_type_num)
		return slave_type_arr[slave_type];
	else
		return slave_type_arr[slave_type_num];
}

static void print_vio_mask_sta(bool debug)
{
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	void __iomem *pd_vio_shift_sta_reg;
	int slave_type, i;

	for (slave_type = 0; slave_type < slave_type_num; slave_type++) {

		pd_vio_shift_sta_reg = mtk_devapc_pd_get(slave_type,
				VIO_SHIFT_STA, 0);

		pr_info(PFX "[%s] %s: 0x%x\n",
				slave_type_to_string(slave_type),
				"VIO_SHIFT_STA",
				readl(pd_vio_shift_sta_reg)
		       );

		for (i = 0; i < vio_info->vio_mask_sta_num[slave_type]; i++) {
			if (debug)
				pr_info(PFX "%s: %s_%d: 0x%x, %s_%d: 0x%x\n",
					slave_type_to_string(slave_type),
					"VIO_MASK", i,
					readl(mtk_devapc_pd_get(slave_type,
							VIO_MASK, i)),
					"VIO_STA", i,
					readl(mtk_devapc_pd_get(slave_type,
							VIO_STA, i))
					);
			else
				pr_debug(PFX "%s: %s_%d: 0x%x, %s_%d: 0x%x\n",
					slave_type_to_string(slave_type),
					"VIO_MASK", i,
					readl(mtk_devapc_pd_get(slave_type,
							VIO_MASK, i)),
					"VIO_STA", i,
					readl(mtk_devapc_pd_get(slave_type,
							VIO_STA, i))
					);
		}
	}
}

static void devapc_vio_info_print(void)
{
	struct mtk_devapc_vio_info *vio_info;

	vio_info = mtk_devapc_ctx->soc->vio_info;

	/* Print violation information */
	if (vio_info->write)
		pr_info(PFX "Write Violation\n");
	else if (vio_info->read)
		pr_info(PFX "Read Violation\n");
	else
		pr_err(PFX "R/W Violation are not raised\n");

	pr_info(PFX "%s%x, %s%x, %s%x, %s%x\n",
			"Vio Addr:0x", vio_info->vio_addr,
			"High:0x", vio_info->vio_addr_high,
			"Bus ID:0x", vio_info->master_id,
			"Dom ID:0x", vio_info->domain_id);

	pr_info(PFX "%s - %s%s, %s%x\n",
			"Violation",
			"Current Process:", current->comm,
			"PID:", current->pid);
}

static bool check_type2_vio_status(int slave_type, int *vio_idx, int *index)
{
	uint32_t sramrom_vio_idx, mdp_vio_idx, disp2_vio_idx, mmsys_vio_idx;
	const struct mtk_device_info **device_info;
	const struct mtk_device_num *ndevices;
	int sramrom_slv_type, mm2nd_slv_type;
	bool mdp_vio, disp2_vio, mmsys_vio;
	int i;

	sramrom_slv_type = mtk_devapc_ctx->soc->vio_info->sramrom_slv_type;
	sramrom_vio_idx = mtk_devapc_ctx->soc->vio_info->sramrom_vio_idx;

	mm2nd_slv_type = mtk_devapc_ctx->soc->vio_info->mm2nd_slv_type;
	mdp_vio_idx = mtk_devapc_ctx->soc->vio_info->mdp_vio_idx;
	disp2_vio_idx = mtk_devapc_ctx->soc->vio_info->disp2_vio_idx;
	mmsys_vio_idx = mtk_devapc_ctx->soc->vio_info->mmsys_vio_idx;

	device_info = mtk_devapc_ctx->soc->device_info;
	ndevices = mtk_devapc_ctx->soc->ndevices;

	/* check SRAMROM */
	if (slave_type == sramrom_slv_type &&
			check_vio_status(slave_type, sramrom_vio_idx)) {

		pr_info(PFX "SRAMROM violation is triggered\n");
		sramrom_vio_handler();

		*vio_idx = sramrom_vio_idx;
		for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {
			if (device_info[slave_type][i].vio_index == *vio_idx)
				*index = i;
		}

		return true;
	}

	/* check mm2nd */
	if (slave_type == mm2nd_slv_type) {
		mdp_vio = check_vio_status(slave_type, mdp_vio_idx) ==
			VIOLATION_TRIGGERED;
		disp2_vio = check_vio_status(slave_type, disp2_vio_idx) ==
			VIOLATION_TRIGGERED;
		mmsys_vio = check_vio_status(slave_type, mmsys_vio_idx) ==
			VIOLATION_TRIGGERED;

		if (mdp_vio || disp2_vio || mmsys_vio) {

			pr_info(PFX "MM2nd violation is triggered\n");
			mtk_devapc_ctx->soc->mm2nd_vio_handler(
					mtk_devapc_ctx->infracfg_base,
					mtk_devapc_ctx->soc->vio_info,
					mdp_vio, disp2_vio, mmsys_vio);

			if (mdp_vio)
				*vio_idx = mdp_vio_idx;
			else if (disp2_vio)
				*vio_idx = disp2_vio_idx;
			else if (mmsys_vio)
				*vio_idx = mmsys_vio_idx;

			for (i = 0; i < ndevices[slave_type].vio_slave_num;
					i++) {
				if (device_info[slave_type][i].vio_index ==
						*vio_idx)
					*index = i;
			}

			devapc_vio_info_print();
			return true;
		}
	}

	pr_info(PFX "%s: no violation for %s:0x%x\n", __func__,
			"slave_type", slave_type);
	return false;
}

/*
 * sync_vio_dbg - start to get violation information by selecting violation
 *		  group and enable violation shift.
 *
 * Returns sync done or not
 */
static uint32_t sync_vio_dbg(int slave_type, uint32_t shift_bit)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_vio_shift_sel_reg;
	void __iomem *pd_vio_shift_con_reg;
	uint32_t shift_count;
	uint32_t sync_done;

	if (slave_type >= slave_type_num ||
			shift_bit >= (MOD_NO_IN_1_DEVAPC * 2)) {
		pr_err(PFX "param check failed, %s:0x%x, %s:0x%x\n",
				"slave_type", slave_type,
				"shift_bit", shift_bit);
		return 0;
	}

	pd_vio_shift_sta_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_STA, 0);
	pd_vio_shift_sel_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_SEL, 0);
	pd_vio_shift_con_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_CON, 0);

	pr_debug(PFX "%s:0x%x %s:0x%x\n",
			"slave_type", slave_type,
			"VIO_SHIFT_STA", readl(pd_vio_shift_sta_reg));

	writel(0x1 << shift_bit, pd_vio_shift_sel_reg);
	writel(0x1, pd_vio_shift_con_reg);

	for (shift_count = 0; (shift_count < 100) &&
			((readl(pd_vio_shift_con_reg) & 0x3) != 0x3);
			++shift_count)
		;

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

static void devapc_vio_reason(uint8_t perm)
{
	pr_info(PFX "Permission setting: %s\n", perm_to_string(perm));

	if (perm == 0 || perm > 3)
		pr_info(PFX "Reason: power/clock is not enabled\n");
	else if (perm == 1 || perm == 2 || perm == 3)
		pr_info(PFX "Reason: might be permission denied\n");
}

/*
 * get_permission - get slave's access permission of domain id.
 *
 * Returns the value of access permission
 */
static uint8_t get_permission(int slave_type, int module_index, int domain)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	const struct mtk_device_info **device_info;
	const struct mtk_device_num *ndevices;
	int sys_index, ctrl_index, vio_index;
	uint32_t ret, apc_set_index;
	struct arm_smccc_res res;

	ndevices = mtk_devapc_ctx->soc->ndevices;

	if (slave_type >= slave_type_num ||
			module_index >= ndevices[slave_type].vio_slave_num) {
		pr_err(PFX "%s: param check failed, %s:0x%x, %s:0x%x\n",
				__func__,
				"slave_type", slave_type,
				"module_index", module_index);
		return 0xFF;
	}

	device_info = mtk_devapc_ctx->soc->device_info;

	sys_index = device_info[slave_type][module_index].sys_index;
	ctrl_index = device_info[slave_type][module_index].ctrl_index;
	vio_index = device_info[slave_type][module_index].vio_index;

	if (sys_index == -1 || ctrl_index == -1) {
		pr_err(PFX "%s: cannot get sys_index & ctrl_index\n",
				__func__);
		return 0xFF;
	} else if (sys_index == -2) {
		pr_info(PFX "%s: check ATF logs for type2 permssion\n",
				__func__);
	}

	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_PERM_GET, slave_type, sys_index,
			domain, ctrl_index, vio_index, 0, 0, &res);
	ret = res.a0;

	if (ret == DEAD) {
		pr_err(PFX "%s: permission get failed, ret:0x%x\n",
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
static void mtk_devapc_vio_check(int slave_type, int *shift_bit)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	struct mtk_devapc_vio_info *vio_info;
	uint32_t vio_shift_sta;
	int i;

	if (slave_type >= slave_type_num) {
		pr_err(PFX "%s: param check failed, %s:0x%x\n",
				__func__, "slave_type", slave_type);
		return;
	}

	vio_info = mtk_devapc_ctx->soc->vio_info;
	vio_shift_sta = readl(mtk_devapc_pd_get(slave_type, VIO_SHIFT_STA, 0));

	if (!vio_shift_sta) {
		pr_info(PFX "violation is triggered before. %s:0x%x\n",
				"shift_bit", *shift_bit);

	} else if (vio_shift_sta & (0x1UL << *shift_bit)) {
		pr_info(PFX "%s: 0x%x is matched with %s:%d\n",
				"vio_shift_sta", vio_shift_sta,
				"shift_bit", *shift_bit);

	} else {
		pr_info(PFX "%s: 0x%x is not matched with %s:%d\n",
				"vio_shift_sta", vio_shift_sta,
				"shift_bit", *shift_bit);

		for (i = 0; i < MOD_NO_IN_1_DEVAPC * 2; i++) {
			if (vio_shift_sta & (0x1 << i)) {
				*shift_bit = i;
				break;
			}
		}
	}

	vio_info->shift_sta_bit = *shift_bit;
}

static void devapc_extract_vio_dbg(int slave_type)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	void __iomem *vio_dbg0_reg, *vio_dbg1_reg, *vio_dbg2_reg;
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	struct mtk_devapc_vio_info *vio_info;
	uint32_t dbg0;

	if (slave_type >= slave_type_num) {
		pr_err(PFX "%s: param check failed, %s:0x%x\n",
				__func__, "slave_type", slave_type);
		return;
	}

	vio_dbg0_reg = mtk_devapc_pd_get(slave_type, VIO_DBG0, 0);
	vio_dbg1_reg = mtk_devapc_pd_get(slave_type, VIO_DBG1, 0);
	vio_dbg2_reg = mtk_devapc_pd_get(slave_type, VIO_DBG2, 0);

	vio_dbgs = mtk_devapc_ctx->soc->vio_dbgs;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	/* Extract violation information */
	dbg0 = readl(vio_dbg0_reg);
	vio_info->master_id = readl(vio_dbg1_reg);
	vio_info->vio_addr = readl(vio_dbg2_reg);

	vio_info->domain_id = (dbg0 & vio_dbgs->vio_dbg_dmnid)
		>> vio_dbgs->vio_dbg_dmnid_start_bit;
	vio_info->write = ((dbg0 & vio_dbgs->vio_dbg_w_vio)
			>> vio_dbgs->vio_dbg_w_vio_start_bit) == 1;
	vio_info->read = ((dbg0 & vio_dbgs->vio_dbg_r_vio)
			>> vio_dbgs->vio_dbg_r_vio_start_bit) == 1;
	vio_info->vio_addr_high = (dbg0 & vio_dbgs->vio_addr_high)
		>> vio_dbgs->vio_addr_high_start_bit;

	devapc_vio_info_print();
}

/*
 * mtk_devapc_dump_vio_dbg - shift & dump the violation debug information.
 */
static bool mtk_devapc_dump_vio_dbg(int slave_type, int *vio_idx, int *index)
{
	const struct mtk_device_info **device_info;
	const struct mtk_device_num *ndevices;
	void __iomem *pd_vio_shift_sta_reg;
	uint32_t shift_bit;
	int i;

	if (unlikely(vio_idx == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return NULL;
	}

	device_info = mtk_devapc_ctx->soc->device_info;
	ndevices = mtk_devapc_ctx->soc->ndevices;

	pd_vio_shift_sta_reg = mtk_devapc_pd_get(slave_type, VIO_SHIFT_STA, 0);

	for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {
		if (!device_info[slave_type][i].enable_vio_irq)
			continue;

		*vio_idx = device_info[slave_type][i].vio_index;
		if (check_vio_status(slave_type, *vio_idx) !=
				VIOLATION_TRIGGERED)
			continue;

		shift_bit = mtk_devapc_ctx->soc->shift_group_get(
				slave_type, *vio_idx);

		mtk_devapc_vio_check(slave_type, &shift_bit);

		if (!sync_vio_dbg(slave_type, shift_bit))
			continue;

		devapc_extract_vio_dbg(slave_type);
		*index = i;

		return true;
	}

	pr_info(PFX "check_devapc_vio_status: no violation for %s:0x%x\n",
			"slave_type", slave_type);
	return false;
}

/*
 * start_devapc - initialize devapc status and start receiving interrupt
 *		  while devapc violation is triggered.
 */
static void start_devapc(void)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	const struct mtk_device_info **device_info;
	const struct mtk_device_num *ndevices;
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_apc_con_reg;
	uint32_t vio_shift_sta;
	int slave_type, i, vio_idx, index;
	uint32_t retry = RETRY_COUNT;

	print_vio_mask_sta(false);
	ndevices = mtk_devapc_ctx->soc->ndevices;

	device_info = mtk_devapc_ctx->soc->device_info;

	for (slave_type = 0; slave_type < slave_type_num; slave_type++) {

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

		check_type2_vio_status(slave_type, &vio_idx, &i);

		/* Clear violation status */
		for (i = 0; i < ndevices[slave_type].vio_slave_num; i++) {
			if (!device_info[slave_type][i].enable_vio_irq)
				continue;

			vio_idx = device_info[slave_type][i].vio_index;
			if ((check_vio_status(slave_type, vio_idx) ==
					VIOLATION_TRIGGERED) &&
					clear_vio_status(slave_type, vio_idx)) {
				pr_warn(PFX "%s, %s:0x%x, %s:0x%x, %s:%d\n",
					"clear vio status failed",
					"slave_type", slave_type,
					"vio_index", vio_idx,
					"retry", retry);

				index = i;
				mtk_devapc_dump_vio_dbg(slave_type, &vio_idx,
						&index);

				if (--retry)
					i = index - 1;
				else  /* reset retry and continue */
					retry = RETRY_COUNT;
			}

			mask_module_irq(slave_type, vio_idx, false);
		}
	}

	print_vio_mask_sta(false);

	/* register subsys test cb */
	register_devapc_vio_callback(&devapc_test_handle);

	pr_info(PFX "%s done\n", __func__);
}

/*
 * devapc_extra_handler -
 * 1. trigger kernel exception/aee exception/kernel warning to increase devapc
 * violation severity level
 * 2. call subsys handler to get more debug information
 */
static void devapc_extra_handler(int slave_type, const char *vio_master,
				 uint32_t vio_index, uint32_t vio_addr)
{
	const struct mtk_device_info **device_info;
	struct mtk_devapc_dbg_status *dbg_stat;
	struct mtk_devapc_vio_info *vio_info;
	struct devapc_vio_callbacks *viocb;
	char dispatch_key[48] = {0};
	enum infra_subsys_id id;
	uint32_t ret_cb = 0;

	device_info = mtk_devapc_ctx->soc->device_info;
	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	pr_info(PFX "%s:%d\n", "vio_trigger_times",
			mtk_devapc_ctx->soc->vio_info->vio_trigger_times++);

	/* Dispatch slave owner if APMCU access. Others, dispatch master */
	if (!strncmp(vio_master, "APMCU", 5))
		strncpy(dispatch_key, mtk_devapc_ctx->soc->subsys_get(
				slave_type, vio_index, vio_addr),
				sizeof(dispatch_key) - 1);
	else
		strncpy(dispatch_key, vio_master, sizeof(dispatch_key) - 1);

	dispatch_key[sizeof(dispatch_key) - 1] = '\0';

	/* Callback func for vio master */
	if (!strncasecmp(vio_master, "MD", 2)) {
		id = INFRA_SUBSYS_MD;
		strncpy(dispatch_key, "MD", sizeof(dispatch_key) - 1);

	} else if (!strncasecmp(vio_master, "CONN", 4) ||
			!strncasecmp(dispatch_key, "CONN", 4)) {
		id = INFRA_SUBSYS_CONN;
		strncpy(dispatch_key, "CONNSYS", sizeof(dispatch_key) - 1);

	} else if (!strncasecmp(vio_master, "TINYSYS", 7)) {
		id = INFRA_SUBSYS_ADSP;
		strncpy(dispatch_key, "TINYSYS", sizeof(dispatch_key) - 1);

	} else if (!strncasecmp(vio_master, "GCE", 3) ||
			!strncasecmp(dispatch_key, "GCE", 3)) {
		id = INFRA_SUBSYS_GCE;
		strncpy(dispatch_key, "GCE", sizeof(dispatch_key) - 1);

	} else if (!strncasecmp(vio_master, "AUDIO", 5)) {
		id = INFRA_SUBSYS_AUDIO;
		strncpy(dispatch_key, "AUDIO", sizeof(dispatch_key) - 1);

	} else if (!strncasecmp(vio_master, "APMCU", 5))
		if (vio_info->domain_id == 0)
			id = INFRA_SUBSYS_APMCU;
		else
			id = INFRA_SUBSYS_GZ;
	else
		id = DEVAPC_SUBSYS_RESERVED;

	/* enable_ut to test callback */
	if (dbg_stat->enable_ut)
		id = DEVAPC_SUBSYS_TEST;

	list_for_each_entry(viocb, &viocb_list, list) {
		if (viocb->id == id && viocb->debug_dump)
			viocb->debug_dump();

		/* call MD cb_adv if it's registered */
		if (viocb->id == id && id == INFRA_SUBSYS_MD &&
				viocb->debug_dump_adv)
			ret_cb = viocb->debug_dump_adv(vio_addr);

		/* always call clkmgr cb if it's registered */
		if (viocb->id == DEVAPC_SUBSYS_CLKMGR &&
				viocb->debug_dump)
			viocb->debug_dump();
	}

	/* Severity level */
	if (dbg_stat->enable_KE && (ret_cb != DEVAPC_NOT_KE)) {
		pr_info(PFX "Device APC Violation Issue/%s", dispatch_key);
		BUG_ON(id != INFRA_SUBSYS_CONN);

	} else if (dbg_stat->enable_AEE) {

		/* call mtk aee_kernel_exception */
		aee_kernel_exception("[DEVAPC]",
				"%s%s\n",
				"CRDISPATCH_KEY:Device APC Violation Issue/",
				dispatch_key
				);

	} else if (dbg_stat->enable_WARN) {
		WARN(1, "Device APC Violation Issue/%s", dispatch_key);
	}
}

/*
 * devapc_violation_irq - the devapc Interrupt Service Routine (ISR) will dump
 *			  violation information including which master violates
 *			  access slave.
 */
static irqreturn_t devapc_violation_irq(int irq_number, void *dev_id)
{
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	const struct mtk_device_info **device_info;
	struct mtk_devapc_vio_info *vio_info;
	int slave_type, vio_idx, index;
	const char *vio_master;
	unsigned long flags;
	uint8_t perm;
	bool normal;

	spin_lock_irqsave(&devapc_lock, flags);

	print_vio_mask_sta(false);

	device_info = mtk_devapc_ctx->soc->device_info;
	vio_info = mtk_devapc_ctx->soc->vio_info;
	normal = false;
	vio_idx = index = -1;

	/* There are multiple DEVAPC_PD */
	for (slave_type = 0; slave_type < slave_type_num; slave_type++) {

		if (!check_type2_vio_status(slave_type, &vio_idx, &index))
			if (!mtk_devapc_dump_vio_dbg(slave_type, &vio_idx,
						&index))
				continue;

		/* Ensure that violation info are written before
		 * further operations
		 */
		smp_mb();
		normal = true;

		mask_module_irq(slave_type, vio_idx, true);

		if (clear_vio_status(slave_type, vio_idx))
			pr_warn(PFX "%s, %s:0x%x, %s:0x%x\n",
					"clear vio status failed",
					"slave_type", slave_type,
					"vio_index", vio_idx);

		perm = get_permission(slave_type, index, vio_info->domain_id);

		vio_master = mtk_devapc_ctx->soc->master_get(
				vio_info->master_id,
				vio_info->vio_addr,
				slave_type,
				vio_info->shift_sta_bit,
				vio_info->domain_id);

		if (!vio_master) {
			pr_warn(PFX "master_get failed\n");
			vio_master = "UNKNOWN_MASTER";
		}

		pr_info(PFX "%s - %s:0x%x, %s:0x%x, %s:0x%x, %s:0x%x\n",
				"Violation", "slave_type", slave_type,
				"sys_index",
				device_info[slave_type][index].sys_index,
				"ctrl_index",
				device_info[slave_type][index].ctrl_index,
				"vio_index",
				device_info[slave_type][index].vio_index);

		pr_info(PFX "%s %s %s %s\n",
				"Violation - master:", vio_master,
				"access violation slave:",
				device_info[slave_type][index].device);

		devapc_vio_reason(perm);

		devapc_extra_handler(slave_type, vio_master, vio_idx,
				vio_info->vio_addr);

		mask_module_irq(slave_type, vio_idx, false);
	}

	if (normal) {
		spin_unlock_irqrestore(&devapc_lock, flags);
		return IRQ_HANDLED;
	}

	/* It's an abnormal status */
	pr_info(PFX "WARNING: Abnormal Status\n");
	print_vio_mask_sta(true);
	//BUG_ON(1);

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
			pr_info(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
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
	struct mtk_devapc_vio_info *vio_info = mtk_devapc_ctx->soc->vio_info;
	char msg_buf[1024] = {0};
	char *p = msg_buf;
	int len;

	if (unlikely(dbg_stat == NULL) || unlikely(vio_info == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	devapc_log(p, msg_buf, "DEVAPC debug status:\n");
	devapc_log(p, msg_buf, "\tenable_ut = %d\n", dbg_stat->enable_ut);
	devapc_log(p, msg_buf, "\tenable_KE = %d\n", dbg_stat->enable_KE);
	devapc_log(p, msg_buf, "\tenable_AEE = %d\n", dbg_stat->enable_AEE);
	devapc_log(p, msg_buf, "\tenable_WARN = %d\n", dbg_stat->enable_WARN);
	devapc_log(p, msg_buf, "\tenable_dapc = %d\n", dbg_stat->enable_dapc);
	devapc_log(p, msg_buf, "\tviolation count = %d\n",
			vio_info->vio_trigger_times);
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
	uint32_t slave_type_num = mtk_devapc_ctx->soc->slave_type_num;
	long param, sys_index, domain, ctrl_index;
	struct mtk_devapc_dbg_status *dbg_stat;
	uint32_t slave_type, apc_set_idx, ret;
	char *parm_str, *cmd_str, *pinput;
	struct arm_smccc_res res;
	char input[32] = {0};
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

	err = kstrtol(parm_str, 10, &param);

	if (err)
		return err;

	if (!strncmp(cmd_str, "enable_ut", sizeof("enable_ut"))) {
		dbg_stat->enable_ut = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_ut = %s\n",
			dbg_stat->enable_ut ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "devapc_ut", sizeof("devapc_ut"))) {
		if (dbg_stat->enable_ut)
			devapc_ut(param);
		else
			pr_info(PFX "devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "enable_KE", sizeof("enable_KE"))) {
		if (dbg_stat->enable_ut) {
			dbg_stat->enable_KE = (param != 0);
			pr_info(PFX "debapc_dbg_stat->enable_KE = %s\n",
					dbg_stat->enable_KE ?
					"enable" : "disable");
		} else
			pr_info(PFX "devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "enable_AEE", sizeof("enable_AEE"))) {
		if (dbg_stat->enable_ut) {
			dbg_stat->enable_AEE = (param != 0);
			pr_info(PFX "debapc_dbg_stat->enable_AEE = %s\n",
					dbg_stat->enable_AEE ?
					"enable" : "disable");
		} else
			pr_info(PFX "devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "enable_WARN", sizeof("enable_WARN"))) {
		if (dbg_stat->enable_ut) {
			dbg_stat->enable_WARN = (param != 0);
			pr_info(PFX "debapc_dbg_stat->enable_WARN = %s\n",
					dbg_stat->enable_WARN ?
					"enable" : "disable");
		} else
			pr_info(PFX "devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "enable_dapc", sizeof("enable_dapc"))) {
		dbg_stat->enable_dapc = (param != 0);
		pr_info(PFX "debapc_dbg_stat->enable_dapc = %s\n",
			dbg_stat->enable_dapc ? "enable" : "disable");

		return count;

	} else if (!strncmp(cmd_str, "dump_apc", sizeof("dump_apc"))) {
		if (!dbg_stat->enable_dapc) {
			pr_info(PFX "dump_apc is not enabled\n");
			return -EINVAL;
		}

		/* slave_type is already parse before */
		slave_type = (uint32_t)param;

		if (slave_type >= slave_type_num) {
			pr_err(PFX "Wrong slave type:0x%x\n", slave_type);
			return -EFAULT;
		}

		sys_index = 0xFFFFFFFF;
		ctrl_index = 0xFFFFFFFF;
		domain = DOMAIN_OTHERS;

		/* Parse sys_index */
		parm_str = strsep(&pinput, " ");
		if (parm_str)
			err = kstrtol(parm_str, 10, &sys_index);

		/* Parse domain id */
		parm_str = strsep(&pinput, " ");
		if (parm_str)
			err = kstrtol(parm_str, 10, &domain);

		/* Parse ctrl_index */
		parm_str = strsep(&pinput, " ");
		if (parm_str != NULL)
			err = kstrtol(parm_str, 10, &ctrl_index);

		pr_info(PFX "%s:0x%x, %s:0x%lx, %s:0x%lx, %s:0x%lx\n",
				"slave_type", slave_type,
				"sys_index", sys_index,
				"domain_id", domain,
				"ctrl_index", ctrl_index);

		arm_smccc_smc(MTK_SIP_KERNEL_DAPC_PERM_GET, slave_type,
				sys_index, domain, ctrl_index, 0, 0, 0, &res);
		ret = res.a0;

		if (ret == DEAD) {
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

#ifdef CONFIG_DEVAPC_SWP_SUPPORT
static struct devapc_swp_context {
	void __iomem *devapc_swp_base;
	bool swp_enable;
	bool swp_clr;
	bool swp_rw;
	uint32_t swp_phy_addr;
	uint32_t swp_rg;
	uint32_t swp_wr_val;
	uint32_t swp_wr_mask;
} devapc_swp_ctx[1];

static ssize_t set_swp_addr_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
		"%s:%s\n\t%s:%s\n\t%s:0x%x\n\t%s:0x%x\n\t%s:0x%x\n\t%s:0x%x\n",
			"devapc_swp",
			devapc_swp_ctx->swp_enable ? "enable" : "disable",
			"swp_rw",
			devapc_swp_ctx->swp_rw ? "write" : "read",
			"swp_physical_addr", devapc_swp_ctx->swp_phy_addr,
			"swp_rg", devapc_swp_ctx->swp_rg,
			"swp_wr_val", devapc_swp_ctx->swp_wr_val,
			"swp_wr_mask", devapc_swp_ctx->swp_wr_mask
		       );
}

static ssize_t set_swp_addr_store(struct device_driver *driver,
		const char *buf, size_t count)
{
	char *cmd_str, *param_str;
	unsigned int param;
	int err;

	pr_info(PFX "buf: %s", buf);

	cmd_str = strsep((char **)&buf, " ");
	if (!cmd_str)
		return -EINVAL;

	param_str = strsep((char **)&buf, " ");
	if (!param_str)
		return -EINVAL;

	err = kstrtou32(param_str, 16, &param);
	if (err)
		return err;

	if (!strncmp(cmd_str, "enable_swp", sizeof("enable_swp"))) {
		devapc_swp_ctx->swp_enable = (param != 0);
		pr_info(PFX "devapc_swp_enable = %s\n",
			devapc_swp_ctx->swp_enable ? "enable" : "disable");

		writel(param, devapc_swp_ctx->devapc_swp_base);
		if (!devapc_swp_ctx->swp_enable)
			devapc_swp_ctx->swp_phy_addr = 0x0;

	} else if (!strncmp(cmd_str, "set_swp_clr", sizeof("set_swp_clr"))) {
		pr_info(PFX "set swp clear: 0x%x\n", param);
		devapc_swp_ctx->swp_clr = (param != 0);

		if (devapc_swp_ctx->swp_clr)
			writel(0x1 << DEVAPC_SWP_CON_CLEAR,
			       devapc_swp_ctx->devapc_swp_base);

	} else if (!strncmp(cmd_str, "set_swp_rw", sizeof("set_swp_rw"))) {
		pr_info(PFX "set swp r/w: %s\n", param ? "write" : "read");
		devapc_swp_ctx->swp_rw = (param != 0);

		if (devapc_swp_ctx->swp_rw)
			writel(0x1 << DEVAPC_SWP_CON_RW,
			       devapc_swp_ctx->devapc_swp_base);

	} else if (!strncmp(cmd_str, "set_swp_addr", sizeof("set_swp_addr"))) {
		pr_info(PFX "set swp physical addr: 0x%x\n", param);
		devapc_swp_ctx->swp_phy_addr = param;

		writel(devapc_swp_ctx->swp_phy_addr,
		       devapc_swp_ctx->devapc_swp_base + DEVAPC_SWP_SA_OFFSET);

	} else if (!strncmp(cmd_str, "set_swp_rg", sizeof("set_swp_rg"))) {
		pr_info(PFX "set swp range: 0x%x\n", param);
		devapc_swp_ctx->swp_rg = param;

		writel(devapc_swp_ctx->swp_rg,
		       devapc_swp_ctx->devapc_swp_base + DEVAPC_SWP_RG_OFFSET);

	} else if (!strncmp(cmd_str, "set_swp_wr_val",
				sizeof("set_swp_wr_val"))) {
		pr_info(PFX "set swp write value: 0x%x\n", param);
		devapc_swp_ctx->swp_wr_val = param;

		writel(devapc_swp_ctx->swp_wr_val,
		       devapc_swp_ctx->devapc_swp_base +
		       DEVAPC_SWP_WR_VAL_OFFSET);

	} else if (!strncmp(cmd_str, "set_swp_wr_mask",
				sizeof("set_swp_wr_mask"))) {
		pr_info(PFX "set swp write mask: 0x%x\n", param);
		devapc_swp_ctx->swp_wr_mask = param;

		writel(devapc_swp_ctx->swp_wr_mask,
		       devapc_swp_ctx->devapc_swp_base +
		       DEVAPC_SWP_WR_MASK_OFFSET);

	} else
		return -EINVAL;

	return count;
}
static DRIVER_ATTR_RW(set_swp_addr);
#endif /* CONFIG_DEVAPC_SWP_SUPPORT */

int mtk_devapc_probe(struct platform_device *pdev,
		struct mtk_devapc_soc *soc)
{
	struct device_node *node = pdev->dev.of_node;
	uint32_t slave_type_num;
	int slave_type;
	int ret;

	pr_info(PFX "driver registered\n");

	if (IS_ERR(node)) {
		pr_err(PFX "cannot find device node\n");
		return -ENODEV;
	}

	mtk_devapc_ctx->soc = soc;
	slave_type_num = mtk_devapc_ctx->soc->slave_type_num;

	for (slave_type = 0; slave_type < slave_type_num; slave_type++) {
		mtk_devapc_ctx->devapc_pd_base[slave_type] = of_iomap(node,
				slave_type);
		if (unlikely(mtk_devapc_ctx->devapc_pd_base[slave_type]
					== NULL)) {
			pr_err(PFX "parse devapc_pd_base:0x%x failed\n",
					slave_type);
			return -EINVAL;
		}
	}

	mtk_devapc_ctx->devapc_infra_ao_base = of_iomap(node, slave_type_num);
	if (unlikely(mtk_devapc_ctx->devapc_infra_ao_base == NULL)) {
		pr_err(PFX "parse devapc_infra_ao_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->infracfg_base = of_iomap(node, slave_type_num + 1);
	if (unlikely(mtk_devapc_ctx->infracfg_base == NULL)) {
		pr_err(PFX "parse infracfg_base failed\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_irq = irq_of_parse_and_map(node, 0);
	if (!mtk_devapc_ctx->devapc_irq) {
		pr_err(PFX "parse and map the interrupt failed\n");
		return -EINVAL;
	}

	for (slave_type = 0; slave_type < slave_type_num; slave_type++)
		pr_debug(PFX "%s:0x%x %s:0x%px\n",
				"slave_type", slave_type,
				"devapc_pd_base",
				mtk_devapc_ctx->devapc_pd_base[slave_type]);

	pr_debug(PFX " IRQ:%d\n", mtk_devapc_ctx->devapc_irq);

	/* CCF (Common Clock Framework) */
	mtk_devapc_ctx->devapc_infra_clk = devm_clk_get(&pdev->dev,
			"devapc-infra-clock");

	if (IS_ERR(mtk_devapc_ctx->devapc_infra_clk))
		pr_info(PFX "(Infra) Cannot get devapc clock from CCF (%d)\n",
				PTR_ERR(mtk_devapc_ctx->devapc_infra_clk));

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);

#ifdef CONFIG_DEVAPC_SWP_SUPPORT
	devapc_swp_ctx->devapc_swp_base = of_iomap(node, slave_type_num + 2);
	ret = driver_create_file(pdev->dev.driver,
			&driver_attr_set_swp_addr);
	if (ret)
		pr_info(PFX "create SWP sysfs file failed, ret:%d\n", ret);
#endif

	mtk_devapc_ctx->sramrom_base = of_iomap(node, slave_type_num + 3);
	if (unlikely(mtk_devapc_ctx->sramrom_base == NULL))
		pr_info(PFX "parse sramrom_base failed\n");

	if (!IS_ERR(mtk_devapc_ctx->devapc_infra_clk)) {
		if (clk_prepare_enable(mtk_devapc_ctx->devapc_infra_clk)) {
			pr_err(PFX " Cannot enable devapc clock\n");
			return -EINVAL;
		}
	}

	start_devapc();

	ret = devm_request_irq(&pdev->dev, mtk_devapc_ctx->devapc_irq,
			(irq_handler_t)devapc_violation_irq,
			IRQF_TRIGGER_NONE, "devapc", NULL);
	if (ret) {
		pr_err(PFX "request devapc irq failed, ret:%d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_devapc_probe);

int mtk_devapc_remove(struct platform_device *dev)
{
	if (!IS_ERR(mtk_devapc_ctx->devapc_infra_clk))
		clk_disable_unprepare(mtk_devapc_ctx->devapc_infra_clk);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_devapc_remove);

MODULE_DESCRIPTION("Mediatek Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
