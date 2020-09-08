// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/qcom_scm.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "arm-smmu.h"

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

		smmu->smrs[i].mask = FIELD_GET(ARM_SMMU_SMR_MASK, smr);
		smmu->smrs[i].id = FIELD_GET(ARM_SMMU_SMR_ID, smr);
		if (smmu->features & ARM_SMMU_FEAT_EXIDS)
			smmu->smrs[i].valid = FIELD_GET(
						ARM_SMMU_S2CR_EXIDVALID,
						s2cr);
		else
			smmu->smrs[i].valid = FIELD_GET(
						ARM_SMMU_SMR_VALID,
						smr);

		smmu->s2crs[i].group = NULL;
		smmu->s2crs[i].count = 0;
		smmu->s2crs[i].type = FIELD_GET(ARM_SMMU_S2CR_TYPE, s2cr);
		smmu->s2crs[i].privcfg = FIELD_GET(ARM_SMMU_S2CR_PRIVCFG, s2cr);
		smmu->s2crs[i].cbndx = FIELD_GET(ARM_SMMU_S2CR_CBNDX, s2cr);

		if (!smmu->smrs[i].valid)
			continue;

		smmu->s2crs[i].pinned = true;
		bitmap_set(smmu->context_map, smmu->s2crs[i].cbndx, 1);
	}

	return 0;
}

static const struct of_device_id qcom_smmu_client_of_match[] __maybe_unused = {
	{ .compatible = "qcom,adreno" },
	{ .compatible = "qcom,mdp4" },
	{ .compatible = "qcom,mdss" },
	{ .compatible = "qcom,sc7180-mdss" },
	{ .compatible = "qcom,sc7180-mss-pil" },
	{ .compatible = "qcom,sdm845-mdss" },
	{ .compatible = "qcom,sdm845-mss-pil" },
	{ }
};

static int qcom_smmu_def_domain_type(struct device *dev)
{
	const struct of_device_id *match =
		of_match_device(qcom_smmu_client_of_match, dev);

	return match ? IOMMU_DOMAIN_IDENTITY : 0;
}

static int qcom_sdm845_smmu500_reset(struct arm_smmu_device *smmu)
{
	int ret;

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

static int qcom_smmu500_reset(struct arm_smmu_device *smmu)
{
	const struct device_node *np = smmu->dev->of_node;

	arm_mmu500_reset(smmu);

	if (of_device_is_compatible(np, "qcom,sdm845-smmu-500"))
		return qcom_sdm845_smmu500_reset(smmu);

	return 0;
}

static const struct arm_smmu_impl qcom_smmu_impl = {
	.def_domain_type = qcom_smmu_def_domain_type,
	.cfg_probe = qcom_sdm845_smmu500_cfg_probe,
	.reset = qcom_smmu500_reset,
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

#define TNX_TCR_CNTL			0x130
#define TNX_TCR_CNTL_TBU_OT_CAPTURE_EN	BIT(18)
#define TNX_TCR_CNTL_ALWAYS_CAPTURE	BIT(15)
#define TNX_TCR_CNTL_MATCH_MASK_UPD	BIT(7)
#define TNX_TCR_CNTL_MATCH_MASK_VALID	BIT(6)

#define CAPTURE1_SNAPSHOT_1		0x138

#define TNX_TCR_CNTL_2			0x178
#define TNX_TCR_CNTL_2_CAP1_VALID	BIT(0)

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
				      ARM_SMMU_IMPL_DEF0,
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
			}
		}
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
				      struct arm_smmu_master_cfg *cfg,
				      struct arm_smmu_smr *smr)
{
	struct arm_smmu_smr *smr2;
	struct arm_smmu_device *smmu = cfg->smmu;
	int i, idx;

	for_each_cfg_sme(cfg, fwspec, i, idx) {
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
	if ((fsr & ARM_SMMU_FSR_FAULT) && (fsr & ARM_SMMU_FSR_SS)) {
		u32 sctlr_orig, sctlr;
		/*
		 * We are in a fault; Our request to halt the bus will not
		 * complete until transactions in front of us (such as the fault
		 * itself) have completed. Disable iommu faults and terminate
		 * any existing transactions.
		 */
		sctlr_orig = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR);
		sctlr = sctlr_orig & ~(ARM_SMMU_SCTLR_CFCFG | ARM_SMMU_SCTLR_CFIE);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr);

		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
				  ARM_SMMU_RESUME_TERMINATE);

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
	__acquires(&smmu->atos_lock)
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
	__releases(&smmu->atos_lock)
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
	unsigned long spinlock_flags;
	int idx = cfg->cbndx;
	u32 sctlr_orig, sctlr;
	int needs_redo = 0;
	ktime_t timeout;

	/* only 36 bit iova is supported */
	if (iova >= (1ULL << 36)) {
		dev_err_ratelimited(smmu->dev, "ECATS: address too large: %pad\n",
					&iova);
		return 0;
	}

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
	sctlr = sctlr_orig & ~(ARM_SMMU_SCTLR_CFCFG | ARM_SMMU_SCTLR_CFIE);
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr);

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);
	if (fsr & ARM_SMMU_FSR_FAULT) {
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
		if (fsr & ARM_SMMU_FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  ARM_SMMU_RESUME_TERMINATE);
	}

	/* Only one concurrent atos operation */
	ret = qsmmuv500_ecats_lock(smmu_domain, tbu, &spinlock_flags);
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
		if (val & ARM_SMMU_FSR_FAULT)
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

		if (fsr & ARM_SMMU_FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  ARM_SMMU_RESUME_TERMINATE);

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

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, sctlr_orig);
	qsmmuv500_ecats_unlock(smmu_domain, tbu, &spinlock_flags);

out_resume:
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
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	u32 frsynra;

	/*
	 * Read the SID from the frsynra register, kick of translation
	 */
	frsynra = arm_smmu_gr1_read(smmu,
				    ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
	frsynra &= CBFRSYNRA_SID_MASK;
	sid      = frsynra;

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
	struct arm_smmu_master_cfg *cfg = dev_iommu_priv_get(dev);
	struct arm_smmu_device *smmu = cfg->smmu;
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

		if (!arm_smmu_fwspec_match_smr(fwspec, cfg, smr))
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

	tlb = smmu_domain->pgtbl_cfg.tlb;

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ACTLR, iommudata->actlr);

	/*
	 * Flush the context bank after modifying ACTLR to ensure there
	 * are no cache entries with stale state
	 */
	tlb->tlb_flush_all(smmu_domain);
}

static int qsmmuv500_tbu_register(struct device *dev, void *cookie)
{
	struct qsmmuv500_tbu_device *tbu;
	struct qsmmuv500_archdata *data = cookie;

	if (!dev->driver) {
		dev_err(dev, "TBU failed probe, QSMMUV500 cannot continue!\n");
		return -EINVAL;
	}

	tbu = dev_get_drvdata(dev);

	INIT_LIST_HEAD(&tbu->list);
	tbu->smmu = &data->smmu;
	list_add(&tbu->list, &data->tbus);
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
