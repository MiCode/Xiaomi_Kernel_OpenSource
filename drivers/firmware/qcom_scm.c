// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010,2015,2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/qcom_scm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/reset-controller.h>
#include <soc/qcom/qseecom_scm.h>

#include "qcom_scm.h"
#include "qtee_shmbridge_internal.h"

#define SCM_HAS_CORE_CLK	BIT(0)
#define SCM_HAS_IFACE_CLK	BIT(1)
#define SCM_HAS_BUS_CLK		BIT(2)

struct qcom_scm {
	struct device *dev;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct clk *bus_clk;
	struct reset_controller_dev reset;
	struct notifier_block restart_nb;

	u64 dload_mode_addr;
};

static struct qcom_scm *__scm;

static int qcom_scm_clk_enable(void)
{
	int ret;

	ret = clk_prepare_enable(__scm->core_clk);
	if (ret)
		goto bail;

	ret = clk_prepare_enable(__scm->iface_clk);
	if (ret)
		goto disable_core;

	ret = clk_prepare_enable(__scm->bus_clk);
	if (ret)
		goto disable_iface;

	return 0;

disable_iface:
	clk_disable_unprepare(__scm->iface_clk);
disable_core:
	clk_disable_unprepare(__scm->core_clk);
bail:
	return ret;
}

static void qcom_scm_clk_disable(void)
{
	clk_disable_unprepare(__scm->core_clk);
	clk_disable_unprepare(__scm->iface_clk);
	clk_disable_unprepare(__scm->bus_clk);
}

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int qcom_scm_set_cold_boot_addr(void *entry, const cpumask_t *cpus)
{
	return __qcom_scm_set_cold_boot_addr(__scm ? __scm->dev : NULL, entry,
					     cpus);
}
EXPORT_SYMBOL(qcom_scm_set_cold_boot_addr);

/**
 * scm_set_boot_addr_mc - Set entry physical address for cpus
 * @dev: Device pointer
 * @addr: 32bit physical address
 * @aff0: Collective bitmask of the affinity-level-0 of the mpidr
 *	  1<<aff0_CPU0| 1<<aff0_CPU1....... | 1<<aff0_CPU32
 *	  Supports maximum 32 cpus under any affinity level.
 * @aff1:  Collective bitmask of the affinity-level-1 of the mpidr
 * @aff2:  Collective bitmask of the affinity-level-2 of the mpidr
 * @flags: Flag to differentiate between coldboot vs warmboot
 */
int qcom_scm_set_warm_boot_addr_mc(void *entry, u32 aff0, u32 aff1, u32 aff2, u32 flags)
{
	return __qcom_scm_set_warm_boot_addr_mc(__scm->dev, entry, aff0, aff1, aff2, flags);
}
EXPORT_SYMBOL(qcom_scm_set_warm_boot_addr_mc);

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int qcom_scm_set_warm_boot_addr(void *entry, const cpumask_t *cpus)
{
	return __qcom_scm_set_warm_boot_addr(__scm->dev, entry, cpus);
}
EXPORT_SYMBOL(qcom_scm_set_warm_boot_addr);

/**
 * qcom_scm_cpu_hp() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void qcom_scm_cpu_hp(u32 flags)
{
	__qcom_scm_cpu_hp(__scm ? __scm->dev : NULL, flags);
}
EXPORT_SYMBOL(qcom_scm_cpu_hp);

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void qcom_scm_cpu_power_down(u32 flags)
{
	__qcom_scm_cpu_power_down(__scm ? __scm->dev : NULL, flags);
}
EXPORT_SYMBOL(qcom_scm_cpu_power_down);

/**
 * qcm_scm_sec_wdog_deactivate() - Deactivate secure watchdog
 */
int qcom_scm_sec_wdog_deactivate(void)
{
	return __qcom_scm_sec_wdog_deactivate(__scm->dev);
}
EXPORT_SYMBOL(qcom_scm_sec_wdog_deactivate);

int qcom_scm_sec_wdog_trigger(void)
{
	return __qcom_scm_sec_wdog_trigger(__scm->dev);
}
EXPORT_SYMBOL(qcom_scm_sec_wdog_trigger);

#ifdef CONFIG_TLB_CONF_HANDLER
int qcom_scm_tlb_conf_handler(unsigned long addr)
{
	return __qcom_scm_tlb_conf_handler(__scm->dev, addr);
}
EXPORT_SYMBOL(qcom_scm_tlb_conf_handler);
#endif

/**
 * qcom_scm_disable_sdi() - Disable SDI
 */
