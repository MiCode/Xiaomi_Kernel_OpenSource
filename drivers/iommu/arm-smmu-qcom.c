// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/qcom_scm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "arm-smmu.h"
#include "arm-smmu-debug.h"
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define IMPL_DEF4_MICRO_MMU_CTRL	0
#define IMPL_DEF4_CLK_ON_STATUS		0x50
#define IMPL_DEF4_CLK_ON_CLIENT_STATUS	0x54
#define MICRO_MMU_CTRL_LOCAL_HALT_REQ	BIT(2)
#define MICRO_MMU_CTRL_IDLE		BIT(3)

/* Definitions for implementation-defined registers */
#define ACTLR_QCOM_OSH			BIT(28)
#define ACTLR_QCOM_ISH			BIT(29)
#define ACTLR_QCOM_NSH			BIT(30)

struct arm_smmu_impl_def_reg {
	u32 offset;
	u32 value;
};

struct qsmmuv2_archdata {
	spinlock_t			atos_lock;
	struct arm_smmu_impl_def_reg	*impl_def_attach_registers;
	unsigned int			num_impl_def_attach_registers;
	struct arm_smmu_device		smmu;
};

#define to_qsmmuv2_archdata(smmu)				\
	container_of(smmu, struct qsmmuv2_archdata, smmu)

static int qsmmuv2_wait_for_halt(struct arm_smmu_device *smmu)
{
	void __iomem *reg = arm_smmu_page(smmu, ARM_SMMU_IMPL_DEF4);
	struct device *dev = smmu->dev;
	u32 tmp;

	if (readl_poll_timeout_atomic(reg + IMPL_DEF4_MICRO_MMU_CTRL, tmp,
				      (tmp & MICRO_MMU_CTRL_IDLE), 0, 30000)) {
		dev_err(dev, "Couldn't halt SMMU!\n");
		return -EBUSY;
	}

	return 0;
}

static int __qsmmuv2_halt(struct arm_smmu_device *smmu, bool wait)
{
	u32 val;

	val = arm_smmu_readl(smmu, ARM_SMMU_IMPL_DEF4,
			     IMPL_DEF4_MICRO_MMU_CTRL);
	val |= MICRO_MMU_CTRL_LOCAL_HALT_REQ;

	arm_smmu_writel(smmu, ARM_SMMU_IMPL_DEF4, IMPL_DEF4_MICRO_MMU_CTRL,
			val);

	return wait ? qsmmuv2_wait_for_halt(smmu) : 0;
}

static int qsmmuv2_halt(struct arm_smmu_device *smmu)
{
	return __qsmmuv2_halt(smmu, true);
}

static int qsmmuv2_halt_nowait(struct arm_smmu_device *smmu)
{
	return __qsmmuv2_halt(smmu, false);
}

static void qsmmuv2_resume(struct arm_smmu_device *smmu)
{
	u32 val;

	val = arm_smmu_readl(smmu, ARM_SMMU_IMPL_DEF4,
			     IMPL_DEF4_MICRO_MMU_CTRL);
	val &= ~MICRO_MMU_CTRL_LOCAL_HALT_REQ;

	arm_smmu_writel(smmu, ARM_SMMU_IMPL_DEF4, IMPL_DEF4_MICRO_MMU_CTRL,
			val);
}



static phys_addr_t __qsmmuv2_iova_to_phys_hard(
					struct arm_smmu_domain *smmu_domain,
					dma_addr_t iova)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct device *dev = smmu->dev;
	int idx = cfg->cbndx;
	void __iomem *reg;
	u32 tmp;
	u64 phys;
	unsigned long va;


	/* ATS1 registers can only be written atomically */
	va = iova & ~0xfffUL;
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
		arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_ATS1PR, va);
	else
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ATS1PR, va);

	reg = arm_smmu_page(smmu, ARM_SMMU_CB(smmu, idx));
	if (readl_poll_timeout_atomic(reg + ARM_SMMU_CB_ATSR, tmp,
				      !(tmp & ATSR_ACTIVE), 5, 50)) {
		dev_err(dev, "iova to phys timed out on %pad.\n", &iova);
		phys = 0;
		return phys;
	}

	phys = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_PAR);
	if (phys & CB_PAR_F) {
		dev_err(dev, "translation fault!\n");
		dev_err(dev, "PAR = 0x%llx\n", phys);
		phys = 0;
	} else {
		phys = (phys & (PHYS_MASK & ~0xfffULL)) | (iova & 0xfff);
	}

	return phys;
}

static phys_addr_t qsmmuv2_iova_to_phys_hard(
					struct arm_smmu_domain *smmu_domain,
					dma_addr_t iova,
					unsigned long trans_flags)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct qsmmuv2_archdata *data = to_qsmmuv2_archdata(smmu);
	int idx = smmu_domain->cfg.cbndx;
	phys_addr_t phys = 0;
	unsigned long flags;
	u32 sctlr, sctlr_orig, fsr;

	spin_lock_irqsave(&data->atos_lock, flags);

	qsmmuv2_halt_nowait(smmu);

	/* disable stall mode momentarily */
	sctlr_orig = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~(SCTLR_CFCFG);
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr);

	/* clear FSR to allow ATOS to log any faults */
	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if (fsr & FSR_FAULT) {
		/* Clear pending interrupts */
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation.
		 */
		wmb();

		/*
		 * TBU halt takes care of resuming any stalled transcation.
		 * Kept it here for completeness sake.
		 */
		if (fsr & FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  RESUME_TERMINATE);
	}

	qsmmuv2_wait_for_halt(smmu);

	phys = __qsmmuv2_iova_to_phys_hard(smmu_domain, iova);

	/* restore SCTLR */
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr_orig);

	qsmmuv2_resume(smmu);
	spin_unlock_irqrestore(&data->atos_lock, flags);

	return phys;
}

static void qsmmuv2_tlb_sync_timeout(struct arm_smmu_device *smmu)
{
	u32 clk_on, clk_on_client;

	dev_err_ratelimited(smmu->dev,
			    "TLB sync timed out -- SMMU may be deadlocked\n");
	clk_on = arm_smmu_readl(smmu, ARM_SMMU_IMPL_DEF4,
				IMPL_DEF4_CLK_ON_STATUS);
	clk_on_client = arm_smmu_readl(smmu, ARM_SMMU_IMPL_DEF4,
				       IMPL_DEF4_CLK_ON_CLIENT_STATUS);
	dev_err_ratelimited(smmu->dev,
			    "clk on 0x%x, clk on client 0x%x status\n",
			    clk_on, clk_on_client);

	BUG_ON(IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG));
}

static int qsmmuv2_device_reset(struct arm_smmu_device *smmu)
{
	struct qsmmuv2_archdata *data = to_qsmmuv2_archdata(smmu);
	struct arm_smmu_impl_def_reg *regs = data->impl_def_attach_registers;
	u32 i;

	/* Program implementation defined registers */
	qsmmuv2_halt(smmu);
	for (i = 0; i < data->num_impl_def_attach_registers; ++i)
		arm_smmu_gr0_write(smmu, regs[i].offset, regs[i].value);
	qsmmuv2_resume(smmu);

	return 0;
}

static void qsmmuv2_init_cb(struct arm_smmu_domain *smmu_domain,
				struct device *dev)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	const struct iommu_flush_ops *tlb;
	u32 val;

	tlb = smmu_domain->pgtbl_info[0].pgtbl_cfg.tlb;

	val = ACTLR_QCOM_ISH | ACTLR_QCOM_OSH | ACTLR_QCOM_NSH;

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ACTLR, val);

	/*
	 * Flush the context bank after modifying ACTLR to ensure there
	 * are no cache entries with stale state
	 */
	tlb->tlb_flush_all(smmu_domain);
}

