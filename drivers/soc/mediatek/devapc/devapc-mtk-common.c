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
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/uaccess.h>
#include "devapc-mtk-common.h"

static struct mtk_devapc_context {
	struct clk *devapc_infra_clk;
	uint32_t devapc_irq;

	/* HW reg mapped addr */
	void __iomem *devapc_pd_base;
	void __iomem *devapc_ao_base;
	void __iomem *sramrom_base;

	struct mtk_devapc_soc *soc;
	struct mutex viocb_list_lock;
} mtk_devapc_ctx[1];

static LIST_HEAD(viocb_list);
static DEFINE_SPINLOCK(devapc_lock);

/*
 * mtk_devapc_pd_get - get devapc pd_types of register address.
 *
 * Returns the value of reg addr
 */
static void __iomem *mtk_devapc_pd_get(enum DEVAPC_PD_REG_TYPE pd_type,
				       uint32_t index)
{
	const uint32_t *devapc_pds = mtk_devapc_ctx->soc->devapc_pds;
	void __iomem *reg;

	if (unlikely(devapc_pds == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return NULL;
	}

	reg = mtk_devapc_ctx->devapc_pd_base + devapc_pds[pd_type];

	if (pd_type == VIO_MASK || pd_type == VIO_STA)
		reg += 0x4 * index;

	return reg;
}

/*
 * handle_sramrom_vio - clean sramrom violation & print violation information
 *			for debugging.
 */
static void handle_sramrom_vio(void)
{
	const struct mtk_sramrom_sec_vio_desc *sramrom_vios;
	size_t sramrom_vio_sta, sramrom_vio_addr;
	uint32_t master_id, domain_id, dbg1;
	struct arm_smccc_res res;
	int rw, sramrom_vio;

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
	dbg1 = sramrom_vio_addr;

	pr_info(PFX "%s, %s: 0x%x, %s: 0x%x, rw: %s, vio addr: 0x%x\n",
		__func__, "master_id", master_id,
		"domain_id", domain_id,
		rw ? "Write" : "Read",
		dbg1);
}

static void mask_infra_module_irq(uint32_t module, bool mask)
{
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	uint32_t apc_bit_index;
	uint32_t apc_index;
	void __iomem *reg;

	if (module > vio_max_idx) {
		pr_err(PFX "%s:%d module overflow!\n", __func__, __LINE__);
		return;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_MASK, apc_index);

	if (mask)
		writel(readl(reg) | (1 << apc_bit_index), reg);
	else
		writel(readl(reg) & (~(1 << apc_bit_index)), reg);
}

static int32_t check_infra_vio_status(uint32_t module)
{
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	uint32_t apc_bit_index;
	uint32_t apc_index;
	void __iomem *reg;

	if (module > vio_max_idx) {
		pr_err(PFX "%s:%d module overflow!\n", __func__, __LINE__);
		return -EOVERFLOW;
	}

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_STA, apc_index);

	if (readl(reg) & (0x1 << apc_bit_index))
		return VIOLATION_TRIGGERED;

	return 0;
}

static int32_t clear_infra_vio_status(uint32_t module)
{
	int sramrom_vio_idx = mtk_devapc_ctx->soc->vio_info->sramrom_vio_idx;
	int vio_max_idx = mtk_devapc_ctx->soc->vio_info->vio_max_idx;
	uint32_t apc_bit_index;
	uint32_t apc_index;
	void __iomem *reg;

	if (module > vio_max_idx) {
		pr_err(PFX "%s:%d module overflow!\n", __func__, __LINE__);
		return -EOVERFLOW;
	}

	if (module == sramrom_vio_idx)
		handle_sramrom_vio();

	apc_index = module / (MOD_NO_IN_1_DEVAPC * 2);
	apc_bit_index = module % (MOD_NO_IN_1_DEVAPC * 2);

	reg = mtk_devapc_pd_get(VIO_STA, apc_index);
	writel((0x1 << apc_bit_index), reg);

	if (check_infra_vio_status(module))
		return -EIO;

	return 0;
}