void qcom_scm_disable_sdi(void)
{
	__qcom_scm_disable_sdi(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_disable_sdi);

int qcom_scm_set_remote_state(u32 state, u32 id)
{
	return __qcom_scm_set_remote_state(__scm->dev, state, id);
}
EXPORT_SYMBOL(qcom_scm_set_remote_state);

int qcom_scm_spin_cpu(void)
{
	return __qcom_scm_spin_cpu(__scm->dev);
}
EXPORT_SYMBOL(qcom_scm_spin_cpu);

void qcom_scm_set_download_mode(enum qcom_download_mode mode,
				phys_addr_t tcsr_boot_misc)
{
	int ret = -EINVAL;
	struct device *dev = __scm ? __scm->dev : NULL;

	if (tcsr_boot_misc || (__scm && __scm->dload_mode_addr)) {
		ret = __qcom_scm_io_writel(dev,
				tcsr_boot_misc ? : __scm->dload_mode_addr,
				mode);
	} else
		ret = __qcom_scm_set_dload_mode(dev, mode);

	if (ret)
		dev_err(dev, "failed to set download mode: %d\n", ret);
}
EXPORT_SYMBOL(qcom_scm_set_download_mode);

int qcom_scm_config_cpu_errata(void)
{
	return __qcom_scm_config_cpu_errata(__scm->dev);
}
EXPORT_SYMBOL(qcom_scm_config_cpu_errata);

void qcom_scm_phy_update_scm_level_shifter(u32 val)
{
	struct device *dev = __scm ? __scm->dev : NULL;

	__qcom_scm_phy_update_scm_level_shifter(dev, val);
}
EXPORT_SYMBOL(qcom_scm_phy_update_scm_level_shifter);

/**
 * qcom_scm_pas_supported() - Check if the peripheral authentication service is
 *			      available for the given peripherial
 * @peripheral:	peripheral id
 *
 * Returns true if PAS is supported for this peripheral, otherwise false.
 */
bool qcom_scm_pas_supported(u32 peripheral)
{
	int ret;

	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_PIL,
					   QCOM_SCM_PIL_PAS_IS_SUPPORTED);
	if (ret <= 0)
		return false;

	return __qcom_scm_pas_supported(__scm->dev, peripheral);
}
EXPORT_SYMBOL(qcom_scm_pas_supported);

/**
 * qcom_scm_pas_init_image() - Initialize peripheral authentication service
 *			       state machine for a given peripheral, using the
 *			       metadata
 * @peripheral: peripheral id
 * @metadata:	pointer to memory containing ELF header, program header table
 *		and optional blob of data used for authenticating the metadata
 *		and the rest of the firmware
 * @size:	size of the metadata
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_init_image(u32 peripheral, const void *metadata, size_t size)
{
	dma_addr_t mdata_phys;
	void *mdata_buf;
	int ret;

	/*
	 * During the scm call memory protection will be enabled for the meta
	 * data blob, so make sure it's physically contiguous, 4K aligned and
	 * non-cachable to avoid XPU violations.
	 */
	mdata_buf = dma_alloc_coherent(__scm->dev, size, &mdata_phys,
				       GFP_KERNEL);
	if (!mdata_buf) {
		dev_err(__scm->dev, "Allocation of metadata buffer failed.\n");
		return -ENOMEM;
	}
	memcpy(mdata_buf, metadata, size);

	ret = qcom_scm_clk_enable();
	if (ret)
		goto free_metadata;

	ret = __qcom_scm_pas_init_image(__scm->dev, peripheral, mdata_phys);

	qcom_scm_clk_disable();

free_metadata:
	dma_free_coherent(__scm->dev, size, mdata_buf, mdata_phys);

	return ret;
}
EXPORT_SYMBOL(qcom_scm_pas_init_image);

/**
 * qcom_scm_pas_mem_setup() - Prepare the memory related to a given peripheral
 *			      for firmware loading
 * @peripheral:	peripheral id
 * @addr:	start address of memory area to prepare
 * @size:	size of the memory area to prepare
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_mem_setup(u32 peripheral, phys_addr_t addr, phys_addr_t size)
{
	int ret;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = __qcom_scm_pas_mem_setup(__scm->dev, peripheral, addr, size);
	qcom_scm_clk_disable();

	return ret;
}
EXPORT_SYMBOL(qcom_scm_pas_mem_setup);

/**
 * qcom_scm_pas_mss_reset() - MSS restart
 */
int qcom_scm_pas_mss_reset(bool reset)
{
	int ret;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = __qcom_scm_pas_mss_reset(__scm->dev, reset);
	qcom_scm_clk_disable();

	return ret;
}
EXPORT_SYMBOL(qcom_scm_pas_mss_reset);

/**
 * qcom_scm_pas_auth_and_reset() - Authenticate the given peripheral firmware
 *				   and reset the remote processor
 * @peripheral:	peripheral id
 *
 * Return 0 on success.
 */
int qcom_scm_pas_auth_and_reset(u32 peripheral)
{
	int ret;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = __qcom_scm_pas_auth_and_reset(__scm->dev, peripheral);
	qcom_scm_clk_disable();

	return ret;
}
EXPORT_SYMBOL(qcom_scm_pas_auth_and_reset);

/**
 * qcom_scm_pas_shutdown() - Shut down the remote processor
 * @peripheral: peripheral id
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_shutdown(u32 peripheral)
{
	int ret;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = __qcom_scm_pas_shutdown(__scm->dev, peripheral);
	qcom_scm_clk_disable();

	return ret;
}
EXPORT_SYMBOL(qcom_scm_pas_shutdown);

static int qcom_scm_pas_reset_assert(struct reset_controller_dev *rcdev,
				     unsigned long idx)
{
	if (idx != 0)
		return -EINVAL;

	return __qcom_scm_pas_mss_reset(__scm->dev, 1);
}

static int qcom_scm_pas_reset_deassert(struct reset_controller_dev *rcdev,
				       unsigned long idx)
{
	if (idx != 0)
		return -EINVAL;

	return __qcom_scm_pas_mss_reset(__scm->dev, 0);
}

static const struct reset_control_ops qcom_scm_pas_reset_ops = {
	.assert = qcom_scm_pas_reset_assert,
	.deassert = qcom_scm_pas_reset_deassert,
};

int qcom_scm_get_sec_dump_state(u32 *dump_state)
{
	return __qcom_scm_get_sec_dump_state(__scm ? __scm->dev : NULL,
						dump_state);
}
EXPORT_SYMBOL(qcom_scm_get_sec_dump_state);

int qcom_scm_tz_blsp_modify_owner(int food, u64 subsystem, int *out)
{
	return __qcom_scm_tz_blsp_modify_owner(__scm->dev, subsystem, food,
					       out);
}
EXPORT_SYMBOL(qcom_scm_tz_blsp_modify_owner);

int qcom_scm_io_readl(phys_addr_t addr, unsigned int *val)
{
	return __qcom_scm_io_readl(__scm ? __scm->dev : NULL, addr, val);
}
EXPORT_SYMBOL(qcom_scm_io_readl);

int qcom_scm_io_writel(phys_addr_t addr, unsigned int val)
{
	return __qcom_scm_io_writel(__scm ? __scm->dev : NULL, addr, val);
}
EXPORT_SYMBOL(qcom_scm_io_writel);

/**
 * qcom_scm_io_reset()
 */