static int arm_smmu_parse_impl_def_registers(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	struct qsmmuv2_archdata *data = to_qsmmuv2_archdata(smmu);
	int i, ntuples, ret;
	u32 *tuples;
	struct arm_smmu_impl_def_reg *regs, *regit;

	if (!of_find_property(dev->of_node, "attach-impl-defs", &ntuples))
		return 0;

	ntuples /= sizeof(u32);
	if (ntuples % 2) {
		dev_err(dev,
			"Invalid number of attach-impl-defs registers: %d\n",
			ntuples);
		return -EINVAL;
	}

	regs = devm_kzalloc(dev, sizeof(*data->impl_def_attach_registers) *
			    ntuples, GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	tuples = kzalloc(sizeof(u32) * ntuples * 2, GFP_KERNEL);
	if (!tuples)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "attach-impl-defs",
					tuples, ntuples);
	if (ret) {
		kfree(tuples);
		return ret;
	}

	for (i = 0, regit = regs; i < ntuples; i += 2, ++regit) {
		regit->offset = tuples[i];
		regit->value = tuples[i + 1];
	}

	kfree(tuples);

	data->impl_def_attach_registers = regs;
	data->num_impl_def_attach_registers = ntuples / 2;

	return 0;
}

static const struct arm_smmu_impl qsmmuv2_impl = {
	.init_context_bank = qsmmuv2_init_cb,
	.iova_to_phys_hard = qsmmuv2_iova_to_phys_hard,
	.tlb_sync_timeout = qsmmuv2_tlb_sync_timeout,
	.reset = qsmmuv2_device_reset,
};

struct qcom_smmu {
	struct arm_smmu_device smmu;
};

static int qcom_sdm845_smmu500_cfg_probe(struct arm_smmu_device *smmu)
{
	u32 s2cr;
	u32 smr;
	int i;

	for (i = 0; i < smmu->num_mapping_groups; i++) {
		smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(i));
		s2cr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_S2CR(i));

		smmu->smrs[i].mask = FIELD_GET(SMR_MASK, smr);
		smmu->smrs[i].id = FIELD_GET(SMR_ID, smr);
		if (smmu->features & ARM_SMMU_FEAT_EXIDS)
			smmu->smrs[i].valid = FIELD_GET(S2CR_EXIDVALID, s2cr);
		else
			smmu->smrs[i].valid = FIELD_GET(SMR_VALID, smr);

		smmu->s2crs[i].group = NULL;
		smmu->s2crs[i].count = 0;
		smmu->s2crs[i].type = FIELD_GET(S2CR_TYPE, s2cr);
		smmu->s2crs[i].privcfg = FIELD_GET(S2CR_PRIVCFG, s2cr);
		smmu->s2crs[i].cbndx = FIELD_GET(S2CR_CBNDX, s2cr);

		if (!smmu->smrs[i].valid)
			continue;

		smmu->s2crs[i].pinned = true;
		bitmap_set(smmu->context_map, smmu->s2crs[i].cbndx, 1);
	}

	return 0;
}

static int qcom_sdm845_smmu500_reset(struct arm_smmu_device *smmu)
{
	int ret;

	arm_mmu500_reset(smmu);

	/*
	 * To address performance degradation in non-real time clients,
	 * such as USB and UFS, turn off wait-for-safe on sdm845 based boards,
	 * such as MTP and db845, whose firmwares implement secure monitor
	 * call handlers to turn on/off the wait-for-safe logic.
	 */
	ret = qcom_scm_qsmmu500_wait_safe_toggle(0);
	if (ret)
		dev_warn(smmu->dev, "Failed to turn off SAFE logic\n");

	return ret;
}

static const struct arm_smmu_impl qcom_smmu_impl = {
	.cfg_probe = qcom_sdm845_smmu500_cfg_probe,
	.reset = qcom_sdm845_smmu500_reset,
};

#define TCU_HW_VERSION_HLOS1		(0x18)

#define DEBUG_SID_HALT_REG		0x0
#define DEBUG_SID_HALT_REQ		BIT(16)
#define DEBUG_SID_HALT_SID		GENMASK(9, 0)

#define DEBUG_VA_ADDR_REG		0x8

#define DEBUG_TXN_TRIGG_REG		0x18
#define DEBUG_TXN_AXPROT		GENMASK(8, 6)
#define DEBUG_TXN_AXCACHE		GENMASK(5, 2)
#define DEBUG_TXN_WRITE			BIT(1)
#define DEBUG_TXN_AXPROT_PRIV		0x1
#define DEBUG_TXN_AXPROT_UNPRIV		0x0
#define DEBUG_TXN_AXPROT_NSEC		0x2
#define DEBUG_TXN_AXPROT_SEC		0x0
#define DEBUG_TXN_AXPROT_INST		0x4
#define DEBUG_TXN_AXPROT_DATA		0x0
#define DEBUG_TXN_READ			(0x0 << 1)
#define DEBUG_TXN_TRIGGER		BIT(0)

#define DEBUG_SR_HALT_ACK_REG		0x20
#define DEBUG_SR_HALT_ACK_VAL		(0x1 << 1)
#define DEBUG_SR_ECATS_RUNNING_VAL	(0x1 << 0)

#define DEBUG_PAR_REG			0x28
#define DEBUG_PAR_PA			GENMASK_ULL(47, 12)
#define DEBUG_PAR_FAULT_VAL		BIT(0)

#define DEBUG_AXUSER_REG		0x30
#define DEBUG_AXUSER_CDMID		GENMASK_ULL(43, 36)
#define DEBUG_AXUSER_CDMID_VAL          255

#define TBU_DBG_TIMEOUT_US		100


struct actlr_setting {
	struct arm_smmu_smr smr;
	u32 actlr;
};

struct qsmmuv500_archdata {
	struct list_head		tbus;
	void __iomem			*tcu_base;
	u32				version;
	struct actlr_setting		*actlrs;
	u32				actlr_tbl_size;
	struct work_struct		outstanding_tnx_work;
	spinlock_t			atos_lock;
	struct arm_smmu_device		smmu;
};
#define to_qsmmuv500_archdata(smmu)				\
	container_of(smmu, struct qsmmuv500_archdata, smmu)

struct qsmmuv500_group_iommudata {
	bool has_actlr;
	u32 actlr;
};
#define to_qsmmuv500_group_iommudata(group)				\
	((struct qsmmuv500_group_iommudata *)				\
		(iommu_group_get_iommudata(group)))

static struct qsmmuv500_tbu_device *qsmmuv500_find_tbu(
	struct arm_smmu_device *smmu, u32 sid);

/*
 * Provides mutually exclusive access to the registers used by the
 * outstanding transaction snapshot feature and the transaction
 * snapshot capture feature.
 */
static DEFINE_MUTEX(capture_reg_lock);
static DEFINE_SPINLOCK(testbus_lock);

#ifdef CONFIG_IOMMU_DEBUGFS
static struct dentry *debugfs_capturebus_dir;
#endif

#ifdef CONFIG_ARM_SMMU_TESTBUS_DEBUGFS
static struct dentry *debugfs_testbus_dir;

static ssize_t arm_smmu_debug_testbus_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset,
		enum testbus_sel tbu, enum testbus_ops ops)

{
	char buf[100];
	ssize_t retval;
	size_t buflen;
	int buf_len = sizeof(buf);

	if (*offset)
		return 0;

	memset(buf, 0, buf_len);