static void print_vio_mask_sta(void)
{
	int vio_mask_sta_num = mtk_devapc_ctx->soc->vio_info->vio_mask_sta_num;
	int i;

	for (i = 0; i < vio_mask_sta_num; i++) {
		pr_debug(PFX "%s: (%d:0x%x) %s: (%d:0x%x)\n",
				"INFRA VIO_MASK", i,
				readl(mtk_devapc_pd_get(VIO_MASK, i)),
				"INFRA VIO_STA", i,
				readl(mtk_devapc_pd_get(VIO_STA, i))
		);
	}
}

/*
 * start_devapc - initialize devapc status and start receiving interrupt
 *		  while devapc violation is triggered.
 */
static void start_devapc(void)
{
	const struct mtk_device_info *device_info;
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_apc_con_reg;
	uint32_t vio_shift_sta;
	int i;

	pd_apc_con_reg = mtk_devapc_pd_get(APC_CON, 0);
	pd_vio_shift_sta_reg = mtk_devapc_pd_get(VIO_SHIFT_STA, 0);

	device_info = mtk_devapc_ctx->soc->device_info;

	if (unlikely(pd_apc_con_reg == NULL ||
		     pd_vio_shift_sta_reg == NULL ||
		     device_info == NULL)) {
		pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
		return;
	}

	writel(0x80000000, pd_apc_con_reg);
	print_vio_mask_sta();

	vio_shift_sta = readl(pd_vio_shift_sta_reg);
	if (vio_shift_sta) {
		writel(vio_shift_sta, pd_vio_shift_sta_reg);
		pr_info(PFX "clear VIO_SHIFT_STA: 0x%x to 0x%x\n",
				vio_shift_sta,
				readl(pd_vio_shift_sta_reg));

	}

	for (i = 0; i < mtk_devapc_ctx->soc->ndevices; i++) {
		if (device_info[i].enable_vio_irq) {
			if (check_infra_vio_status(i) == VIOLATION_TRIGGERED &&
					clear_infra_vio_status(i))
				pr_warn(PFX "clear vio status failed\n");

			mask_infra_module_irq(i, false);
		}
	}

	print_vio_mask_sta();
}

/*
 * sync_vio_dbg - start to get violation information by selecting violation
 *		  group and enable violation shift.
 *
 * Returns sync done or not
 */