int qcom_scm_io_reset(void)
{
	return __qcom_scm_io_reset(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_io_reset);

bool qcom_scm_is_secure_wdog_trigger_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_BOOT,
						QCOM_SCM_BOOT_SEC_WDOG_TRIGGER);
}
EXPORT_SYMBOL(qcom_scm_is_secure_wdog_trigger_available);

bool qcom_scm_is_mode_switch_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_BOOT,
						QCOM_SCM_BOOT_SWITCH_MODE);
}
EXPORT_SYMBOL(qcom_scm_is_mode_switch_available);

int qcom_scm_get_jtag_etm_feat_id(u64 *version)
{
	return __qcom_scm_get_feat_version(__scm ? __scm->dev : NULL,
					QCOM_SCM_TZ_DBG_ETM_FEAT_ID, version);
}
EXPORT_SYMBOL(qcom_scm_get_jtag_etm_feat_id);

/**
 * qcom_halt_spmi_pmic_arbiter() - Halt SPMI PMIC arbiter
 *
 * Force the SPMI PMIC arbiter to shutdown so that no more SPMI transactions
 * are sent from the MSM to the PMIC. This is required in order to avoid an
 * SPMI lockup on certain PMIC chips if PS_HOLD is lowered in the middle of
 * an SPMI transaction.
 */
void qcom_scm_halt_spmi_pmic_arbiter(void)
{
	struct device *dev = __scm ? __scm->dev : NULL;

	pr_crit("Calling SCM to disable SPMI PMIC arbiter\n");
	return __qcom_scm_halt_spmi_pmic_arbiter(dev);
}
EXPORT_SYMBOL(qcom_scm_halt_spmi_pmic_arbiter);

/**
 * qcom_deassert_ps_hold() - Deassert PS_HOLD
 *
 * Deassert PS_HOLD to signal the PMIC that we are ready to power down or reset.
 *
 * This function should never return if the SCM call is available.
 */
void qcom_scm_deassert_ps_hold(void)
{
	struct device *dev = __scm ? __scm->dev : NULL;

	__qcom_scm_deassert_ps_hold(dev);
}
EXPORT_SYMBOL(qcom_scm_deassert_ps_hold);

int qcom_scm_paravirt_smmu_attach(u64 sid, u64 asid,
			u64 ste_pa, u64 ste_size, u64 cd_pa,
			u64 cd_size)
{
	return __qcom_scm_paravirt_smmu_attach(__scm ? __scm->dev : NULL, sid, asid,
					ste_pa, ste_size, cd_pa, cd_size);
}
EXPORT_SYMBOL(qcom_scm_paravirt_smmu_attach);

int qcom_scm_paravirt_tlb_inv(u64 asid)
{
	return __qcom_scm_paravirt_tlb_inv(__scm ? __scm->dev : NULL, asid);
}
EXPORT_SYMBOL(qcom_scm_paravirt_tlb_inv);

int qcom_scm_paravirt_smmu_detach(u64 sid)
{
	return __qcom_scm_paravirt_smmu_detach(__scm ? __scm->dev : NULL, sid);
}
EXPORT_SYMBOL(qcom_scm_paravirt_smmu_detach);

void qcom_scm_mmu_sync(bool sync)
{
	__qcom_scm_mmu_sync(__scm ? __scm->dev : NULL, sync);
}
EXPORT_SYMBOL(qcom_scm_mmu_sync);

int qcom_scm_restore_sec_cfg(u32 device_id, u32 spare)
{
	return __qcom_scm_restore_sec_cfg(__scm->dev, device_id, spare);
}
EXPORT_SYMBOL(qcom_scm_restore_sec_cfg);

int qcom_scm_iommu_secure_ptbl_size(u32 spare, size_t *size)
{
	return __qcom_scm_iommu_secure_ptbl_size(__scm->dev, spare, size);
}
EXPORT_SYMBOL(qcom_scm_iommu_secure_ptbl_size);

int qcom_scm_iommu_secure_ptbl_init(u64 addr, u32 size, u32 spare)
{
	return __qcom_scm_iommu_secure_ptbl_init(__scm->dev, addr, size, spare);
}
EXPORT_SYMBOL(qcom_scm_iommu_secure_ptbl_init);

int qcom_scm_mem_protect_video(u32 cp_start, u32 cp_size,
			       u32 cp_nonpixel_start, u32 cp_nonpixel_size)
{
	return __qcom_scm_mem_protect_video(__scm->dev, cp_start, cp_size,
					cp_nonpixel_start, cp_nonpixel_size);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_video);

int qcom_scm_mem_protect_region_id(phys_addr_t paddr, size_t size)
{
	return __qcom_scm_mem_protect_region_id(__scm ? __scm->dev : NULL,
								paddr, size);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_region_id);