	if (tbu == SEL_TBU) {
		struct qsmmuv500_tbu_device *tbu = file->private_data;
		void __iomem *tbu_base = tbu->base;
		long val;

		arm_smmu_power_on(tbu->pwr);
		if (ops == TESTBUS_SELECT)
			val = arm_smmu_debug_tbu_testbus_select(tbu_base,
							READ, 0);
		else
			val = arm_smmu_debug_tbu_testbus_output(tbu_base);
		arm_smmu_power_off(tbu->smmu, tbu->pwr);

		scnprintf(buf, buf_len, "0x%0x\n", val);
	} else {

		struct arm_smmu_device *smmu = file->private_data;
		struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
		phys_addr_t phys_addr = smmu->phys_addr;
		void __iomem *tcu_base = data->tcu_base;

		arm_smmu_power_on(smmu->pwr);

		if (ops == TESTBUS_SELECT) {
			scnprintf(buf, buf_len, "TCU clk testbus sel: 0x%0x\n",
				  arm_smmu_debug_tcu_testbus_select(phys_addr,
					tcu_base, CLK_TESTBUS, READ, 0));
			scnprintf(buf + strlen(buf), buf_len - strlen(buf),
				  "TCU testbus sel : 0x%0x\n",
				  arm_smmu_debug_tcu_testbus_select(phys_addr,
					 tcu_base, PTW_AND_CACHE_TESTBUS,
					 READ, 0));
		} else {
			scnprintf(buf, buf_len, "0x%0x\n",
				  arm_smmu_debug_tcu_testbus_output(phys_addr));
		}

		arm_smmu_power_off(smmu, smmu->pwr);
	}
	buflen = min(count, strlen(buf));
	if (copy_to_user(ubuf, buf, buflen)) {
		pr_err_ratelimited("Couldn't copy_to_user\n");
		retval = -EFAULT;
	} else {
		*offset = 1;
		retval = buflen;
	}

	return retval;
}

static ssize_t arm_smmu_debug_tcu_testbus_sel_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *offset)
{
	struct arm_smmu_device *smmu = file->private_data;
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
	void __iomem *tcu_base = data->tcu_base;
	phys_addr_t phys_addr = smmu->phys_addr;
	char *comma;
	char buf[100];
	u64 sel, val;

	if (count >= 100) {
		pr_err_ratelimited("Value too large\n");
		return -EINVAL;
	}

	memset(buf, 0, 100);

	if (copy_from_user(buf, ubuf, count)) {
		pr_err_ratelimited("Couldn't copy from user\n");
		return -EFAULT;
	}

	comma = strnchr(buf, count, ',');
	if (!comma)
		goto invalid_format;

	/* split up the words */
	*comma = '\0';

	if (kstrtou64(buf, 0, &sel))
		goto invalid_format;

	if (sel != 1 && sel != 2)
		goto invalid_format;

	if (kstrtou64(comma + 1, 0, &val))
		goto invalid_format;

	arm_smmu_power_on(smmu->pwr);

	if (sel == 1)
		arm_smmu_debug_tcu_testbus_select(phys_addr,
				tcu_base, CLK_TESTBUS, WRITE, val);
	else if (sel == 2)
		arm_smmu_debug_tcu_testbus_select(phys_addr,
				tcu_base, PTW_AND_CACHE_TESTBUS, WRITE, val);

	arm_smmu_power_off(smmu, smmu->pwr);

	return count;

invalid_format:
	pr_err_ratelimited("Invalid format. Expected: <1, testbus select> for tcu CLK testbus (or) <2, testbus select> for tcu PTW/CACHE testbuses\n");
	return -EINVAL;
}

static ssize_t arm_smmu_debug_tcu_testbus_sel_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	return arm_smmu_debug_testbus_read(file, ubuf,
			count, offset, SEL_TCU, TESTBUS_SELECT);
}

static const struct file_operations arm_smmu_debug_tcu_testbus_sel_fops = {
	.open	= simple_open,
	.write	= arm_smmu_debug_tcu_testbus_sel_write,
	.read	= arm_smmu_debug_tcu_testbus_sel_read,
};

static ssize_t arm_smmu_debug_tcu_testbus_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	return arm_smmu_debug_testbus_read(file, ubuf,
			count, offset, SEL_TCU, TESTBUS_OUTPUT);
}

static const struct file_operations arm_smmu_debug_tcu_testbus_fops = {
	.open	= simple_open,
	.read	= arm_smmu_debug_tcu_testbus_read,
};

static int qsmmuv500_tcu_testbus_init(struct arm_smmu_device *smmu)
{
	struct dentry *testbus_dir;

	if (!iommu_debugfs_dir)
		return 0;

	if (!debugfs_testbus_dir) {
		debugfs_testbus_dir = debugfs_create_dir("testbus",
						       iommu_debugfs_dir);
		if (IS_ERR(debugfs_testbus_dir)) {
			pr_err_ratelimited("Couldn't create iommu/testbus debugfs directory\n");
			return -ENODEV;
		}
	}

	testbus_dir = debugfs_create_dir(dev_name(smmu->dev),
				debugfs_testbus_dir);

	if (IS_ERR(testbus_dir)) {
		pr_err_ratelimited("Couldn't create iommu/testbus/%s debugfs directory\n",
		       dev_name(smmu->dev));
		goto err;
	}

	if (IS_ERR(debugfs_create_file("tcu_testbus_sel", 0400,
					testbus_dir, smmu,
					&arm_smmu_debug_tcu_testbus_sel_fops))) {
		pr_err_ratelimited("Couldn't create iommu/testbus/%s/tcu_testbus_sel debugfs file\n",
		       dev_name(smmu->dev));
		goto err_rmdir;
	}

	if (IS_ERR(debugfs_create_file("tcu_testbus_output", 0400,
					testbus_dir, smmu,
					&arm_smmu_debug_tcu_testbus_fops))) {
		pr_err_ratelimited("Couldn't create iommu/testbus/%s/tcu_testbus_output debugfs file\n",
				   dev_name(smmu->dev));
		goto err_rmdir;
	}

	return 0;
err_rmdir:
	debugfs_remove_recursive(testbus_dir);
err:
	return 0;
}

static ssize_t arm_smmu_debug_tbu_testbus_sel_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *offset)
{
	struct qsmmuv500_tbu_device *tbu = file->private_data;
	void __iomem *tbu_base = tbu->base;
	u64 val;

	if (kstrtoull_from_user(ubuf, count, 0, &val)) {
		pr_err_ratelimited("Invalid format for tbu testbus select\n");
		return -EINVAL;
	}

	arm_smmu_power_on(tbu->pwr);
	arm_smmu_debug_tbu_testbus_select(tbu_base, WRITE, val);
	arm_smmu_power_off(tbu->smmu, tbu->pwr);

	return count;
}

static ssize_t arm_smmu_debug_tbu_testbus_sel_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	return arm_smmu_debug_testbus_read(file, ubuf,
			count, offset, SEL_TBU, TESTBUS_SELECT);
}

static const struct file_operations arm_smmu_debug_tbu_testbus_sel_fops = {
	.open	= simple_open,
	.write	= arm_smmu_debug_tbu_testbus_sel_write,
	.read	= arm_smmu_debug_tbu_testbus_sel_read,
};

static ssize_t arm_smmu_debug_tbu_testbus_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	return arm_smmu_debug_testbus_read(file, ubuf,
			count, offset, SEL_TBU, TESTBUS_OUTPUT);
}

static const struct file_operations arm_smmu_debug_tbu_testbus_fops = {
	.open	= simple_open,
	.read	= arm_smmu_debug_tbu_testbus_read,
};

static int qsmmuv500_tbu_testbus_init(struct qsmmuv500_tbu_device *tbu)
{
	struct dentry *testbus_dir;

	if (!iommu_debugfs_dir)
		return 0;

	if (!debugfs_testbus_dir) {
		debugfs_testbus_dir = debugfs_create_dir("testbus",
						       iommu_debugfs_dir);
		if (IS_ERR(debugfs_testbus_dir)) {
			pr_err_ratelimited("Couldn't create iommu/testbus debugfs directory\n");
			return -ENODEV;
		}
	}

	testbus_dir = debugfs_create_dir(dev_name(tbu->dev),
				debugfs_testbus_dir);

	if (IS_ERR(testbus_dir)) {
		pr_err_ratelimited("Couldn't create iommu/testbus/%s debugfs directory\n",
		       dev_name(tbu->dev));
		goto err;
	}

	if (IS_ERR(debugfs_create_file("tbu_testbus_sel", 0400,
					testbus_dir, tbu,
					&arm_smmu_debug_tbu_testbus_sel_fops)))	{
		pr_err_ratelimited("Couldn't create iommu/testbus/%s/tbu_testbus_sel debugfs file\n",
		       dev_name(tbu->dev));
		goto err_rmdir;
	}

	if (IS_ERR(debugfs_create_file("tbu_testbus_output", 0400,
					testbus_dir, tbu,
					&arm_smmu_debug_tbu_testbus_fops))) {
		pr_err_ratelimited("Couldn't create iommu/testbus/%s/tbu_testbus_output debugfs file\n",
		       dev_name(tbu->dev));
		goto err_rmdir;
	}

	return 0;
err_rmdir:
	debugfs_remove_recursive(testbus_dir);
err:
	return 0;
}
#else
static int qsmmuv500_tcu_testbus_init(struct arm_smmu_device *smmu)
{
	return 0;
}

