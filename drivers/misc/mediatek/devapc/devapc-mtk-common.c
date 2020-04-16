// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/sched/debug.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/arm-smccc.h>
#include <mt-plat/mtk_secure_api.h>
#include <mt-plat/devapc_public.h>
#include <mt-plat/aee.h>
#include "devapc-mtk-common.h"

struct mtk_devapc_context {
	struct clk *devapc_infra_clk;
	uint32_t devapc_irq;

	/* HW reg mapped addr */
	void __iomem *devapc_pd_base;
	void __iomem *devapc_ao_base;
	void __iomem *sramrom_base;

	struct mtk_devapc_soc *soc;
} mtk_devapc_ctx[1];

LIST_HEAD(viocb_list);

/**************************************************************************
 *STATIC FUNCTION
 **************************************************************************/

static void __iomem *mtk_devapc_pd_get(enum DEVAPC_PD_REG_TYPE pd_type,
		const struct mtk_devapc_pd_desc *devapc_pds, uint32_t index)
{
	void __iomem *reg = NULL;

	if (unlikely(devapc_pds == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return NULL;
	}

	if (pd_type == VIO_MASK) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_vio_mask_offset + 0x4 * index;

	} else if (pd_type == VIO_STA) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_vio_sta_offset + 0x4 * index;

	} else if (pd_type == VIO_DBG0) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_vio_dbg0_offset;

	} else if (pd_type == VIO_DBG1) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_vio_dbg1_offset;

	} else if (pd_type == APC_CON) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_apc_con_offset;

	} else if (pd_type == VIO_SHIFT_STA) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_shift_sta_offset;

	} else if (pd_type == VIO_SHIFT_SEL) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_shift_sel_offset;

	} else if (pd_type == VIO_SHIFT_CON) {
		reg = mtk_devapc_ctx->devapc_pd_base +
			devapc_pds->pd_shift_con_offset;

	}

	return reg;
}

static void unmask_infra_module_irq(uint32_t module)
{
	uint32_t apc_index = 0;
	uint32_t apc_bit_index = 0;
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	void __iomem *reg;

	if (module > vio_max_idx) {
		DEVAPC_MSG("%s:%d module overflow!\n", __func__, __LINE__);
		return;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_MASK,
			mtk_devapc_ctx->soc->devapc_pds, apc_index);

	writel(readl(reg) & (0xFFFFFFFF ^ (1 << apc_bit_index)), reg);

}

static void mask_infra_module_irq(uint32_t module)
{
	uint32_t apc_index = 0;
	uint32_t apc_bit_index = 0;
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	void __iomem *reg;

	if (module > vio_max_idx) {
		DEVAPC_MSG("%s:%d module overflow!\n", __func__, __LINE__);
		return;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_MASK,
			mtk_devapc_ctx->soc->devapc_pds, apc_index);

	writel(readl(reg) | (1 << apc_bit_index), reg);

}

static int clear_infra_vio_status(uint32_t module)
{
	uint32_t apc_index = 0;
	uint32_t apc_bit_index = 0;
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	int sramrom_vio_idx = mtk_devapc_ctx->soc->vio_info->sramrom_vio_idx;
	void __iomem *reg;

	if (module > vio_max_idx) {
		DEVAPC_MSG("%s:%d module overflow!\n", __func__, __LINE__);
		return -EOVERFLOW;
	}

	if (module == sramrom_vio_idx)
		handle_sramrom_vio();

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_STA,
			mtk_devapc_ctx->soc->devapc_pds, apc_index);

	writel((0x1 << apc_bit_index), reg);

	return 0;
}

static int check_infra_vio_status(uint32_t module)
{
	uint32_t apc_index = 0;
	uint32_t apc_bit_index = 0;
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	void __iomem *reg;

	if (module > vio_max_idx) {
		DEVAPC_MSG("%s:%d module overflow!\n", __func__, __LINE__);
		return -EOVERFLOW;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_STA,
			mtk_devapc_ctx->soc->devapc_pds, apc_index);

	if (readl(reg) & (0x1 << apc_bit_index))
		return VIOLATION_TRIGGERED;

	return 0;
}