int qcom_scm_mem_protect_lock_id2_flat(phys_addr_t list_addr,
				size_t list_size, size_t chunk_size,
				size_t memory_usage, int lock)
{
	return __qcom_scm_mem_protect_lock_id2_flat(__scm->dev, list_addr,
				list_size, chunk_size, memory_usage, lock);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_lock_id2_flat);

int qcom_scm_iommu_secure_map(phys_addr_t sg_list_addr, size_t num_sg,
				size_t sg_block_size, u64 sec_id, int cbndx,
				unsigned long iova, size_t total_len)
{
	return __qcom_scm_iommu_secure_map(__scm->dev, sg_list_addr, num_sg,
				sg_block_size, sec_id, cbndx, iova, total_len);
}
EXPORT_SYMBOL(qcom_scm_iommu_secure_map);

int qcom_scm_iommu_secure_unmap(u64 sec_id, int cbndx, unsigned long iova,
				size_t total_len)
{
	return __qcom_scm_iommu_secure_unmap(__scm->dev, sec_id, cbndx, iova,
						total_len);
}
EXPORT_SYMBOL(qcom_scm_iommu_secure_unmap);

int qcom_scm_mem_protect_audio(phys_addr_t paddr, size_t size)
{
	return __qcom_scm_mem_protect_audio(__scm ? __scm->dev : NULL,
								paddr, size);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_audio);

/**
 * qcom_scm_assign_mem_regions() - Make a secure call to reassign memory
 *				   ownership of several memory regions
 * @mem_regions:    A buffer describing the set of memory regions that need to
 *		    be reassigned
 * @mem_regions_sz: The size of the buffer describing the set of memory
 *                  regions that need to be reassigned (in bytes)
 * @srcvms:	    A buffer populated with he vmid(s) for the current set of
 *		    owners
 * @src_sz:	    The size of the src_vms buffer (in bytes)
 * @newvms:	    A buffer populated with the new owners and corresponding
 *		    permission flags.
 * @newvms_sz:	    The size of the new_vms buffer (in bytes)
 *
 * NOTE: It is up to the caller to ensure that the buffers that will be accessed
 * by the secure world are cache aligned, and have been flushed prior to
 * invoking this call.
 *
 * Return negative errno on failure, 0 on success.
 */
int qcom_scm_assign_mem_regions(struct qcom_scm_mem_map_info *mem_regions,
				size_t mem_regions_sz, u32 *srcvms,
				size_t src_sz,
				struct qcom_scm_current_perm_info *newvms,
				size_t newvms_sz)
{
	return __qcom_scm_assign_mem(__scm ? __scm->dev : NULL,
				     virt_to_phys(mem_regions), mem_regions_sz,
				     virt_to_phys(srcvms), src_sz,
				     virt_to_phys(newvms), newvms_sz);
}
EXPORT_SYMBOL(qcom_scm_assign_mem_regions);

/**
 * qcom_scm_assign_mem() - Make a secure call to reassign memory ownership
 * @mem_addr: mem region whose ownership need to be reassigned
 * @mem_sz:   size of the region.
 * @srcvm:    vmid for current set of owners, each set bit in
 *            flag indicate a unique owner
 * @newvm:    array having new owners and corresponding permission
 *            flags
 * @dest_cnt: number of owners in next set.
 *
 * Return negative errno on failure or 0 on success with @srcvm updated.
 */
int qcom_scm_assign_mem(phys_addr_t mem_addr, size_t mem_sz,
			unsigned int *srcvm,
			const struct qcom_scm_vmperm *newvm,
			unsigned int dest_cnt)
{
	struct qcom_scm_current_perm_info *destvm;
	struct qcom_scm_mem_map_info *mem_to_map;
	phys_addr_t mem_to_map_phys;
	phys_addr_t dest_phys;
	dma_addr_t ptr_phys;
	size_t mem_to_map_sz;
	size_t dest_sz;
	size_t src_sz;
	size_t ptr_sz;
	int next_vm;
	__le32 *src;
	void *ptr;
	int ret, i, b;
	unsigned long srcvm_bits = *srcvm;

	src_sz = hweight_long(srcvm_bits) * sizeof(*src);
	mem_to_map_sz = sizeof(*mem_to_map);
	dest_sz = dest_cnt * sizeof(*destvm);
	ptr_sz = ALIGN(src_sz, SZ_64) + ALIGN(mem_to_map_sz, SZ_64) +
			ALIGN(dest_sz, SZ_64);

	ptr = dma_alloc_coherent(__scm->dev, ptr_sz, &ptr_phys, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	/* Fill source vmid detail */
	src = ptr;
	i = 0;
	for_each_set_bit(b, &srcvm_bits, BITS_PER_LONG)
		src[i++] = cpu_to_le32(b);

	/* Fill details of mem buff to map */
	mem_to_map = ptr + ALIGN(src_sz, SZ_64);
	mem_to_map_phys = ptr_phys + ALIGN(src_sz, SZ_64);
	mem_to_map->mem_addr = cpu_to_le64(mem_addr);
	mem_to_map->mem_size = cpu_to_le64(mem_sz);

	next_vm = 0;
	/* Fill details of next vmid detail */
	destvm = ptr + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	dest_phys = ptr_phys + ALIGN(mem_to_map_sz, SZ_64) + ALIGN(src_sz, SZ_64);
	for (i = 0; i < dest_cnt; i++, destvm++, newvm++) {
		destvm->vmid = cpu_to_le32(newvm->vmid);
		destvm->perm = cpu_to_le32(newvm->perm);
		destvm->ctx = 0;
		destvm->ctx_size = 0;
		next_vm |= BIT(newvm->vmid);
	}

	ret = __qcom_scm_assign_mem(__scm->dev, mem_to_map_phys, mem_to_map_sz,
				    ptr_phys, src_sz, dest_phys, dest_sz);
	dma_free_coherent(__scm->dev, ptr_sz, ptr, ptr_phys);
	if (ret) {
		dev_err(__scm->dev,
			"Assign memory protection call failed %d\n", ret);
		return -EINVAL;
	}

	*srcvm = next_vm;
	return 0;
}
EXPORT_SYMBOL(qcom_scm_assign_mem);

/**
 * qcom_scm_mem_protect_sd_ctrl() - SDE memory protect.
 *
 */
int qcom_scm_mem_protect_sd_ctrl(u32 devid, phys_addr_t mem_addr, u64 mem_size,
				u32 vmid)
{
	return __qcom_scm_mem_protect_sd_ctrl(__scm->dev, devid, mem_addr,
						mem_size, vmid);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_sd_ctrl);

bool qcom_scm_kgsl_set_smmu_aperture_available(void)
{
	int ret;

	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_MP,
					QCOM_SCM_MP_CP_SMMU_APERTURE_ID);

	return ret > 0;
}
EXPORT_SYMBOL(qcom_scm_kgsl_set_smmu_aperture_available);