static int qsmmuv500_tbu_testbus_init(struct qsmmuv500_tbu_device *tbu)
{
	return 0;
}
#endif

static void arm_smmu_testbus_dump(struct arm_smmu_device *smmu, u16 sid)
{
	if (smmu->model == QCOM_SMMUV500 &&
	    IS_ENABLED(CONFIG_ARM_SMMU_TESTBUS_DUMP)) {
		struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
		struct qsmmuv500_tbu_device *tbu;

		tbu = qsmmuv500_find_tbu(smmu, sid);
		spin_lock(&testbus_lock);
		if (tbu)
			arm_smmu_debug_dump_tbu_testbus(tbu->dev,
							tbu->base,
							tbu_testbus_sel);
		else
			arm_smmu_debug_dump_tcu_testbus(smmu->dev,
							smmu->phys_addr,
							data->tcu_base,
							tcu_testbus_sel);
		spin_unlock(&testbus_lock);
	}
}

static void qsmmuv500_log_outstanding_transactions(struct work_struct *work)
{
	struct qsmmuv500_tbu_device *tbu = NULL;
	u64 outstanding_tnxs;
	u64 tcr_cntl_val, res;
	struct qsmmuv500_archdata *data = container_of(work,
						struct qsmmuv500_archdata,
						outstanding_tnx_work);
	struct arm_smmu_device *smmu = &data->smmu;
	void __iomem *base;

	if (!mutex_trylock(&capture_reg_lock)) {
		dev_warn_ratelimited(smmu->dev,
			"Tnx snapshot regs in use, not dumping OT tnxs.\n");
		goto bug;
	}

	if (arm_smmu_power_on(smmu->pwr)) {
		dev_err_ratelimited(smmu->dev,
				    "%s: Failed to power on SMMU.\n",
				    __func__);
		goto unlock;
	}

	list_for_each_entry(tbu, &data->tbus, list) {
		if (arm_smmu_power_on(tbu->pwr)) {
			dev_err_ratelimited(tbu->dev,
					    "%s: Failed to power on TBU.\n",
					    __func__);
			continue;
		}
		base = tbu->base;

		tcr_cntl_val = readq_relaxed(base + TNX_TCR_CNTL);

		/* Write 1 into MATCH_MASK_UPD of TNX_TCR_CNTL */
		writeq_relaxed(tcr_cntl_val | TNX_TCR_CNTL_MATCH_MASK_UPD,
			       base + TNX_TCR_CNTL);

		/*
		 * Simultaneously write 0 into MATCH_MASK_UPD, 0 into
		 * ALWAYS_CAPTURE, 0 into MATCH_MASK_VALID, and 1 into
		 * TBU_OT_CAPTURE_EN of TNX_TCR_CNTL
		 */
		tcr_cntl_val &= ~(TNX_TCR_CNTL_MATCH_MASK_UPD |
				  TNX_TCR_CNTL_ALWAYS_CAPTURE |
				  TNX_TCR_CNTL_MATCH_MASK_VALID);
		writeq_relaxed(tcr_cntl_val | TNX_TCR_CNTL_TBU_OT_CAPTURE_EN,
			       base + TNX_TCR_CNTL);

		/* Poll for CAPTURE1_VALID to become 1 on TNX_TCR_CNTL_2 */
		if (readq_poll_timeout_atomic(base + TNX_TCR_CNTL_2, res,
					      res & TNX_TCR_CNTL_2_CAP1_VALID,
					      0, TBU_DBG_TIMEOUT_US)) {
			dev_err_ratelimited(tbu->dev,
					    "Timeout on TNX snapshot poll\n");
			goto poll_timeout;
		}

		/* Read Register CAPTURE1_SNAPSHOT_1 */
		outstanding_tnxs = readq_relaxed(base + CAPTURE1_SNAPSHOT_1);
		dev_err_ratelimited(tbu->dev,
				    "Outstanding Transaction Bitmap: 0x%llx\n",
				    outstanding_tnxs);
poll_timeout:
		/* Write TBU_OT_CAPTURE_EN to 0 of TNX_TCR_CNTL */
		writeq_relaxed(tcr_cntl_val & ~TNX_TCR_CNTL_TBU_OT_CAPTURE_EN,
			       tbu->base + TNX_TCR_CNTL);

		arm_smmu_power_off(smmu, tbu->pwr);
	}

	arm_smmu_power_off(smmu, smmu->pwr);
unlock:
	mutex_unlock(&capture_reg_lock);
bug:
	BUG_ON(IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG));
}

static ssize_t arm_smmu_debug_capturebus_snapshot_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	struct qsmmuv500_tbu_device *tbu = file->private_data;
	struct arm_smmu_device *smmu = tbu->smmu;
	void __iomem *tbu_base = tbu->base;
	u64 snapshot[NO_OF_CAPTURE_POINTS][REGS_PER_CAPTURE_POINT];
	char buf[400];
	ssize_t retval;
	size_t buflen;
	int buf_len = sizeof(buf);
	int i, j;

	if (*offset)
		return 0;

	memset(buf, 0, buf_len);

	if (arm_smmu_power_on(smmu->pwr))
		return -EINVAL;

	if (arm_smmu_power_on(tbu->pwr)) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return -EINVAL;
	}

	if (!mutex_trylock(&capture_reg_lock)) {
		dev_warn_ratelimited(smmu->dev,
			"capture bus regs in use, not dumping it.\n");
		return -EBUSY;
	}

	arm_smmu_debug_get_capture_snapshot(tbu_base, snapshot);

	mutex_unlock(&capture_reg_lock);
	arm_smmu_power_off(tbu->smmu, tbu->pwr);
	arm_smmu_power_off(smmu, smmu->pwr);

	for (i = 0; i < NO_OF_CAPTURE_POINTS ; ++i) {
		for (j = 0; j < REGS_PER_CAPTURE_POINT; ++j) {
			scnprintf(buf + strlen(buf), buf_len - strlen(buf),
				 "Capture_%d_Snapshot_%d : 0x%0llx\n",
				  i+1, j+1, snapshot[i][j]);
		}
	}

	buflen = min(count, strlen(buf));
	if (copy_to_user(ubuf, buf, buflen)) {
		dev_err_ratelimited(smmu->dev, "Couldn't copy_to_user\n");
		retval = -EFAULT;
	} else {
		*offset = 1;
		retval = buflen;
	}

	return retval;
}

static const struct file_operations arm_smmu_debug_capturebus_snapshot_fops = {
	.open	= simple_open,
	.read	= arm_smmu_debug_capturebus_snapshot_read,
};

static ssize_t arm_smmu_debug_capturebus_config_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *offset)
{
	struct qsmmuv500_tbu_device *tbu = file->private_data;
	struct arm_smmu_device *smmu = tbu->smmu;
	void __iomem *tbu_base = tbu->base;
	char *comma1, *comma2;
	char buf[100];
	u64 sel, mask, match, val;

	if (count >= sizeof(buf)) {
		dev_err_ratelimited(smmu->dev, "Input too large\n");
		goto invalid_format;
	}

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, ubuf, count)) {
		dev_err_ratelimited(smmu->dev, "Couldn't copy from user\n");
		return -EFAULT;
	}

	comma1 = strnchr(buf, count, ',');
	if (!comma1)
		goto invalid_format;

	*comma1  = '\0';

	if (kstrtou64(buf, 0, &sel))
		goto invalid_format;

	if (sel > 4) {
		goto invalid_format;
	} else if (sel == 4) {
		if (kstrtou64(comma1 + 1, 0, &val))
			goto invalid_format;
		goto program_capturebus;
	}

	comma2 = strnchr(comma1 + 1, count, ',');
	if (!comma2)
		goto invalid_format;

	/* split up the words */
	*comma2 = '\0';

	if (kstrtou64(comma1 + 1, 0, &mask))
		goto invalid_format;

	if (kstrtou64(comma2 + 1, 0, &match))
		goto invalid_format;