static uint32_t sync_vio_dbg(int shift_bit)
{
	void __iomem *pd_vio_shift_sta_reg;
	void __iomem *pd_vio_shift_sel_reg;
	void __iomem *pd_vio_shift_con_reg;
	uint32_t shift_count = 0;
	uint32_t sync_done;

	pd_vio_shift_sta_reg = mtk_devapc_pd_get(VIO_SHIFT_STA, 0);
	pd_vio_shift_sel_reg = mtk_devapc_pd_get(VIO_SHIFT_SEL, 0);
	pd_vio_shift_con_reg = mtk_devapc_pd_get(VIO_SHIFT_CON, 0);

	pr_info(PFX "VIO_SHIFT_STA = 0x%x\n", readl(pd_vio_shift_sta_reg));

	writel(0x1 << shift_bit, pd_vio_shift_sel_reg);
	writel(0x1, pd_vio_shift_con_reg);

	for (shift_count = 0; (shift_count < 100) &&
			((readl(pd_vio_shift_con_reg) & 0x3) != 0x3);
			++shift_count)
		pr_debug(PFX "Syncing VIO DBG0 & DBG1 (%d, %d)\n",
				shift_bit, shift_count);

	if ((readl(pd_vio_shift_con_reg) & 0x3) == 0x3) {
		sync_done = 1;
	} else {
		sync_done = 0;
		pr_info(PFX "sync failed, shift_bit: %d\n", shift_bit);
	}

	/* disable shift mechanism */
	writel(0x0, pd_vio_shift_con_reg);
	writel(0x0, pd_vio_shift_sel_reg);
	writel(0x1 << shift_bit, pd_vio_shift_sta_reg);

	pr_debug(PFX "(Post) %s%X, %s%X, %s%X\n",
			"VIO_SHIFT_STA=0x",
			readl(pd_vio_shift_sta_reg),
			"VIO_SHIFT_SEL=0x",
			readl(pd_vio_shift_sel_reg),
			"VIO_SHIFT_CON=0x",
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
static uint8_t get_permission(int vio_index, int domain)
{
	const struct mtk_device_info *device_info;
	struct arm_smccc_res res;
	int apc_set_idx;
	int slave_type;
	int config_idx;
	uint32_t ret;

	device_info = mtk_devapc_ctx->soc->device_info;

	slave_type = device_info[vio_index].slave_type;
	config_idx = device_info[vio_index].config_index;

	if (slave_type >= E_DAPC_OTHERS_SLAVE || config_idx == -1) {
		pr_err(PFX "%s, cannot get APC\n", __func__);
		return 0xFF;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_DAPC_DUMP,
			slave_type, domain, config_idx, 0, 0, 0, 0, &res);
	ret = res.a0;

	if (ret == DEAD || ret == SIP_SVC_E_NOT_SUPPORTED) {
		pr_err(PFX "%s, SMC call failed, ret: 0x%x\n",
				__func__, ret);
		return 0xFF;
	}

	apc_set_idx = config_idx % MOD_NO_IN_1_DEVAPC;
	ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

	return (ret & 0x3);
}

/*
 * mtk_devapc_vio_check - check violation shift status is raised or not.
 *
 * Returns the value of violation shift status reg
 */
static uint32_t mtk_devapc_vio_check(void)
{
	return readl(mtk_devapc_pd_get(VIO_SHIFT_STA, 0));
}

/*
 * mtk_devapc_dump_vio_dbg - shift & dump the violation debug information.
 */
static void mtk_devapc_dump_vio_dbg(void)
{
	const struct mtk_infra_vio_dbg_desc *vio_dbgs;
	void __iomem *vio_dbg0_reg, *vio_dbg1_reg;
	struct mtk_devapc_vio_info *vio_info;
	uint32_t write_vio, read_vio;
	uint32_t vio_addr_high;
	uint32_t dbg0 = 0;
	int i;

	vio_dbg0_reg = mtk_devapc_pd_get(VIO_DBG0, 0);
	vio_dbg1_reg = mtk_devapc_pd_get(VIO_DBG1, 0);

	vio_dbgs = mtk_devapc_ctx->soc->vio_dbgs;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	for (i = 0; i <= vio_info->vio_shift_max_bit; ++i) {
		if (mtk_devapc_vio_check() & (0x1 << i)) {

			if (sync_vio_dbg(i) == 0)
				continue;

			dbg0 = readl(vio_dbg0_reg);
			vio_info->vio_dbg1 = readl(vio_dbg1_reg);

			vio_info->master_id = (dbg0 & vio_dbgs->vio_dbg_mstid)
				>> vio_dbgs->vio_dbg_mstid_start_bit;
			vio_info->domain_id = (dbg0 & vio_dbgs->vio_dbg_dmnid)
				>> vio_dbgs->vio_dbg_dmnid_start_bit;
			write_vio = (dbg0 & vio_dbgs->vio_dbg_w_vio)
				>> vio_dbgs->vio_dbg_w_vio_start_bit;
			read_vio = (dbg0 & vio_dbgs->vio_dbg_r_vio)
				>> vio_dbgs->vio_dbg_r_vio_start_bit;
			vio_addr_high = (dbg0 & vio_dbgs->vio_addr_high)
				>> vio_dbgs->vio_addr_high_start_bit;

			/* violation information */
			pr_info(PFX "%s%s%s%s%x %s%x, %s%x, %s%x\n",
					"Violation(",
					read_vio == 1?" R":"",
					write_vio == 1?" W ) - ":" ) - ",
					"Vio Addr:0x", vio_info->vio_dbg1,
					"High:0x", vio_addr_high,
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
 * devapc_violation_irq - the devapc Interrupt Service Routine (ISR) will dump
 *			  violation information including which master violates
 *			  access slave.
 */
static irqreturn_t devapc_violation_irq(int irq_number, void *dev_id)
{
	const struct mtk_device_info *device_info;
	struct mtk_devapc_dbg_status *dbg_stat;
	struct mtk_devapc_vio_info *vio_info;
	const char *vio_master;
	unsigned long flags;
	int i, device_count;
	uint8_t perm;
	int32_t ret;

	spin_lock_irqsave(&devapc_lock, flags);
	device_info = mtk_devapc_ctx->soc->device_info;
	vio_info = mtk_devapc_ctx->soc->vio_info;

	print_vio_mask_sta();
	mtk_devapc_dump_vio_dbg();

	device_count = mtk_devapc_ctx->soc->ndevices;
	dbg_stat = mtk_devapc_ctx->soc->dbg_stat;

	for (i = 0; i < device_count; i++) {
		if (device_info[i].enable_vio_irq == true &&
			check_infra_vio_status(i) == VIOLATION_TRIGGERED) {

			mask_infra_module_irq(i, true);
			ret = clear_infra_vio_status(i);
			if (ret)
				pr_warn(PFX "Warning: %s, ret: 0x%x\n",
						"clear vio status failed",
						ret);

			perm = get_permission(i, vio_info->domain_id);
			vio_master = mtk_devapc_ctx->soc->master_get(
					vio_info->master_id,
					vio_info->vio_dbg1, i);

			if (vio_master == NULL)
				vio_master = "UNKNOWN";

			pr_info(PFX "%s %s %s %s (%s=%d)\n",
				"Violation Master:", vio_master,
				"Access Violation Slave:",
				device_info[i].device,
				"vio idx", i);

			pr_info(PFX "Permission: %s\n",
				perm_to_string(perm));

			if (dbg_stat->enable_WARN) {
				WARN(1, "Violation master: %s access %s\n",
						vio_master,
						device_info[i].device);
			}

			mask_infra_module_irq(i, false);
			break;
		}
	}

	spin_unlock_irqrestore(&devapc_lock, flags);

	return IRQ_HANDLED;
}

/*
 * devapc_ut - There are two UT commands to support
 * 1. test permission denied violation
 * 2. test sramrom decode error violation
 */
static void devapc_ut(uint32_t cmd)
{
	void __iomem *devapc_ao_base = mtk_devapc_ctx->devapc_ao_base;
	void __iomem *sramrom_base = mtk_devapc_ctx->sramrom_base;

	pr_info(PFX "%s, test violation..., cmd = %d\n", __func__, cmd);

	if (cmd == DEVAPC_UT_DAPC_VIO) {
		if (unlikely(devapc_ao_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		pr_info(PFX "%s, devapc_ao_infra_base = 0x%x\n",
			__func__, readl(devapc_ao_base));
		pr_info(PFX "test done, it should generate violation!\n");

	} else if (cmd == DEVAPC_UT_SRAM_VIO) {
		if (unlikely(sramrom_base == NULL)) {
			pr_err(PFX "%s:%d NULL pointer\n", __func__, __LINE__);
			return;
		}

		pr_info(PFX "%s, sramrom_base = 0x%x\n",
			__func__, readl(sramrom_base + RANDOM_OFFSET));
		pr_info(PFX "test done, it should generate violation!\n");

	} else {
		pr_info(PFX "%s, cmd(0x%x) not supported\n", __func__, cmd);
	}
}

/*
 * mtk_devapc_dbg_read - dump status of struct mtk_devapc_dbg_status.
 * Currently, we have four debug status:
 * 1. enable_ut: enable/disable devapc ut commands
 * 2. enable_WARN: enable/disable trigger kernel warning while violation
 *    is triggered.
 * 3. enable_dapc: enable/disable dump access permission control
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
	devapc_log(p, msg_buf, "\tenable_WARN = %d\n", dbg_stat->enable_WARN);
	devapc_log(p, msg_buf, "\tenable_dapc = %d\n", dbg_stat->enable_dapc);
	devapc_log(p, msg_buf, "\n");

	len = p - msg_buf;

	return simple_read_from_buffer(buffer, count, ppos, msg_buf, len);
}

/*
 * mtk_devapc_dbg_write - control status of struct mtk_devapc_dbg_status.
 * There are five nodes we can control:
 * 1. enable_ut
 * 2. enable_WARN
 * 3. enable_dapc
 * 4. devapc_ut
 * 5. dump_apc
 */
ssize_t mtk_devapc_dbg_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	long slave_type = 0, domain = 0, index = 0;
	struct mtk_devapc_dbg_status *dbg_stat;
	int err = 0, len = 0, apc_set_idx;
	struct arm_smccc_res res;
	unsigned long param = 0;
	char *parm_str = NULL;
	char *cmd_str = NULL;
	char input[32] = {0};
	char *pinput = NULL;
	uint32_t ret;

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
		pr_info(PFX "debapc_dbg_stat->enable_ut = %s\n",
			dbg_stat->enable_ut ? "enable" : "disable");
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

		if (slave_type >= E_DAPC_OTHERS_SLAVE || err) {
			pr_err(PFX "Wrong slave type(%lu), err(0x%x)\n",
				slave_type, err);
			return -EFAULT;
		}

		parm_str = strsep(&pinput, " ");
		if (parm_str != NULL)
			err = kstrtol(parm_str, 10, &domain);
		else
			domain = E_DOMAIN_OTHERS;

		if (domain >= E_DOMAIN_OTHERS || err) {
			pr_err(PFX "Wrong domain type(%lu), err(0x%x)\n",
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
			pr_err(PFX "Wrong index(%lu), err(0x%x)\n",
				index, err);
			return -EFAULT;
		}

		pr_info(PFX "slave_type = %lu\n", slave_type);
		pr_info(PFX "domain id = %lu\n", domain);
		pr_info(PFX "slave config_idx = %lu\n", index);

		arm_smccc_smc(MTK_SIP_KERNEL_DAPC_DUMP,
				slave_type, domain, index, 0, 0, 0, 0, &res);
		ret = res.a0;

		if (ret == DEAD || ret == SIP_SVC_E_NOT_SUPPORTED) {
			pr_err(PFX "%s, SMC call failed, ret: 0x%x\n",
					__func__, ret);
			return -EINVAL;
		}

		apc_set_idx = index % MOD_NO_IN_1_DEVAPC;
		ret = (ret & (0x3 << (apc_set_idx * 2))) >> (apc_set_idx * 2);

		pr_info(PFX "The permission is %s\n",
			perm_to_string((ret & 0x3)));
		return count;
	} else {
		return -EINVAL;
	}

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

	mtk_devapc_ctx->devapc_pd_base = devm_of_iomap(&pdev->dev, node,
			DT_DEVAPC_PD_IDX, NULL);
	if (unlikely(mtk_devapc_ctx->devapc_pd_base == NULL)) {
		pr_err(PFX "Failed to parse devapc_pd_base.\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->devapc_ao_base = devm_of_iomap(&pdev->dev, node,
			DT_DEVAPC_AO_IDX, NULL);
	if (unlikely(mtk_devapc_ctx->devapc_ao_base == NULL)) {
		pr_err(PFX "Failed to parse devapc_ao_base.\n");
		return -EINVAL;
	}

	mtk_devapc_ctx->sramrom_base = devm_of_iomap(&pdev->dev, node,
			DT_SRAMROM_IDX, NULL);
	if (unlikely(mtk_devapc_ctx->sramrom_base == NULL))
		pr_info(PFX "Failed to parse sramrom_base.\n");

	mtk_devapc_ctx->devapc_irq = irq_of_parse_and_map(node, 0);
	if (!mtk_devapc_ctx->devapc_irq) {
		pr_err(PFX "Failed to parse and map the interrupt.\n");
		return -EINVAL;
	}

	pr_debug(PFX "devapc_pd_base: %p, IRQ: %d\n",
			mtk_devapc_ctx->devapc_pd_base,
			mtk_devapc_ctx->devapc_irq);

	ret = devm_request_irq(&pdev->dev, mtk_devapc_ctx->devapc_irq,
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