int qcom_scm_kgsl_set_smmu_aperture(unsigned int num_context_bank)
{
	return __qcom_scm_kgsl_set_smmu_aperture(__scm->dev,
						num_context_bank);
}
EXPORT_SYMBOL(qcom_scm_kgsl_set_smmu_aperture);

int qcom_scm_enable_shm_bridge(void)
{
	return __qcom_scm_enable_shm_bridge(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_enable_shm_bridge);

int qcom_scm_delete_shm_bridge(u64 handle)
{
	return __qcom_scm_delete_shm_bridge(__scm ? __scm->dev : NULL, handle);
}
EXPORT_SYMBOL(qcom_scm_delete_shm_bridge);

int qcom_scm_create_shm_bridge(u64 pfn_and_ns_perm_flags,
	u64 ipfn_and_s_perm_flags, u64 size_and_flags, u64 ns_vmids,
	u64 *handle)
{
	return __qcom_scm_create_shm_bridge(__scm ? __scm->dev : NULL,
				pfn_and_ns_perm_flags, ipfn_and_s_perm_flags,
				size_and_flags, ns_vmids, handle);
}
EXPORT_SYMBOL(qcom_scm_create_shm_bridge);

int qcom_scm_smmu_prepare_atos_id(u64 dev_id, int cb_num, int operation)
{
	return __qcom_scm_smmu_prepare_atos_id(__scm->dev, dev_id, cb_num,
						operation);
}
EXPORT_SYMBOL(qcom_scm_smmu_prepare_atos_id);

/**
 * qcom_mdf_assign_memory_to_subsys - SDE memory protect.
 *
 */
int qcom_mdf_assign_memory_to_subsys(u64 start_addr, u64 end_addr,
		phys_addr_t paddr, u64 size)
{
	return __qcom_mdf_assign_memory_to_subsys(__scm->dev,
		start_addr, end_addr, paddr, size);
}
EXPORT_SYMBOL(qcom_mdf_assign_memory_to_subsys);

int qcom_scm_get_feat_version_cp(u64 *version)
{
	return __qcom_scm_get_feat_version(__scm->dev, QCOM_SCM_MP_CP_FEAT_ID,
						version);
}
EXPORT_SYMBOL(qcom_scm_get_feat_version_cp);

/**
 * qcom_scm_dcvs_core_available() - check if core DCVS operations are available
 */
bool qcom_scm_dcvs_core_available(void)
{
	return __qcom_scm_dcvs_core_available(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_dcvs_core_available);

/**
 * qcom_scm_dcvs_ca_available() - check if context aware DCVS operations are
 * available
 */
bool qcom_scm_dcvs_ca_available(void)
{
	return __qcom_scm_dcvs_ca_available(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_dcvs_ca_available);

/**
 * qcom_scm_dcvs_reset()
 */
int qcom_scm_dcvs_reset(void)
{
	return __qcom_scm_dcvs_reset(__scm ? __scm->dev : NULL);
}
EXPORT_SYMBOL(qcom_scm_dcvs_reset);

int qcom_scm_dcvs_init_v2(phys_addr_t addr, size_t size, int *version)
{
	return __qcom_scm_dcvs_init_v2(__scm->dev, addr, size, version);
}
EXPORT_SYMBOL(qcom_scm_dcvs_init_v2);

int qcom_scm_dcvs_init_ca_v2(phys_addr_t addr, size_t size)
{
	return __qcom_scm_dcvs_init_ca_v2(__scm->dev, addr, size);
}
EXPORT_SYMBOL(qcom_scm_dcvs_init_ca_v2);

int qcom_scm_dcvs_update(int level, s64 total_time, s64 busy_time)
{
	return __qcom_scm_dcvs_update(__scm->dev, level, total_time, busy_time);
}
EXPORT_SYMBOL(qcom_scm_dcvs_update);

int qcom_scm_dcvs_update_v2(int level, s64 total_time, s64 busy_time)
{
	return __qcom_scm_dcvs_update_v2(__scm->dev, level, total_time,
					 busy_time);
}
EXPORT_SYMBOL(qcom_scm_dcvs_update_v2);

int qcom_scm_dcvs_update_ca_v2(int level, s64 total_time, s64 busy_time,
			       int context_count)
{
	return __qcom_scm_dcvs_update_ca_v2(__scm->dev, level, total_time,
					    busy_time, context_count);
}
EXPORT_SYMBOL(qcom_scm_dcvs_update_ca_v2);

int qcom_scm_config_set_ice_key(uint32_t index, phys_addr_t paddr, size_t size,
				uint32_t cipher, unsigned int data_unit,
				unsigned int food)
{
	return __qcom_scm_config_set_ice_key(__scm->dev, index, paddr, size,
					     cipher, data_unit, food);
}
EXPORT_SYMBOL(qcom_scm_config_set_ice_key);

int qcom_scm_clear_ice_key(uint32_t index,  unsigned int food)
{
	return __qcom_scm_clear_ice_key(__scm->dev, index, food);
}
EXPORT_SYMBOL(qcom_scm_clear_ice_key);

/**
 * qcom_scm_hdcp_available() - Check if secure environment supports HDCP.
 *
 * Return true if HDCP is supported, false if not.
 */
bool qcom_scm_hdcp_available(void)
{
	int ret = qcom_scm_clk_enable();

	if (ret)
		return ret;

	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_HDCP,
						QCOM_SCM_HDCP_INVOKE);

	qcom_scm_clk_disable();

	return ret > 0 ? true : false;
}
EXPORT_SYMBOL(qcom_scm_hdcp_available);

/**
 * qcom_scm_hdcp_req() - Send HDCP request.
 * @req: HDCP request array
 * @req_cnt: HDCP request array count
 * @resp: response buffer passed to SCM
 *
 * Write HDCP register(s) through SCM.
 */
int qcom_scm_hdcp_req(struct qcom_scm_hdcp_req *req, u32 req_cnt, u32 *resp)
{
	int ret = qcom_scm_clk_enable();

	if (ret)
		return ret;

	ret = __qcom_scm_hdcp_req(__scm->dev, req, req_cnt, resp);
	qcom_scm_clk_disable();
	return ret;
}
EXPORT_SYMBOL(qcom_scm_hdcp_req);

bool qcom_scm_is_lmh_debug_set_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH,
					QCOM_SCM_LMH_DEBUG_SET);
}
EXPORT_SYMBOL(qcom_scm_is_lmh_debug_set_available);