program_capturebus:
	if (arm_smmu_power_on(smmu->pwr))
		return -EINVAL;

	if (arm_smmu_power_on(tbu->pwr)) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return -EINVAL;
	}

	if (!mutex_trylock(&capture_reg_lock)) {
		dev_warn_ratelimited(smmu->dev,
			"capture bus regs in use, not configuring it.\n");
		return -EBUSY;
	}

	if (sel == 4)
		arm_smmu_debug_set_tnx_tcr_cntl(tbu_base, val);
	else
		arm_smmu_debug_set_mask_and_match(tbu_base, sel, mask, match);

	mutex_unlock(&capture_reg_lock);
	arm_smmu_power_off(tbu->smmu, tbu->pwr);
	arm_smmu_power_off(smmu, smmu->pwr);

	return count;

invalid_format:
	dev_err_ratelimited(smmu->dev, "Invalid format\n");
	dev_err_ratelimited(smmu->dev,
			    "Expected:<1/2/3,Mask,Match> <4,TNX_TCR_CNTL>\n");
	return -EINVAL;
}

static ssize_t arm_smmu_debug_capturebus_config_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *offset)
{
	struct qsmmuv500_tbu_device *tbu = file->private_data;
	struct arm_smmu_device *smmu = tbu->smmu;
	void __iomem *tbu_base = tbu->base;
	u64 val;
	u64 mask[NO_OF_MASK_AND_MATCH], match[NO_OF_MASK_AND_MATCH];
	char buf[400];
	ssize_t retval;
	size_t buflen;
	int buf_len = sizeof(buf);
	int i;

	if (*offset)
		return 0;

	memset(buf, 0, buf_len);

	if (arm_smmu_power_on(smmu->pwr))
		return -EINVAL;

	if (arm_smmu_power_on(tbu->pwr)) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return -EINVAL;
	}

	if (!mutex_trylock(&capture_reg_lock)) {
		dev_warn_ratelimited(smmu->dev,
			"capture bus regs in use, not configuring it.\n");
		return -EBUSY;
	}

	arm_smmu_debug_get_mask_and_match(tbu_base,
					mask, match);
	val = arm_smmu_debug_get_tnx_tcr_cntl(tbu_base);

	mutex_unlock(&capture_reg_lock);
	arm_smmu_power_off(tbu->smmu, tbu->pwr);
	arm_smmu_power_off(smmu, smmu->pwr);

	for (i = 0; i < NO_OF_MASK_AND_MATCH; ++i) {
		scnprintf(buf + strlen(buf), buf_len - strlen(buf),
				"Mask_%d : 0x%0llx\t", i+1, mask[i]);
		scnprintf(buf + strlen(buf), buf_len - strlen(buf),
				"Match_%d : 0x%0llx\n", i+1, match[i]);
	}
	scnprintf(buf + strlen(buf), buf_len - strlen(buf), "0x%0lx\n", val);

	buflen = min(count, strlen(buf));
	if (copy_to_user(ubuf, buf, buflen)) {
		dev_err_ratelimited(smmu->dev, "Couldn't copy_to_user\n");
		retval = -EFAULT;
	} else {
		*offset = 1;
		retval = buflen;
	}

	return retval;
}

static const struct file_operations arm_smmu_debug_capturebus_config_fops = {
	.open	= simple_open,
	.write	= arm_smmu_debug_capturebus_config_write,
	.read	= arm_smmu_debug_capturebus_config_read,
};

#ifdef CONFIG_IOMMU_DEBUGFS
static int qsmmuv500_capturebus_init(struct qsmmuv500_tbu_device *tbu)
{
	struct dentry *capturebus_dir;

	if (!iommu_debugfs_dir)
		return 0;

	if (!debugfs_capturebus_dir) {
		debugfs_capturebus_dir = debugfs_create_dir(
					 "capturebus", iommu_debugfs_dir);
		if (IS_ERR(debugfs_capturebus_dir)) {
			dev_err_ratelimited(tbu->dev, "Couldn't create iommu/capturebus debugfs directory\n");
			return PTR_ERR(debugfs_capturebus_dir);
		}
	}

	capturebus_dir = debugfs_create_dir(dev_name(tbu->dev),
				debugfs_capturebus_dir);
	if (IS_ERR(capturebus_dir)) {
		dev_err_ratelimited(tbu->dev, "Couldn't create iommu/capturebus/%s debugfs directory\n",
				dev_name(tbu->dev));
		goto err;
	}

	if (IS_ERR(debugfs_create_file("config", 0400, capturebus_dir, tbu,
			&arm_smmu_debug_capturebus_config_fops))) {
		dev_err_ratelimited(tbu->dev, "Couldn't create iommu/capturebus/%s/config debugfs file\n",
				dev_name(tbu->dev));
		goto err_rmdir;
	}

	if (IS_ERR(debugfs_create_file("snapshot", 0400, capturebus_dir, tbu,
			&arm_smmu_debug_capturebus_snapshot_fops))) {
		dev_err_ratelimited(tbu->dev, "Couldn't create iommu/capturebus/%s/snapshot debugfs file\n",
				dev_name(tbu->dev));
		goto err_rmdir;
	}
	return 0;
err_rmdir:
	debugfs_remove_recursive(capturebus_dir);
err:
	return -ENODEV;
}
#else
static int qsmmuv500_capturebus_init(struct qsmmuv500_tbu_device *tbu)
{
	return 0;
}
#endif

static irqreturn_t arm_smmu_debug_capture_bus_match(int irq, void *dev)
{
	struct qsmmuv500_tbu_device *tbu = dev;
	struct arm_smmu_device *smmu = tbu->smmu;
	void __iomem *tbu_base = tbu->base;
	u64 mask[NO_OF_MASK_AND_MATCH], match[NO_OF_MASK_AND_MATCH];
	u64 snapshot[NO_OF_CAPTURE_POINTS][REGS_PER_CAPTURE_POINT];
	int i, j;
	u64 val;

	if (arm_smmu_power_on(smmu->pwr))
		return IRQ_NONE;

	if (arm_smmu_power_on(tbu->pwr)) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return IRQ_NONE;
	}

	if (!mutex_trylock(&capture_reg_lock)) {
		dev_warn_ratelimited(smmu->dev,
			"capture bus regs in use, not dumping it.\n");
		return IRQ_NONE;
	}

	val = arm_smmu_debug_get_tnx_tcr_cntl(tbu_base);
	arm_smmu_debug_get_mask_and_match(tbu_base, mask, match);
	arm_smmu_debug_get_capture_snapshot(tbu_base, snapshot);
	arm_smmu_debug_clear_intr_and_validbits(tbu_base);

	mutex_unlock(&capture_reg_lock);
	arm_smmu_power_off(tbu->smmu, tbu->pwr);
	arm_smmu_power_off(smmu, smmu->pwr);

	dev_info(tbu->dev, "TNX_TCR_CNTL : 0x%0llx\n", val);

	for (i = 0; i < NO_OF_MASK_AND_MATCH; ++i) {
		dev_info(tbu->dev,
				"Mask_%d : 0x%0llx\n", i+1, mask[i]);
		dev_info(tbu->dev,
				"Match_%d : 0x%0llx\n", i+1, match[i]);
	}

	for (i = 0; i < NO_OF_CAPTURE_POINTS ; ++i) {
		for (j = 0; j < REGS_PER_CAPTURE_POINT; ++j) {
			dev_info(tbu->dev,
					"Capture_%d_Snapshot_%d : 0x%0llx\n",
					i+1, j+1, snapshot[i][j]);
		}
	}

	return IRQ_HANDLED;
}