static void print_vio_mask_sta(void)
{
	int i;
	int vio_mask_sta_num;

	vio_mask_sta_num = mtk_devapc_ctx->soc->vio_info->vio_mask_sta_num;

	for (i = 0; i < vio_mask_sta_num; i++) {
		DEVAPC_DBG_MSG("%s: (%d:0x%x) %s: (%d:0x%x)\n",
				"INFRA VIO_MASK", i,
				readl(mtk_devapc_pd_get(VIO_MASK,
						mtk_devapc_ctx->soc->devapc_pds,
						i)
				),
				"INFRA VIO_STA", i,
				readl(mtk_devapc_pd_get(VIO_STA,
						mtk_devapc_ctx->soc->devapc_pds,
						i)
				)
		);
	}
}

static void devapc_test_cb(void)
{
	DEVAPC_MSG("%s success !\n", __func__);
}

static struct devapc_vio_callbacks devapc_test_handle = {
	.id = DEVAPC_SUBSYS_TEST,
	.debug_dump = devapc_test_cb,
};

static void start_devapc(void)
{
	int i;
	uint32_t vio_shift_sta;
	void __iomem *pd_apc_con_reg;
	void __iomem *pd_vio_shift_sta_reg;
	const struct mtk_device_info *device_info;

	DEVAPC_MSG("%s...\n", __func__);

	pd_apc_con_reg = mtk_devapc_pd_get(APC_CON,
			mtk_devapc_ctx->soc->devapc_pds, 0);
	pd_vio_shift_sta_reg = mtk_devapc_pd_get(VIO_SHIFT_STA,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	device_info = mtk_devapc_ctx->soc->device_info;

	if (unlikely(pd_apc_con_reg == NULL || pd_vio_shift_sta_reg == NULL ||
				device_info == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return;
	}


	writel(0x80000000, pd_apc_con_reg);
	print_vio_mask_sta();

	DEVAPC_DBG_MSG("Clear INFRA VIO_STA and unmask INFRA VIO_MASK...\n");

	vio_shift_sta = readl(pd_vio_shift_sta_reg);
	if (vio_shift_sta) {
		DEVAPC_MSG("(Pre) clear VIO_SHIFT_STA = 0x%x\n", vio_shift_sta);

		writel(vio_shift_sta, pd_vio_shift_sta_reg);

		DEVAPC_MSG("(Post) clear VIO_SHIFT_STA = 0x%x\n",
				readl(pd_vio_shift_sta_reg));
	} else
		DEVAPC_MSG("No violation happened before booting kernel\n");

	for (i = 0; i < mtk_devapc_ctx->soc->ndevices; i++)
		if (true == device_info[i].enable_vio_irq) {
			clear_infra_vio_status(i);
			unmask_infra_module_irq(i);
		}

	print_vio_mask_sta();

	register_devapc_vio_callback(&devapc_test_handle);
}

static void devapc_violation_triggered(uint32_t vio_idx,
				       uint32_t vio_addr,
				       const char *vio_master)
{
	char subsys_str[48] = {0};
	struct devapc_vio_callbacks *viocb;
	enum infra_subsys_id id = DEVAPC_SUBSYS_RESERVED;
	const struct mtk_device_info *device_info;
	struct mtk_devapc_dbg_status *dbg_stat;

	device_info = mtk_devapc_ctx->soc->device_info;
	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;

	if (unlikely(dbg_stat == NULL || device_info == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return;
	}

	DEVAPC_MSG("%s, count=%d\n", __func__,
		mtk_devapc_ctx->soc->vio_info->devapc_vio_trigger_times++);

	/* mask irq for module index "vio_idx" */
	mask_infra_module_irq(vio_idx);

	/* Dispatch slave owner if APMCU access. Others, dispatch master. */
	if (!strncmp(vio_master, "APMCU", 5)) {
		strncpy(subsys_str, mtk_devapc_ctx->soc->subsys_get(vio_idx),
			sizeof(subsys_str));
	} else {
		strncpy(subsys_str, vio_master,
			sizeof(subsys_str));
	}

	subsys_str[sizeof(subsys_str) - 1] = '\0';

	/* Callback func for vio master */
	if (!strncasecmp(vio_master, "MD", 2))
		id = INFRA_SUBSYS_MD;
	else if (!strncasecmp(vio_master, "CONNSYS", 7))
		id = INFRA_SUBSYS_CONN;
	else if (!strncasecmp(vio_master, "HIFI3", 5))
		id = INFRA_SUBSYS_ADSP;
	else if (!strncasecmp(vio_master, "GCE", 3))
		id = INFRA_SUBSYS_GCE;
	else if (!strncasecmp(vio_master, "APMCU", 5))
		id = INFRA_SUBSYS_APMCU;

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

	if (dbg_stat->enable_KE) {
		DEVAPC_MSG("Violation master: %s access %s\n", vio_master,
				device_info[vio_idx].device);
		DEVAPC_MSG("Device APC Violation Issue/%s", subsys_str);

		/* Connsys will trigger EE instead of AP KE */
		if (id != INFRA_SUBSYS_CONN)
			BUG();
	} else if (dbg_stat->enable_AEE) {

		/* call mtk aee_kernel_exception */
		aee_kernel_exception("[DEVAPC]",
				"%s %s %s %s, Vio Addr: 0x%x\n%s%s\n",
				"Violation Master:",
				vio_master,
				"Access Violation Slave:",
				device_info[vio_idx].device,
				vio_addr,
				"CRDISPATCH_KEY:Device APC Violation Issue/",
				subsys_str
				);

	}

	/* unmask irq for module index "vio_idx" */
	unmask_infra_module_irq(vio_idx);

}

static uint32_t sync_vio_dbg(int shift_bit)
{
	uint32_t shift_count = 0;
	uint32_t sync_done;
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_vio_shift_sel_reg;
	void __iomem *pd_vio_shift_con_reg;

	pd_vio_shift_sta_reg = mtk_devapc_pd_get(VIO_SHIFT_STA,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	pd_vio_shift_sel_reg = mtk_devapc_pd_get(VIO_SHIFT_SEL,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	pd_vio_shift_con_reg = mtk_devapc_pd_get(VIO_SHIFT_CON,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	writel(0x1 << shift_bit, pd_vio_shift_sel_reg);
	writel(0x1, pd_vio_shift_con_reg);

	for (shift_count = 0; (shift_count < 100) &&
		((readl(pd_vio_shift_con_reg) & 0x3) != 0x3);
		++shift_count)
		DEVAPC_DBG_MSG("Syncing VIO DBG0 & DBG1 (%d, %d)\n",
				shift_bit, shift_count);

	if ((readl(pd_vio_shift_con_reg) & 0x3) == 0x3)
		sync_done = 1;
	else {
		sync_done = 0;
		DEVAPC_MSG("sync failed, shift_bit: %d\n",
				shift_bit);
	}

	/* disable shift mechanism */
	writel(0x0, pd_vio_shift_con_reg);
	writel(0x0, pd_vio_shift_sel_reg);
	writel(0x1 << shift_bit, pd_vio_shift_sta_reg);

	DEVAPC_DBG_MSG("%s%X, %s%X, %s%X\n",
			"VIO_SHIFT_STA=0x",
			readl(pd_vio_shift_sta_reg),
			"VIO_SHIFT_SEL=0x",
			readl(pd_vio_shift_sel_reg),
			"VIO_SHIFT_CON=0x",
			readl(pd_vio_shift_con_reg));

	return sync_done;
}

static void dump_backtrace(void *passed_regs)
{
	struct pt_regs *regs = passed_regs;

	DEVAPC_MSG("====== %s ======\n",
			"Start dumping Device APC violation tracing");

	DEVAPC_MSG("****** %s ******\n",
			"[All IRQ Registers]");
	if (regs)
		show_regs(regs);

	DEVAPC_MSG("****** %s ******\n",
			"[All Current Task Stack]");
	show_stack(current, NULL);

	DEVAPC_MSG("====== %s ======\n",
			"End of dumping Device APC violation tracing");
}

static char *perm_to_string(uint32_t perm)
{
	if (perm == 0x0)
		return "NO_PROTECTION";
	else if (perm == 0x1)
		return "SECURE_RW_ONLY";
	else if (perm == 0x2)
		return "SECURE_RW_NS_R_ONLY";
	else if (perm == 0x3)
		return "FORBIDDEN";
	else
		return "NO_PERM_CTRL";
}

static uint32_t get_permission(int vio_index, int domain)
{
	int slave_type;
	int config_idx;
	int apc_set_idx;
	uint32_t ret;
	struct arm_smccc_res res;
	const struct mtk_device_info *device_info;

	device_info = mtk_devapc_ctx->soc->device_info;
	if (vio_index >= mtk_devapc_ctx->soc->ndevices)
		return -EOVERFLOW;

	slave_type = device_info[vio_index].slave_type;
	config_idx = device_info[vio_index].config_index;

	DEVAPC_DBG_MSG("%s, slave type = 0x%x, config_idx = 0x%x\n",
			__func__,
			slave_type,
			config_idx);

	if (slave_type >= E_DAPC_OTHERS_SLAVE || config_idx == -1) {
		DEVAPC_MSG("%s, cannot get APC\n", __func__);
		return DEAD;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_PERM_GET,
			slave_type, domain, config_idx, 0, 0, 0, 0, &res);
	ret = res.a0;

	if (ret == DEAD) {
		DEVAPC_MSG("%s, param is overflow\n", __func__);
		return ret;
	}

	DEVAPC_DBG_MSG("%s, dump perm = 0x%x\n", __func__, ret);

	apc_set_idx = config_idx % MOD_NO_IN_1_DEVAPC;
	ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

	DEVAPC_DBG_MSG("%s, after shipping, dump perm = 0x%x\n",
			__func__,
			(ret & 0x3));

	return (ret & 0x3);
}

void register_devapc_vio_callback(struct devapc_vio_callbacks *viocb)
{
	INIT_LIST_HEAD(&viocb->list);
	list_add_tail(&viocb->list, &viocb_list);
}
EXPORT_SYMBOL(register_devapc_vio_callback);

uint32_t devapc_vio_check(void)
{
	void __iomem *reg;

	reg = mtk_devapc_pd_get(VIO_SHIFT_STA,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	return readl(reg);
}
EXPORT_SYMBOL(devapc_vio_check);

void dump_dbg_info(void)
{
	uint32_t dbg0 = 0;
	uint32_t write_vio, read_vio;
	uint32_t vio_addr_high;
	int i;
	void __iomem *vio_dbg0_reg, *vio_dbg1_reg;
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	struct mtk_devapc_vio_info *vio_info;

	vio_dbg0_reg = mtk_devapc_pd_get(VIO_DBG0,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	vio_dbg1_reg = mtk_devapc_pd_get(VIO_DBG1,
			mtk_devapc_ctx->soc->devapc_pds, 0);

	vio_dbgs = mtk_devapc_ctx->soc->vio_dbgs;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	for (i = 0; i <= vio_info->vio_shift_max_bit; ++i) {
		if (devapc_vio_check() & (0x1 << i)) {

			if (sync_vio_dbg(i) == 0)
				continue;

			dbg0 = readl(vio_dbg0_reg);
			vio_info->vio_dbg1 = readl(vio_dbg1_reg);

			vio_info->master_id =
				(dbg0 & vio_dbgs->infra_vio_dbg_mstid)
				>> vio_dbgs->infra_vio_dbg_mstid_start_bit;
			vio_info->domain_id =
				(dbg0 & vio_dbgs->infra_vio_dbg_dmnid)
				>> vio_dbgs->infra_vio_dbg_dmnid_start_bit;
			write_vio = (dbg0 & vio_dbgs->infra_vio_dbg_w_vio)
				>> vio_dbgs->infra_vio_dbg_w_vio_start_bit;
			read_vio = (dbg0 & vio_dbgs->infra_vio_dbg_r_vio)
				>> vio_dbgs->infra_vio_dbg_r_vio_start_bit;
			vio_addr_high = (dbg0 & vio_dbgs->infra_vio_addr_high)
				>> vio_dbgs->infra_vio_addr_high_start_bit;

			/* violation information */
			DEVAPC_MSG("%s%s%s%s%x %s%x, %s%x, %s%x\n",
					"Violation(",
					read_vio == 1?" R":"",
					write_vio == 1?" W ) - ":" ) - ",
					"Vio Addr:0x", vio_info->vio_dbg1,
					"High:0x", vio_addr_high,
					"Bus ID:0x", vio_info->master_id,
					"Dom ID:0x", vio_info->domain_id);

			DEVAPC_MSG("%s - %s%s, %s%i\n",
					"Violation",
					"Current Process:", current->comm,
					"PID:", current->pid);
		}
	}

}
EXPORT_SYMBOL(dump_dbg_info);

void handle_sramrom_vio(void)
{
	size_t sramrom_vio_sta, sramrom_vio_addr;
	int rw, sramrom_vio;
	uint32_t master_id, domain_id, dbg1;
	struct arm_smccc_res res;
	const struct mtk_sramrom_sec_vio_desc *sramrom_vios;

	sramrom_vios = mtk_devapc_ctx->soc->sramrom_sec_vios;

	arm_smccc_smc(MTK_SIP_KERNEL_CLR_SRAMROM_VIO,
			0, 0, 0, 0, 0, 0, 0, &res);

	sramrom_vio = res.a0;
	sramrom_vio_sta = res.a1;
	sramrom_vio_addr = res.a2;

	if (sramrom_vio == SRAM_VIOLATION)
		DEVAPC_MSG("%s, SRAM violation is triggered\n", __func__);
	else if (sramrom_vio == ROM_VIOLATION)
		DEVAPC_MSG("%s, ROM violation is triggered\n", __func__);
	else {
		DEVAPC_MSG("SRAMROM violation is not triggered\n");
		return;
	}

	master_id =
		(sramrom_vio_sta & sramrom_vios->sramrom_sec_vio_id_mask) >>
			sramrom_vios->sramrom_sec_vio_id_shift;
	domain_id =
		(sramrom_vio_sta & sramrom_vios->sramrom_sec_vio_domain_mask) >>
			sramrom_vios->sramrom_sec_vio_domain_shift;
	rw =
		(sramrom_vio_sta & sramrom_vios->sramrom_sec_vio_rw_mask) >>
			sramrom_vios->sramrom_sec_vio_rw_shift;
	dbg1 = sramrom_vio_addr;

	DEVAPC_MSG("%s, %s: 0x%x, %s: 0x%x, rw: %s, vio addr: 0x%x\n",
		__func__, "master_id", master_id,
		"domain_id", domain_id,
		rw ? "Write" : "Read",
		dbg1
	);
}

static irqreturn_t devapc_violation_irq(int irq_number, void *dev_id)
{
	int i, device_count;
	uint32_t perm;
	const char *vio_master;
	struct pt_regs *regs = get_irq_regs();
	const struct mtk_device_info *device_info;
	struct mtk_devapc_vio_info *vio_info;

	device_info = mtk_devapc_ctx->soc->device_info;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	if (irq_number != mtk_devapc_ctx->devapc_irq) {
		DEVAPC_MSG("(ERROR) irq_number %d is not registered\n",
			irq_number);

		return IRQ_NONE;
	}
	print_vio_mask_sta();
	dump_dbg_info();

	device_count = mtk_devapc_ctx->soc->ndevices;

	/* checking and showing violation normal slaves */
	for (i = 0; i < device_count; i++) {
		if (device_info[i].enable_vio_irq == true
			&& check_infra_vio_status(i) == VIOLATION_TRIGGERED) {

			clear_infra_vio_status(i);
			perm = get_permission(i, vio_info->domain_id);
			vio_master = mtk_devapc_ctx->soc->master_get(
					vio_info->master_id,
					vio_info->vio_dbg1, i);

			if (vio_master == NULL)
				vio_master = "UNKNOWN";

			DEVAPC_MSG("%s %s %s %s (%s=%d)\n",
				"Violation Master:",
				vio_master,
				"Access Violation Slave:",
				device_info[i].device,
				"vio idx",
				i);
			DEVAPC_MSG("Permission: %s\n",
				perm_to_string(perm));

			devapc_violation_triggered(i, vio_info->vio_dbg1,
					vio_master);
		}
	}

	dump_backtrace(regs);

	return IRQ_HANDLED;
}

static const char *mtk_sid_to_str(int sid)
{
	if (sid == INFRA_SUBSYS_MD)
		return "INFRA_SUBSYS_MD";
	else if (sid == INFRA_SUBSYS_CONN)
		return "INFRA_SUBSYS_CONN";
	else if (sid == INFRA_SUBSYS_ADSP)
		return "INFRA_SUBSYS_ADSP";
	else if (sid == INFRA_SUBSYS_GCE)
		return "INFRA_SUBSYS_GCE";
	else if (sid == DEVAPC_SUBSYS_CLKMGR)
		return "DEVAPC_SUBSYS_CLKMGR";
	else if (sid == DEVAPC_SUBSYS_TEST)
		return "DEVAPC_SUBSYS_TEST";

	return "UNKNOWN_SUBSYS";
}

static void devapc_ut(uint32_t cmd)
{
	uint32_t offset = 0x88;
	void __iomem *devapc_ao_base, *sramrom_base;
	struct devapc_vio_callbacks *viocb;

	devapc_ao_base = mtk_devapc_ctx->devapc_ao_base;
	sramrom_base = mtk_devapc_ctx->sramrom_base;

	DEVAPC_MSG("%s, test violation..., cmd = %d\n", __func__, cmd);

	if (cmd == DEVAPC_UT_DAPC_VIO) {
		if (unlikely(devapc_ao_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		DEVAPC_MSG("%s, devapc_ao_infra_base = 0x%x\n",
			__func__,
			readl(devapc_ao_base));

		DEVAPC_MSG("%s, test done, it should generate violation!\n",
			__func__);

	} else if (cmd == DEVAPC_UT_SRAM_VIO) {
		if (unlikely(sramrom_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		DEVAPC_MSG("%s, sramrom_base = 0x%x\n",
			__func__,
			readl(sramrom_base + offset));

		DEVAPC_MSG("%s, test done, it should generate violation!\n",
			__func__);

	} else if (cmd == DEVAPC_UT_DUMP_SUBSYS_CB) {
		DEVAPC_MSG("DEVAPC dump subsys cb:\n");
		list_for_each_entry(viocb, &viocb_list, list) {
			if (viocb->id != DEVAPC_SUBSYS_RESERVED &&
					viocb->debug_dump) {
				DEVAPC_MSG("\t%s is registered\n",
						mtk_sid_to_str(viocb->id));
			}
		}

	} else {
		DEVAPC_MSG("%s, cmd(0x%x) not supported\n", __func__, cmd);
	}
}

int mtk_devapc_probe(struct platform_device *pdev,
		struct mtk_devapc_soc *soc)
{
	struct device_node *node = pdev->dev.of_node;
	int ret;

	DEVAPC_MSG("driver registered\n");

	if (IS_ERR(node)) {
		pr_err(PFX "cannot find device node\n");
		return -ENODEV;
	}

	mtk_devapc_ctx->soc = soc;
	mtk_devapc_ctx->devapc_pd_base = of_iomap(node,
			DT_DEVAPC_PD_IDX);
	mtk_devapc_ctx->devapc_ao_base = of_iomap(node,
			DT_DEVAPC_AO_IDX);
	mtk_devapc_ctx->devapc_irq = irq_of_parse_and_map(node,
			DT_DEVAPC_PD_IDX);

	DEVAPC_DBG_MSG("devapc_pd_base: %p, IRQ: %d\n",
			mtk_devapc_ctx->devapc_pd_base,
			mtk_devapc_ctx->devapc_irq);

	ret = request_irq(mtk_devapc_ctx->devapc_irq,
			(irq_handler_t)devapc_violation_irq,
			IRQF_TRIGGER_LOW, "devapc", NULL);
	if (ret) {
		pr_err(PFX "Failed to request devapc irq, ret(%d)\n", ret);
		return ret;
	}

	/* CCF (Common Clock Framework) */
	mtk_devapc_ctx->devapc_infra_clk = devm_clk_get(&pdev->dev,
			"devapc-infra-clock");

	if (IS_ERR(mtk_devapc_ctx->devapc_infra_clk)) {
		pr_err(PFX "(Infra) Cannot get devapc clock from CCF.\n");
		return PTR_ERR(mtk_devapc_ctx->devapc_infra_clk);
	}

	clk_prepare_enable(mtk_devapc_ctx->devapc_infra_clk);
	start_devapc();

	return 0;
}

int mtk_devapc_remove(struct platform_device *dev)
{
	clk_disable_unprepare(mtk_devapc_ctx->devapc_infra_clk);
	return 0;
}

ssize_t mtk_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	int len;
	char msg_buf[1024] = {0};
	char *p = msg_buf;
	struct mtk_devapc_dbg_status *dbg_stat;

	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;
	if (unlikely(dbg_stat == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return -EINVAL;
	}

	devapc_log("DEVAPC debug status:\n");
	devapc_log("\tenable_ut = %d\n", dbg_stat->enable_ut);
	devapc_log("\tenable_KE = %d\n", dbg_stat->enable_KE);
	devapc_log("\tenable_AEE = %d\n", dbg_stat->enable_AEE);
	devapc_log("\tenable_dapc = %d\n", dbg_stat->enable_dapc);
	devapc_log("\n");

	len = p - msg_buf;

	return simple_read_from_buffer(buffer, count, ppos, msg_buf, len);
}

ssize_t mtk_devapc_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char input[32] = {0};
	char *cmd_str = NULL;
	char *parm_str = NULL;
	char *pinput = NULL;	/* pointer to input */
	unsigned long param = 0;
	uint32_t ret;
	int err = 0, len = 0, apc_set_idx;
	long slave_type = 0, domain = 0, index = 0;
	struct arm_smccc_res res;
	struct mtk_devapc_dbg_status *dbg_stat;

	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;
	if (unlikely(dbg_stat == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return -EINVAL;
	}


	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		DEVAPC_MSG("copy from user failed!\n");
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");

	if (cmd_str == NULL)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");

	if (parm_str == NULL)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);

	if (err != 0)
		return err;

	if (!strncmp(cmd_str, "enable_ut", sizeof("enable_ut"))) {
		dbg_stat->enable_ut = (param != 0);
		DEVAPC_MSG("debapc_dbg_stat->enable_ut = %s\n",
			dbg_stat->enable_ut ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_KE", sizeof("enable_KE"))) {
		dbg_stat->enable_KE = (param != 0);
		DEVAPC_MSG("debapc_dbg_stat->enable_KE = %s\n",
			dbg_stat->enable_KE ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_AEE", sizeof("enable_AEE"))) {
		dbg_stat->enable_AEE = (param != 0);
		DEVAPC_MSG("debapc_dbg_stat->enable_AEE = %s\n",
			dbg_stat->enable_AEE ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "enable_dapc", sizeof("enable_dapc"))) {
		dbg_stat->enable_dapc = (param != 0);
		DEVAPC_MSG("debapc_dbg_stat->enable_dapc = %s\n",
			dbg_stat->enable_dapc ? "enable" : "disable");
		return count;

	} else if (!strncmp(cmd_str, "devapc_ut", sizeof("devapc_ut"))) {
		if (dbg_stat->enable_ut)
			devapc_ut(param);
		else
			DEVAPC_MSG("devapc_ut is not enabled\n");

		return count;

	} else if (!strncmp(cmd_str, "dump_apc", sizeof("dump_apc"))) {

		if (!dbg_stat->enable_dapc) {
			DEVAPC_MSG("dump_apc is not enabled\n");
			return -EINVAL;
		}

		/* slave_type is already parse before */
		slave_type = param;

		if (slave_type >= E_DAPC_OTHERS_SLAVE || err) {
			DEVAPC_MSG("Wrong slave type(%lu), err(0x%x)\n",
				slave_type, err);
			return -EFAULT;
		}

		parm_str = strsep(&pinput, " ");
		if (parm_str != NULL)
			err = kstrtol(parm_str, 10, &domain);
		else
			domain = E_DOMAIN_OTHERS;

		if (domain >= E_DOMAIN_OTHERS || err) {
			DEVAPC_MSG("Wrong domain type(%lu), err(0x%x)\n",
				domain, err);
			return -EFAULT;
		}

		parm_str = strsep(&pinput, " ");
		if (parm_str != NULL)
			err = kstrtol(parm_str, 10, &index);
		else
			index = 0xFFFFFFFF;

		if (index > mtk_devapc_ctx->soc->vio_info->vio_cfg_max_idx ||
				err) {
			DEVAPC_MSG("Wrong index(%lu), err(0x%x)\n",
				index, err);
			return -EFAULT;
		}

		DEVAPC_MSG("slave_type = %lu\n", slave_type);
		DEVAPC_MSG("domain id = %lu\n", domain);
		DEVAPC_MSG("slave config_idx = %lu\n", index);

		arm_smccc_smc(MTK_SIP_KERNEL_DAPC_PERM_GET,
				slave_type, domain, index, 0, 0, 0, 0, &res);
		ret = res.a0;

		if (ret == DEAD) {
			DEVAPC_MSG("%s, param is overflow\n", __func__);
			return -EOVERFLOW;
		}

		DEVAPC_MSG("dump perm = 0x%x\n", ret);

		apc_set_idx = index % MOD_NO_IN_1_DEVAPC;
		ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

		DEVAPC_MSG("The permission is %s\n",
			perm_to_string((ret & 0x3)));
		return count;
	} else {
		return -EINVAL;
	}

	return count;
}