bool qcom_scm_is_lmh_debug_read_buf_size_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH,
					QCOM_SCM_LMH_DEBUG_READ_BUF_SIZE);
}
EXPORT_SYMBOL(qcom_scm_is_lmh_debug_read_buf_size_available);

bool qcom_scm_is_lmh_debug_read_buf_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH,
					QCOM_SCM_LMH_DEBUG_READ);
}
EXPORT_SYMBOL(qcom_scm_is_lmh_debug_read_buf_available);

bool qcom_scm_is_lmh_debug_get_type_available(void)
{
	return __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH,
					QCOM_SCM_LMH_DEBUG_GET_TYPE);
}
EXPORT_SYMBOL(qcom_scm_is_lmh_debug_get_type_available);

int qcom_scm_lmh_read_buf_size(int *size)
{
	return __qcom_scm_lmh_read_buf_size(__scm->dev, size);
}
EXPORT_SYMBOL(qcom_scm_lmh_read_buf_size);

int qcom_scm_lmh_limit_dcvsh(phys_addr_t payload, uint32_t payload_size,
			u64 limit_node, uint32_t node_id, u64 version)
{
	return __qcom_scm_lmh_limit_dcvsh(__scm->dev, payload, payload_size,
					limit_node, node_id, version);
}
EXPORT_SYMBOL(qcom_scm_lmh_limit_dcvsh);

int qcom_scm_lmh_debug_read(phys_addr_t payload, uint32_t size)
{
	return __qcom_scm_lmh_debug_read(__scm->dev, payload, size);
}
EXPORT_SYMBOL(qcom_scm_lmh_debug_read);

int qcom_scm_lmh_debug_set_config_write(phys_addr_t payload, int payload_size,
					uint32_t *buf, int buf_size)
{
	return __qcom_scm_lmh_debug_config_write(__scm->dev,
			QCOM_SCM_LMH_DEBUG_SET, payload, payload_size, buf,
			buf_size);
}
EXPORT_SYMBOL(qcom_scm_lmh_debug_set_config_write);

int qcom_scm_lmh_get_type(phys_addr_t payload, u64 payload_size,
		u64 debug_type, uint32_t get_from, uint32_t *size)
{
	return __qcom_scm_lmh_get_type(__scm->dev, payload, payload_size,
					debug_type, get_from, size);
}
EXPORT_SYMBOL(qcom_scm_lmh_get_type);

int qcom_scm_lmh_fetch_data(u32 node_id, u32 debug_type, uint32_t *peak,
		uint32_t *avg)
{
	int ret;

	ret = __qcom_scm_is_call_available(__scm->dev, QCOM_SCM_SVC_LMH,
					   QCOM_SCM_LMH_DEBUG_FETCH_DATA);
	if (ret <= 0)
		return ret;

	return __qcom_scm_lmh_fetch_data(__scm->dev, node_id, debug_type,
			peak, avg);
}
EXPORT_SYMBOL(qcom_scm_lmh_fetch_data);

int qcom_scm_smmu_change_pgtbl_format(u64 dev_id, int cbndx)
{
	return __qcom_scm_smmu_change_pgtbl_format(__scm->dev, dev_id, cbndx);
}
EXPORT_SYMBOL(qcom_scm_smmu_change_pgtbl_format);