static void qsmmuv500_tlb_sync_timeout(struct arm_smmu_device *smmu)
{
	u32 sync_inv_ack, tbu_pwr_status, sync_inv_progress;
	u32 tbu_inv_pending = 0, tbu_sync_pending = 0;
	u32 tbu_inv_acked = 0, tbu_sync_acked = 0;
	u32 tcu_inv_pending = 0, tcu_sync_pending = 0;
	unsigned long tbu_ids = 0;
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
	int ret;

	static DEFINE_RATELIMIT_STATE(_rs,
				      DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	dev_err_ratelimited(smmu->dev,
			    "TLB sync timed out -- SMMU may be deadlocked\n");

	sync_inv_ack = arm_smmu_readl(smmu,
				      ARM_SMMU_IMPL_DEF5,
				      ARM_SMMU_STATS_SYNC_INV_TBU_ACK);
	ret = qcom_scm_io_readl((unsigned long)(smmu->phys_addr +
				ARM_SMMU_TBU_PWR_STATUS), &tbu_pwr_status);
	if (ret) {
		dev_err_ratelimited(smmu->dev,
				    "SCM read of TBU power status fails: %d\n",
				    ret);
		goto out;
	}

	ret = qcom_scm_io_readl((unsigned long)(smmu->phys_addr +
				ARM_SMMU_MMU2QSS_AND_SAFE_WAIT_CNTR),
				&sync_inv_progress);
	if (ret) {
		dev_err_ratelimited(smmu->dev,
				    "SCM read of TBU sync/inv prog fails: %d\n",
				    ret);
		goto out;
	}

	if (sync_inv_ack) {
		tbu_inv_pending = FIELD_GET(TBU_INV_REQ, sync_inv_ack);
		tbu_inv_acked = FIELD_GET(TBU_INV_ACK, sync_inv_ack);
		tbu_sync_pending = FIELD_GET(TBU_SYNC_REQ, sync_inv_ack);
		tbu_sync_acked = FIELD_GET(TBU_SYNC_ACK, sync_inv_ack);
	}

	if (tbu_pwr_status) {
		if (tbu_sync_pending)
			tbu_ids = tbu_pwr_status & ~tbu_sync_acked;
		else if (tbu_inv_pending)
			tbu_ids = tbu_pwr_status & ~tbu_inv_acked;
	}

	tcu_inv_pending = FIELD_GET(TCU_INV_IN_PRGSS, sync_inv_progress);
	tcu_sync_pending = FIELD_GET(TCU_SYNC_IN_PRGSS, sync_inv_progress);

	if (__ratelimit(&_rs)) {
		unsigned long tbu_id;

		dev_err(smmu->dev,
			"TBU ACK 0x%x TBU PWR 0x%x TCU sync_inv 0x%x\n",
			sync_inv_ack, tbu_pwr_status, sync_inv_progress);
		dev_err(smmu->dev,
			"TCU invalidation %s, TCU sync %s\n",
			tcu_inv_pending?"pending":"completed",
			tcu_sync_pending?"pending":"completed");

		for_each_set_bit(tbu_id, &tbu_ids, sizeof(tbu_ids) *
				 BITS_PER_BYTE) {

			struct qsmmuv500_tbu_device *tbu;

			tbu = qsmmuv500_find_tbu(smmu,
						 (u16)(tbu_id << TBUID_SHIFT));
			if (tbu) {
				dev_err(smmu->dev,
					"TBU %s ack pending for TBU %s, %s\n",
					tbu_sync_pending?"sync" : "inv",
					dev_name(tbu->dev),
					tbu_sync_pending ?
					"check pending transactions on TBU"
					: "check for TBU power status");
				arm_smmu_testbus_dump(smmu,
						(u16)(tbu_id << TBUID_SHIFT));
			}
		}

		/*dump TCU testbus*/
		arm_smmu_testbus_dump(smmu, U16_MAX);

	}

	if (tcu_sync_pending) {
		schedule_work(&data->outstanding_tnx_work);
		return;
	}
out:
	BUG_ON(IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG));
}

static void qsmmuv500_device_remove(struct arm_smmu_device *smmu)
{
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);

	cancel_work_sync(&data->outstanding_tnx_work);
}

static bool arm_smmu_fwspec_match_smr(struct iommu_fwspec *fwspec,
				      struct arm_smmu_smr *smr)
{
	struct arm_smmu_smr *smr2;
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	int i, idx;

	for_each_cfg_sme(fwspec, i, idx) {
		smr2 = &smmu->smrs[idx];
		/* Continue if table entry does not match */
		if ((smr->id ^ smr2->id) & ~(smr->mask | smr2->mask))
			continue;
		return true;
	}
	return false;
}

static int qsmmuv500_tbu_halt(struct qsmmuv500_tbu_device *tbu,
				struct arm_smmu_domain *smmu_domain)
{
	unsigned long flags;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	u32 halt, fsr, status;
	void __iomem *tbu_base;

	if (of_property_read_bool(tbu->dev->of_node,
						"qcom,opt-out-tbu-halting")) {
		dev_notice(tbu->dev, "TBU opted-out for halting!\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&tbu->halt_lock, flags);
	if (tbu->halt_count) {
		tbu->halt_count++;
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return 0;
	}

	tbu_base = tbu->base;
	halt = readl_relaxed(tbu_base + DEBUG_SID_HALT_REG);
	halt |= DEBUG_SID_HALT_REQ;
	writel_relaxed(halt, tbu_base + DEBUG_SID_HALT_REG);

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if ((fsr & FSR_FAULT) && (fsr & FSR_SS)) {
		u32 sctlr_orig, sctlr;
		/*
		 * We are in a fault; Our request to halt the bus will not
		 * complete until transactions in front of us (such as the fault
		 * itself) have completed. Disable iommu faults and terminate
		 * any existing transactions.
		 */
		sctlr_orig = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR);
		sctlr = sctlr_orig & ~(SCTLR_CFCFG | SCTLR_CFIE);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr);

		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
				  RESUME_TERMINATE);

		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr_orig);
	}

	if (readl_poll_timeout_atomic(tbu_base + DEBUG_SR_HALT_ACK_REG, status,
					(status & DEBUG_SR_HALT_ACK_VAL),
					0, TBU_DBG_TIMEOUT_US)) {
		dev_err(tbu->dev, "Couldn't halt TBU!\n");

		halt = readl_relaxed(tbu_base + DEBUG_SID_HALT_REG);
		halt &= ~DEBUG_SID_HALT_REQ;
		writel_relaxed(halt, tbu_base + DEBUG_SID_HALT_REG);

		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return -ETIMEDOUT;
	}

	tbu->halt_count = 1;
	spin_unlock_irqrestore(&tbu->halt_lock, flags);
	return 0;
}

static void qsmmuv500_tbu_resume(struct qsmmuv500_tbu_device *tbu)
{
	unsigned long flags;
	u32 val;
	void __iomem *base;

	spin_lock_irqsave(&tbu->halt_lock, flags);
	if (!tbu->halt_count) {
		WARN(1, "%s: bad tbu->halt_count", dev_name(tbu->dev));
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return;

	} else if (tbu->halt_count > 1) {
		tbu->halt_count--;
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return;
	}

	base = tbu->base;
	val = readl_relaxed(base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_REQ;
	writel_relaxed(val, base + DEBUG_SID_HALT_REG);

	tbu->halt_count = 0;
	spin_unlock_irqrestore(&tbu->halt_lock, flags);
}

static struct qsmmuv500_tbu_device *qsmmuv500_find_tbu(
	struct arm_smmu_device *smmu, u32 sid)
{
	struct qsmmuv500_tbu_device *tbu = NULL;
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);

	list_for_each_entry(tbu, &data->tbus, list) {
		if (tbu->sid_start <= sid &&
		    sid < tbu->sid_start + tbu->num_sids)
			return tbu;
	}
	return NULL;
}

static int qsmmuv500_ecats_lock(struct arm_smmu_domain *smmu_domain,
				struct qsmmuv500_tbu_device *tbu,
				unsigned long *flags)
{
	struct arm_smmu_device *smmu = tbu->smmu;
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
	u32 val;

	spin_lock_irqsave(&data->atos_lock, *flags);
	/* The status register is not accessible on version 1.0 */
	if (data->version == 0x01000000)
		return 0;

	if (readl_poll_timeout_atomic(tbu->status_reg,
					val, (val == 0x1), 0,
					TBU_DBG_TIMEOUT_US)) {
		dev_err(tbu->dev, "ECATS hw busy!\n");
		spin_unlock_irqrestore(&data->atos_lock, *flags);
		return  -ETIMEDOUT;
	}

	return 0;
}

static void qsmmuv500_ecats_unlock(struct arm_smmu_domain *smmu_domain,
					struct qsmmuv500_tbu_device *tbu,
					unsigned long *flags)
{
	struct arm_smmu_device *smmu = tbu->smmu;
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);

	/* The status register is not accessible on version 1.0 */
	if (data->version != 0x01000000)
		writel_relaxed(0, tbu->status_reg);
	spin_unlock_irqrestore(&data->atos_lock, *flags);
}

/*
 * Zero means failure.
 */
static phys_addr_t qsmmuv500_iova_to_phys(
		struct arm_smmu_domain *smmu_domain, dma_addr_t iova, u32 sid,
		unsigned long trans_flags)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct qsmmuv500_tbu_device *tbu;
	int ret;
	phys_addr_t phys = 0;
	u64 val, fsr;
	long iova_ext_bits = (s64)iova >> smmu->va_size;
	bool split_tables = test_bit(DOMAIN_ATTR_SPLIT_TABLES,
				     smmu_domain->attributes);
	unsigned long flags;
	int idx = cfg->cbndx;
	u32 sctlr_orig, sctlr;
	int needs_redo = 0;
	ktime_t timeout;

	if (iova_ext_bits && split_tables)
		iova_ext_bits = ~iova_ext_bits;

	tbu = qsmmuv500_find_tbu(smmu, sid);
	if (!tbu)
		return 0;

	ret = arm_smmu_power_on(tbu->pwr);
	if (ret)
		return 0;

	ret = qsmmuv500_tbu_halt(tbu, smmu_domain);
	if (ret)
		goto out_power_off;

	/*
	 * ECATS can trigger the fault interrupt, so disable it temporarily
	 * and check for an interrupt manually.
	 */
	sctlr_orig = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~(SCTLR_CFCFG | SCTLR_CFIE);
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr);

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if (fsr & FSR_FAULT) {
		/* Clear pending interrupts */
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation.
		 */
		wmb();

		/*
		 * TBU halt takes care of resuming any stalled transcation.
		 * Kept it here for completeness sake.
		 */
		if (fsr & FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  RESUME_TERMINATE);
	}

	/* checking out of bound fault after resuming stalled transactions */
	if (iova_ext_bits) {
		dev_err_ratelimited(smmu->dev,
				    "ECATS: address out of bounds: %pad\n",
				    &iova);
		goto out_resume;
	}


	/* Only one concurrent atos operation */
	ret = qsmmuv500_ecats_lock(smmu_domain, tbu, &flags);
	if (ret)
		goto out_resume;

redo:
	/* Set address and stream-id */
	val = readq_relaxed(tbu->base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_SID;
	val |= FIELD_PREP(DEBUG_SID_HALT_SID, sid);
	writeq_relaxed(val, tbu->base + DEBUG_SID_HALT_REG);
	writeq_relaxed(iova, tbu->base + DEBUG_VA_ADDR_REG);
	val = FIELD_PREP(DEBUG_AXUSER_CDMID, DEBUG_AXUSER_CDMID_VAL);
	writeq_relaxed(val, tbu->base + DEBUG_AXUSER_REG);

	/* Write-back Read and Write-Allocate */
	val = FIELD_PREP(DEBUG_TXN_AXCACHE, 0xF);

	/* Non-secure Access */
	val |= FIELD_PREP(DEBUG_TXN_AXPROT, DEBUG_TXN_AXPROT_NSEC);

	/* Write or Read Access */
	if (trans_flags & IOMMU_TRANS_WRITE)
		val |= DEBUG_TXN_WRITE;

	/* Priviledged or Unpriviledged Access */
	if (trans_flags & IOMMU_TRANS_PRIV)
		val |= FIELD_PREP(DEBUG_TXN_AXPROT, DEBUG_TXN_AXPROT_PRIV);

	/* Data or Instruction Access */
	if (trans_flags & IOMMU_TRANS_INST)
		val |= FIELD_PREP(DEBUG_TXN_AXPROT, DEBUG_TXN_AXPROT_INST);

	val |= DEBUG_TXN_TRIGGER;
	writeq_relaxed(val, tbu->base + DEBUG_TXN_TRIGG_REG);

	ret = 0;
	timeout = ktime_add_us(ktime_get(), TBU_DBG_TIMEOUT_US);
	for (;;) {
		val = readl_relaxed(tbu->base + DEBUG_SR_HALT_ACK_REG);
		if (!(val & DEBUG_SR_ECATS_RUNNING_VAL))
			break;
		val = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
		if (val & FSR_FAULT)
			break;
		if (ktime_compare(ktime_get(), timeout) > 0) {
			dev_err_ratelimited(tbu->dev, "ECATS translation timed out!\n");
			ret = -ETIMEDOUT;
			break;
		}
	}

	val = readq_relaxed(tbu->base + DEBUG_PAR_REG);
	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if (val & DEBUG_PAR_FAULT_VAL) {
		dev_err(tbu->dev, "ECATS generated a fault interrupt! FSR = %llx, SID=0x%x\n",
			fsr, sid);

		dev_err(tbu->dev, "ECATS translation failed! PAR = %llx\n",
			val);
		/* Clear pending interrupts */
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation.
		 */
		wmb();

		if (fsr & FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  RESUME_TERMINATE);

		ret = -EINVAL;
	}

	phys = FIELD_GET(DEBUG_PAR_PA, val);
	if (ret < 0)
		phys = 0;

	/* Reset hardware */
	writeq_relaxed(0, tbu->base + DEBUG_TXN_TRIGG_REG);
	writeq_relaxed(0, tbu->base + DEBUG_VA_ADDR_REG);
	val = readl_relaxed(tbu->base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_SID;
	writel_relaxed(val, tbu->base + DEBUG_SID_HALT_REG);

	/*
	 * After a failed translation, the next successful translation will
	 * incorrectly be reported as a failure.
	 */
	if (!phys && needs_redo++ < 2)
		goto redo;

	qsmmuv500_ecats_unlock(smmu_domain, tbu, &flags);

out_resume:
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr_orig);
	qsmmuv500_tbu_resume(tbu);

out_power_off:
	/* Read to complete prior write transcations */
	val = readl_relaxed(tbu->base + DEBUG_SR_HALT_ACK_REG);

	/* Wait for read to complete before off */
	rmb();

	arm_smmu_power_off(tbu->smmu, tbu->pwr);

	return phys;
}

static phys_addr_t qsmmuv500_iova_to_phys_hard(
					struct arm_smmu_domain *smmu_domain,
					dma_addr_t iova,
					unsigned long trans_flags)
{
	u16 sid;
	struct msm_iommu_domain *msm_domain = &smmu_domain->domain;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct iommu_fwspec *fwspec;
	u32 frsynra;

	/* Check to see if the domain is associated with the test
	 * device. If the domain belongs to the test device, then
	 * pick the SID from fwspec.
	 */
	if (msm_domain->is_debug_domain) {
		fwspec = dev_iommu_fwspec_get(smmu_domain->dev);
		sid    = (u16)fwspec->ids[0];
	} else {

		/* If the domain belongs to an actual device, read
		 * SID from the corresponding frsynra register
		 */
		frsynra = arm_smmu_gr1_read(smmu,
					    ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
		sid = FIELD_GET(CBFRSYNRA_SID, frsynra);
	}
	return qsmmuv500_iova_to_phys(smmu_domain, iova, sid, trans_flags);
}

static void qsmmuv500_release_group_iommudata(void *data)
{
	kfree(data);
}

/* If a device has a valid actlr, it must match */
static int qsmmuv500_device_group(struct device *dev,
				struct iommu_group *group)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
	struct qsmmuv500_group_iommudata *iommudata;
	u32 actlr, i;
	struct arm_smmu_smr *smr;

	iommudata = to_qsmmuv500_group_iommudata(group);
	if (!iommudata) {
		iommudata = kzalloc(sizeof(*iommudata), GFP_KERNEL);
		if (!iommudata)
			return -ENOMEM;

		iommu_group_set_iommudata(group, iommudata,
				qsmmuv500_release_group_iommudata);
	}

	for (i = 0; i < data->actlr_tbl_size; i++) {
		smr = &data->actlrs[i].smr;
		actlr = data->actlrs[i].actlr;

		if (!arm_smmu_fwspec_match_smr(fwspec, smr))
			continue;

		if (!iommudata->has_actlr) {
			iommudata->actlr = actlr;
			iommudata->has_actlr = true;
		} else if (iommudata->actlr != actlr) {
			return -EINVAL;
		}
	}

	return 0;
}