int qcom_scm_qsmmu500_wait_safe_toggle(bool en)
{
	return __qcom_scm_qsmmu500_wait_safe_toggle(__scm->dev, en);
}
EXPORT_SYMBOL(qcom_scm_qsmmu500_wait_safe_toggle);

int qcom_scm_smmu_notify_secure_lut(u64 dev_id, bool secure)
{
	return __qcom_scm_smmu_notify_secure_lut(__scm->dev, dev_id, secure);
}
EXPORT_SYMBOL(qcom_scm_smmu_notify_secure_lut);

int qcom_scm_qdss_invoke(phys_addr_t paddr, size_t size, u64 *out)
{
	return __qcom_scm_qdss_invoke(__scm->dev, paddr, size, out);
}
EXPORT_SYMBOL(qcom_scm_qdss_invoke);

int qcom_scm_camera_protect_all(uint32_t protect, uint32_t param)
{
	return __qcom_scm_camera_protect_all(__scm->dev, protect, param);
}
EXPORT_SYMBOL(qcom_scm_camera_protect_all);

int qcom_scm_camera_protect_phy_lanes(bool protect, u64 regmask)
{
	return __qcom_scm_camera_protect_phy_lanes(__scm->dev, protect,
						    regmask);
}
EXPORT_SYMBOL(qcom_scm_camera_protect_phy_lanes);

int qcom_scm_tsens_reinit(int *tsens_ret)
{
	return __qcom_scm_tsens_reinit(__scm->dev, tsens_ret);
}
EXPORT_SYMBOL(qcom_scm_tsens_reinit);

int qcom_scm_ice_restore_cfg(void)
{
	return __qcom_scm_ice_restore_cfg(__scm->dev);
}
EXPORT_SYMBOL(qcom_scm_ice_restore_cfg);