static void qsmmuv500_init_cb(struct arm_smmu_domain *smmu_domain,
				struct device *dev)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct qsmmuv500_group_iommudata *iommudata =
		to_qsmmuv500_group_iommudata(dev->iommu_group);
	int idx = smmu_domain->cfg.cbndx;
	const struct iommu_flush_ops *tlb;

	if (!iommudata->has_actlr)
		return;

	tlb = smmu_domain->pgtbl_info[0].pgtbl_cfg.tlb;

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ACTLR, iommudata->actlr);

	/*
	 * Flush the context bank after modifying ACTLR to ensure there
	 * are no cache entries with stale state
	 */
	tlb->tlb_flush_all(smmu_domain);
}

static int qsmmuv500_tbu_register(struct device *dev, void *cookie)
{
	struct resource *res;
	struct qsmmuv500_tbu_device *tbu;
	struct qsmmuv500_archdata *data = cookie;
	struct platform_device *pdev = to_platform_device(dev);
	int i, err, num_irqs = 0;

	if (!dev->driver) {
		dev_err(dev, "TBU failed probe, QSMMUV500 cannot continue!\n");
		return -EINVAL;
	}

	tbu = dev_get_drvdata(dev);

	INIT_LIST_HEAD(&tbu->list);
	tbu->smmu = &data->smmu;
	list_add(&tbu->list, &data->tbus);

	while ((res = platform_get_resource(pdev, IORESOURCE_IRQ, num_irqs)))
		num_irqs++;

	tbu->irqs = devm_kzalloc(dev, sizeof(*tbu->irqs) * num_irqs,
				  GFP_KERNEL);
	if (!tbu->irqs)
		return -ENOMEM;

	for (i = 0; i < num_irqs; ++i) {
		int irq = platform_get_irq(pdev, i);

		if (irq < 0) {
			dev_err(dev, "failed to get irq index %d\n", i);
			return -ENODEV;
		}
		tbu->irqs[i] = irq;

		err = devm_request_threaded_irq(tbu->dev, tbu->irqs[i],
					NULL, arm_smmu_debug_capture_bus_match,
					IRQF_ONESHOT | IRQF_SHARED,
					"capture bus", tbu);
		if (err) {
			dev_err(dev, "failed to request capture bus irq%d (%u)\n",
				i, tbu->irqs[i]);
			return err;
		}
	}

	qsmmuv500_tbu_testbus_init(tbu);
	qsmmuv500_capturebus_init(tbu);
	return 0;
}

static int qsmmuv500_read_actlr_tbl(struct qsmmuv500_archdata *data)
{
	int len, i;
	struct device *dev = data->smmu.dev;
	struct actlr_setting *actlrs;
	const __be32 *cell;

	cell = of_get_property(dev->of_node, "qcom,actlr", NULL);
	if (!cell)
		return 0;

	len = of_property_count_elems_of_size(dev->of_node, "qcom,actlr",
						sizeof(u32) * 3);
	if (len < 0)
		return 0;

	actlrs = devm_kzalloc(dev, sizeof(*actlrs) * len, GFP_KERNEL);
	if (!actlrs)
		return -ENOMEM;

	for (i = 0; i < len; i++) {
		actlrs[i].smr.id = of_read_number(cell++, 1);
		actlrs[i].smr.mask = of_read_number(cell++, 1);
		actlrs[i].actlr = of_read_number(cell++, 1);
	}

	data->actlrs = actlrs;
	data->actlr_tbl_size = len;
	return 0;
}

static int qsmmuv500_cfg_probe(struct arm_smmu_device *smmu)
{
	struct qsmmuv500_archdata *data = to_qsmmuv500_archdata(smmu);
	u32 val;

	data->version = readl_relaxed(data->tcu_base + TCU_HW_VERSION_HLOS1);

	val = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sACR);
	val &= ~ARM_MMU500_ACR_CACHE_LOCK;
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sACR, val);
	val = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sACR);
	/*
	 * Modifiying the nonsecure copy of the sACR register is only
	 * allowed if permission is given in the secure sACR register.
	 * Attempt to detect if we were able to update the value.
	 */
	WARN_ON(val & ARM_MMU500_ACR_CACHE_LOCK);

	return 0;
}

static const struct arm_smmu_impl qsmmuv500_impl = {
	.cfg_probe = qsmmuv500_cfg_probe,
	.init_context_bank = qsmmuv500_init_cb,
	.iova_to_phys_hard = qsmmuv500_iova_to_phys_hard,
	.tlb_sync_timeout = qsmmuv500_tlb_sync_timeout,
	.device_remove = qsmmuv500_device_remove,
	.device_group = qsmmuv500_device_group,
};

struct arm_smmu_device *qsmmuv500_impl_init(struct arm_smmu_device *smmu)
{
	struct resource *res;
	struct device *dev = smmu->dev;
	struct qsmmuv500_archdata *data;
	struct platform_device *pdev;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&data->tbus);

	pdev = to_platform_device(dev);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tcu-base");
	if (!res) {
		dev_err(dev, "Unable to get the tcu-base\n");
		return ERR_PTR(-EINVAL);
	}
	data->tcu_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->tcu_base))
		return ERR_CAST(data->tcu_base);

	spin_lock_init(&data->atos_lock);
	data->smmu = *smmu;
	data->smmu.impl = &qsmmuv500_impl;

	qsmmuv500_tcu_testbus_init(&data->smmu);

	ret = qsmmuv500_read_actlr_tbl(data);
	if (ret)
		return ERR_PTR(ret);

	INIT_WORK(&data->outstanding_tnx_work,
		  qsmmuv500_log_outstanding_transactions);

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret)
		return ERR_PTR(ret);

	/* Attempt to register child devices */
	ret = device_for_each_child(dev, data, qsmmuv500_tbu_register);
	if (ret)
		return ERR_PTR(-EPROBE_DEFER);

	devm_kfree(smmu->dev, smmu);
	return &data->smmu;
}

struct arm_smmu_device *qcom_smmu_impl_init(struct arm_smmu_device *smmu)
{
	struct qcom_smmu *qsmmu;

	qsmmu = devm_kzalloc(smmu->dev, sizeof(*qsmmu), GFP_KERNEL);
	if (!qsmmu)
		return ERR_PTR(-ENOMEM);

	qsmmu->smmu = *smmu;

	qsmmu->smmu.impl = &qcom_smmu_impl;
	devm_kfree(smmu->dev, smmu);

	return &qsmmu->smmu;
}

struct arm_smmu_device *qsmmuv2_impl_init(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	struct qsmmuv2_archdata *data;
	struct platform_device *pdev;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	pdev = to_platform_device(dev);

	spin_lock_init(&data->atos_lock);
	data->smmu = *smmu;
	data->smmu.impl = &qsmmuv2_impl;

	ret = arm_smmu_parse_impl_def_registers(&data->smmu);
	if (ret)
		return ERR_PTR(ret);

	devm_kfree(smmu->dev, smmu);
	return &data->smmu;
}