int qcom_scm_get_tz_log_feat_id(u64 *version)
{
	return __qcom_scm_get_feat_version(__scm->dev, QCOM_SCM_FEAT_LOG_ID,
					   version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_log_feat_id);

int qcom_scm_get_tz_feat_id_version(u64 feat_id, u64 *version)
{
	return __qcom_scm_get_feat_version(__scm->dev, feat_id,
					   version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_feat_id_version);

int qcom_scm_register_qsee_log_buf(phys_addr_t buf, size_t len)
{
	return __qcom_scm_register_qsee_log_buf(__scm->dev, buf, len);
}
EXPORT_SYMBOL(qcom_scm_register_qsee_log_buf);

int qcom_scm_query_encrypted_log_feature(u64 *enabled)
{
	return __qcom_scm_query_encrypted_log_feature(__scm->dev, enabled);
}
EXPORT_SYMBOL(qcom_scm_query_encrypted_log_feature);

int qcom_scm_request_encrypted_log(phys_addr_t buf, size_t len,
						uint32_t log_id)
{
	return __qcom_scm_request_encrypted_log(__scm->dev, buf, len, log_id);
}
EXPORT_SYMBOL(qcom_scm_request_encrypted_log);

int qcom_scm_invoke_smc_legacy(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data)
{
	return __qcom_scm_invoke_smc_legacy(__scm->dev, in_buf, in_buf_size, out_buf,
		out_buf_size, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_smc_legacy);

int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data)
{
	return __qcom_scm_invoke_smc(__scm->dev, in_buf, in_buf_size, out_buf,
			out_buf_size, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_smc);

int qcom_scm_invoke_callback_response(phys_addr_t out_buf,
	size_t out_buf_size, int32_t *result, u64 *response_type,
	unsigned int *data)
{
	return __qcom_scm_invoke_callback_response(__scm->dev, out_buf,
			out_buf_size, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_callback_response);

int qcom_scm_qseecom_call(u32 cmd_id, struct scm_desc *desc)
{
	return __qcom_scm_qseecom_do(__scm ? __scm->dev : NULL, cmd_id, desc,
				     true);
}
EXPORT_SYMBOL(qcom_scm_qseecom_call);

int qcom_scm_qseecom_call_noretry(u32 cmd_id, struct scm_desc *desc)
{
	return __qcom_scm_qseecom_do(__scm ? __scm->dev : NULL, cmd_id, desc,
				     false);
}
EXPORT_SYMBOL(qcom_scm_qseecom_call_noretry);

int qcom_scm_ddrbw_profiler(phys_addr_t in_buf,
	size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size)
{
	return __qcom_scm_ddrbw_profiler(__scm ? __scm->dev : NULL, in_buf,
			in_buf_size, out_buf, out_buf_size);
}
EXPORT_SYMBOL(qcom_scm_ddrbw_profiler);

/**
 * qcom_scm_is_available() - Checks if SCM is available
 */
bool qcom_scm_is_available(void)
{
	return !!__scm;
}
EXPORT_SYMBOL(qcom_scm_is_available);

static int qcom_scm_do_restart(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	struct qcom_scm *scm = container_of(this, struct qcom_scm, restart_nb);

	if (reboot_mode == REBOOT_WARM)
		__qcom_scm_reboot(scm->dev);

	return NOTIFY_OK;
}

static int qcom_scm_find_dload_address(struct device *dev, u64 *addr)
{
	struct device_node *tcsr;
	struct device_node *np = dev->of_node;
	struct resource res;
	u32 offset;
	int ret;

	tcsr = of_parse_phandle(np, "qcom,dload-mode", 0);
	if (!tcsr)
		return 0;

	ret = of_address_to_resource(tcsr, 0, &res);
	of_node_put(tcsr);
	if (ret)
		return ret;

	ret = of_property_read_u32_index(np, "qcom,dload-mode", 1, &offset);
	if (ret < 0)
		return ret;

	*addr = res.start + offset;

	return 0;
}

static int qcom_scm_probe(struct platform_device *pdev)
{
	struct qcom_scm *scm;
	unsigned long clks;
	int ret;

	scm = devm_kzalloc(&pdev->dev, sizeof(*scm), GFP_KERNEL);
	if (!scm)
		return -ENOMEM;

	ret = qcom_scm_find_dload_address(&pdev->dev, &scm->dload_mode_addr);
	if (ret < 0)
		return ret;

	clks = (unsigned long)of_device_get_match_data(&pdev->dev);

	scm->core_clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(scm->core_clk)) {
		if (PTR_ERR(scm->core_clk) == -EPROBE_DEFER)
			return PTR_ERR(scm->core_clk);

		if (clks & SCM_HAS_CORE_CLK) {
			dev_err(&pdev->dev, "failed to acquire core clk\n");
			return PTR_ERR(scm->core_clk);
		}

		scm->core_clk = NULL;
	}

	scm->iface_clk = devm_clk_get(&pdev->dev, "iface");
	if (IS_ERR(scm->iface_clk)) {
		if (PTR_ERR(scm->iface_clk) == -EPROBE_DEFER)
			return PTR_ERR(scm->iface_clk);

		if (clks & SCM_HAS_IFACE_CLK) {
			dev_err(&pdev->dev, "failed to acquire iface clk\n");
			return PTR_ERR(scm->iface_clk);
		}

		scm->iface_clk = NULL;
	}

	scm->bus_clk = devm_clk_get(&pdev->dev, "bus");
	if (IS_ERR(scm->bus_clk)) {
		if (PTR_ERR(scm->bus_clk) == -EPROBE_DEFER)
			return PTR_ERR(scm->bus_clk);

		if (clks & SCM_HAS_BUS_CLK) {
			dev_err(&pdev->dev, "failed to acquire bus clk\n");
			return PTR_ERR(scm->bus_clk);
		}

		scm->bus_clk = NULL;
	}

	scm->reset.ops = &qcom_scm_pas_reset_ops;
	scm->reset.nr_resets = 1;
	scm->reset.of_node = pdev->dev.of_node;
	ret = devm_reset_controller_register(&pdev->dev, &scm->reset);
	if (ret)
		return ret;

	/* vote for max clk rate for highest performance */
	ret = clk_set_rate(scm->core_clk, INT_MAX);
	if (ret)
		return ret;

	scm->restart_nb.notifier_call = qcom_scm_do_restart;
	scm->restart_nb.priority = 130;
	register_restart_handler(&scm->restart_nb);

	__scm = scm;
	__scm->dev = &pdev->dev;

	__qcom_scm_init();

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret)
		return ret;

	return 0;
}

static void qcom_scm_shutdown(struct platform_device *pdev)
{
	qcom_scm_disable_sdi();
	qcom_scm_halt_spmi_pmic_arbiter();
}

static const struct of_device_id qcom_scm_dt_match[] = {
	{ .compatible = "qcom,scm-apq8064",
	  /* FIXME: This should have .data = (void *) SCM_HAS_CORE_CLK */
	},
	{ .compatible = "qcom,scm-apq8084", .data = (void *)(SCM_HAS_CORE_CLK |
							     SCM_HAS_IFACE_CLK |
							     SCM_HAS_BUS_CLK)
	},
	{ .compatible = "qcom,scm-ipq4019" },
	{ .compatible = "qcom,scm-msm8660", .data = (void *) SCM_HAS_CORE_CLK },
	{ .compatible = "qcom,scm-msm8960", .data = (void *) SCM_HAS_CORE_CLK },
	{ .compatible = "qcom,scm-msm8916", .data = (void *)(SCM_HAS_CORE_CLK |
							     SCM_HAS_IFACE_CLK |
							     SCM_HAS_BUS_CLK)
	},
	{ .compatible = "qcom,scm-msm8974", .data = (void *)(SCM_HAS_CORE_CLK |
							     SCM_HAS_IFACE_CLK |
							     SCM_HAS_BUS_CLK)
	},
	{ .compatible = "qcom,scm-msm8996" },
	{ .compatible = "qcom,scm" },
	{}
};

static struct platform_driver qcom_scm_driver = {
	.driver = {
		.name	= "qcom_scm",
		.of_match_table = qcom_scm_dt_match,
	},
	.probe = qcom_scm_probe,
	.shutdown = qcom_scm_shutdown,
};

static int __init qcom_scm_init(void)
{
	int ret;

	ret = platform_driver_register(&qcom_scm_driver);
	if (ret)
		return ret;

	return qtee_shmbridge_driver_init();
}
subsys_initcall(qcom_scm_init);

#ifdef CONFIG_QCOM_RTIC
static int __init scm_mem_protection_init(void)
{
	return scm_mem_protection_init_do(__scm ? __scm->dev : NULL);
}

early_initcall(scm_mem_protection_init);
#endif

#if IS_MODULE(CONFIG_QCOM_SCM)
static void __exit qcom_scm_exit(void)
{
#if IS_ENABLED(CONFIG_QCOM_SCM_QCPE)
	__qcom_scm_qcpe_exit();
#endif
	platform_driver_unregister(&qcom_scm_driver);
	qtee_shmbridge_driver_exit();
}
module_exit(qcom_scm_exit);
#endif

MODULE_LICENSE("GPL v2");
