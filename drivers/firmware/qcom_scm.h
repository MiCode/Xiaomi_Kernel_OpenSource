/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015,2019 The Linux Foundation. All rights reserved.
 */
#ifndef __QCOM_SCM_INT_H
#define __QCOM_SCM_INT_H

// SIP Services and Function IDs
#define QCOM_SCM_SVC_BOOT			0x01
#define QCOM_SCM_BOOT_SET_ADDR			0x01
#define QCOM_SCM_BOOT_TERMINATE_PC		0x02
#define QCOM_SCM_BOOT_SEC_WDOG_DIS		0x07
#define QCOM_SCM_BOOT_SET_REMOTE_STATE		0x0a
#define QCOM_SCM_BOOT_SET_DLOAD_MODE		0x10
extern int __qcom_scm_set_cold_boot_addr(struct device *dev, void *entry,
		const cpumask_t *cpus);
extern int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
		const cpumask_t *cpus);
extern void __qcom_scm_cpu_power_down(struct device *dev, u32 flags);
extern int __qcom_scm_sec_wdog_deactivate(struct device *dev);
extern int __qcom_scm_set_remote_state(struct device *dev, u32 state, u32 id);
extern int __qcom_scm_set_dload_mode(struct device *dev, bool enable);
#define QCOM_SCM_FLUSH_FLAG_MASK	0x3

#define QCOM_SCM_SVC_PIL			0x02
#define QCOM_SCM_PIL_PAS_INIT_IMAGE		0x01
#define QCOM_SCM_PIL_PAS_MEM_SETUP		0x02
#define QCOM_SCM_PIL_PAS_AUTH_AND_RESET		0x05
#define QCOM_SCM_PIL_PAS_SHUTDOWN		0x06
#define QCOM_SCM_PIL_PAS_IS_SUPPORTED		0x07
#define QCOM_SCM_PIL_PAS_MSS_RESET		0x0a
extern bool __qcom_scm_pas_supported(struct device *dev, u32 peripheral);
extern int  __qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
		dma_addr_t metadata_phys);
extern int  __qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
		phys_addr_t addr, phys_addr_t size);
extern int  __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral);
extern int  __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral);
extern int  __qcom_scm_pas_mss_reset(struct device *dev, bool reset);

#define QCOM_SCM_SVC_UTIL			0x03
#define QCOM_SCM_UTIL_GET_SEC_DUMP_STATE	0x10
extern int __qcom_scm_get_sec_dump_state(struct device *dev, u32 *dump_state);

#define QCOM_SCM_SVC_TZ				0x04
#define QOCM_SCM_TZ_BLSP_MODIFY_OWNER		0x03
extern int __qcom_scm_tz_blsp_modify_owner(struct device *dev, int food,
					   u64 subsystem, int *out);

#define QCOM_SCM_SVC_IO				0x05
#define QCOM_SCM_IO_READ			0x01
#define QCOM_SCM_IO_WRITE			0x02
#define QCOM_SCM_IO_RESET			0x03
extern int __qcom_scm_io_readl(struct device *dev, phys_addr_t addr, unsigned int *val);
extern int __qcom_scm_io_writel(struct device *dev, phys_addr_t addr, unsigned int val);
extern int __qcom_scm_io_reset(struct device *dev);

#define QCOM_SCM_SVC_INFO			0x06
#define QCOM_SCM_INFO_IS_CALL_AVAIL		0x01
#define QCOM_SCM_INFO_GET_FEAT_VERSION_CMD	0x03
extern int __qcom_scm_is_call_available(struct device *dev, u32 svc_id,
		u32 cmd_id);
extern int __qcom_scm_get_feat_version(struct device *dev, u64 feat_id,
					u64 *version);
#define QCOM_SCM_TZ_DBG_ETM_FEAT_ID		0x08

#define QCOM_SCM_SVC_PWR			0x09
#define QCOM_SCM_PWR_MMU_SYNC			0x08
extern void __qcom_scm_mmu_sync(struct device *dev, bool sync);

#define QCOM_SCM_SVC_MP				0x0c
#define QCOM_SCM_MP_RESTORE_SEC_CFG		0x02
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE	0x03
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT	0x04
#define QCOM_SCM_MP_MEM_PROTECT_VIDEO		0x08
#define QCOM_SCM_MP_MEM_PROTECT_REGION_ID	0x10
#define QCOM_SCM_MP_MEM_PROTECT_LOCK_ID2_FLAT	0x11
#define QCOM_SCM_MP_IOMMU_SECURE_MAP2_FLAT	0x12
#define QCOM_SCM_MP_IOMMU_SECURE_UNMAP2_FLAT	0x13
#define QCOM_SCM_MP_ASSIGN			0x16
#define QCOM_SCM_MP_CP_SMMU_APERTURE_ID		0x1b
#define QCOM_SCM_MEMP_SHM_BRIDGE_ENABLE		0x1c
#define QCOM_SCM_MEMP_SHM_BRIDGE_DELETE		0x1d
#define QCOM_SCM_MEMP_SHM_BRDIGE_CREATE		0x1e
#define QCOM_SCM_MP_SMMU_PREPARE_ATOS_ID	0x21
extern int __qcom_scm_restore_sec_cfg(struct device *dev, u32 device_id,
				      u32 spare);
extern int __qcom_scm_iommu_secure_ptbl_size(struct device *dev, u32 spare,
					     size_t *size);
extern int __qcom_scm_iommu_secure_ptbl_init(struct device *dev, u64 addr,
					     u32 size, u32 spare);
extern int __qcom_scm_mem_protect_video(struct device *dev,
				u32 cp_start, u32 cp_size,
				u32 cp_nonpixel_start, u32 cp_nonpixel_size);
extern int __qcom_scm_mem_protect_region_id(struct device *dev,
					phys_addr_t paddr, size_t size);
extern int __qcom_scm_mem_protect_lock_id2_flat(struct device *dev,
				phys_addr_t list_addr, size_t list_size,
				size_t chunk_size, size_t memory_usage,
				int lock);
extern int __qcom_scm_iommu_secure_map(struct device *dev,
				phys_addr_t sg_list_addr, size_t num_sg,
				size_t sg_block_size, u64 sec_id, int cbndx,
				unsigned long iova, size_t total_len);
extern int __qcom_scm_iommu_secure_unmap(struct device *dev, u64 sec_id,
				int cbndx, unsigned long iova,
				size_t total_len);
extern int  __qcom_scm_assign_mem(struct device *dev,
				  phys_addr_t mem_region, size_t mem_sz,
				  phys_addr_t src, size_t src_sz,
				  phys_addr_t dest, size_t dest_sz);
extern int __qcom_scm_kgsl_set_smmu_aperture(struct device *dev,
						unsigned int num_context_bank);
extern int __qcom_scm_enable_shm_bridge(struct device *dev);
extern int __qcom_scm_delete_shm_bridge(struct device *dev, u64 handle);
extern int __qcom_scm_create_shm_bridge(struct device *dev,
			u64 pfn_and_ns_perm_flags, u64 ipfn_and_s_perm_flags,
			u64 size_and_flags, u64 ns_vmids, u64 *handle);
extern int __qcom_scm_smmu_prepare_atos_id(struct device *dev, u64 dev_id,
						int cb_num, int operation);
#define QCOM_SCM_IOMMU_TLBINVAL_FLAG    0x00000001
#define QCOM_SCM_CP_APERTURE_REG	0x0

#define QCOM_SCM_SVC_DCVS			0x0D
#define QCOM_SCM_DCVS_RESET			0x07
#define QCOM_SCM_DCVS_UPDATE			0x08
#define QCOM_SCM_DCVS_INIT			0x09
#define QCOM_SCM_DCVS_UPDATE_V2			0x0a
#define QCOM_SCM_DCVS_INIT_V2			0x0b
#define QCOM_SCM_DCVS_INIT_CA_V2		0x0c
#define QCOM_SCM_DCVS_UPDATE_CA_V2		0x0d
extern bool __qcom_scm_dcvs_core_available(struct device *dev);
extern bool __qcom_scm_dcvs_ca_available(struct device *dev);
extern int __qcom_scm_dcvs_reset(struct device *dev);
extern int __qcom_scm_dcvs_init_v2(struct device *dev, phys_addr_t addr,
				   size_t size, int *version);
extern int __qcom_scm_dcvs_init_ca_v2(struct device *dev, phys_addr_t addr,
				      size_t size);
extern int __qcom_scm_dcvs_update(struct device *dev, int level,
				  s64 total_time, s64 busy_time);

extern int __qcom_scm_dcvs_update_v2(struct device *dev, int level,
				     s64 total_time, s64 busy_time);

extern int __qcom_scm_dcvs_update_ca_v2(struct device *dev, int level,
					s64 total_time, s64 busy_time,
					int context_count);

#define QCOM_SCM_SVC_ES				0x10
#define QCOM_SCM_ES_CONFIG_SET_ICE_KEY		0x05
#define QCOM_SCM_ES_CLEAR_ICE_KEY		0x06
extern int __qcom_scm_config_set_ice_key(struct device *dev, uint32_t index,
					 phys_addr_t paddr, size_t size,
					 uint32_t cipher,
					 unsigned int data_unit,
					 unsigned int food);
extern int __qcom_scm_clear_ice_key(struct device *dev, uint32_t index,
				    unsigned int food);

#define QCOM_SCM_SVC_HDCP			0x11
#define QCOM_SCM_HDCP_INVOKE			0x01
extern int __qcom_scm_hdcp_req(struct device *dev,
		struct qcom_scm_hdcp_req *req, u32 req_cnt, u32 *resp);

#define QCOM_SCM_SVC_SMMU_PROGRAM		0x15
#define QCOM_SCM_SMMU_CHANGE_PGTBL_FORMAT	0x01
#define QCOM_SCM_SMMU_SECURE_LUT		0x03
extern int __qcom_scm_smmu_change_pgtbl_format(struct device *dev, u64 dev_id,
						int cbndx);
extern int __qcom_scm_qsmmu500_wait_safe_toggle(struct device *dev,
						bool enable);
extern int __qcom_scm_smmu_notify_secure_lut(struct device *dev, u64 dev_id,
						bool secure);
#define QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL	0x2

#define QCOM_SCM_SVC_QDSS			0x16
#define QCOM_SCM_QDSS_INVOKE			0x01
extern int __qcom_scm_qdss_invoke(struct device *dev, phys_addr_t addr,
				  size_t size, u64 *out);

#define QCOM_SCM_SVC_CAMERA			0x18
#define QCOM_SCM_CAMERA_PROTECT_PHY_LANES	0x07
extern int __qcom_scm_camera_protect_phy_lanes(struct device *dev,
						bool protect, u64 regmask);

// TOS Services and Function IDs
#define QCOM_SCM_SVC_QSEELOG		0x01
#define QCOM_SCM_QSEELOG_REGISTER	0x06
extern int __qcom_scm_register_qsee_log_buf(struct device *dev, phys_addr_t buf,
					   size_t len);
#define QCOM_SCM_FEAT_LOG_ID		0x0a

#define QCOM_SCM_SVC_KEYSTORE		0x05
#define QCOM_SCM_ICE_RESTORE_KEY_ID	0x06
extern int __qcom_scm_ice_restore_cfg(struct device *dev);

extern void __qcom_scm_init(void);

/* common error codes */
#define QCOM_SCM_V2_EBUSY	-12
#define QCOM_SCM_ENOMEM		-5
#define QCOM_SCM_EOPNOTSUPP	-4
#define QCOM_SCM_EINVAL_ADDR	-3
#define QCOM_SCM_EINVAL_ARG	-2
#define QCOM_SCM_ERROR		-1
#define QCOM_SCM_INTERRUPTED	1

static inline int qcom_scm_remap_error(int err)
{
	switch (err) {
	case QCOM_SCM_ERROR:
		return -EIO;
	case QCOM_SCM_EINVAL_ADDR:
	case QCOM_SCM_EINVAL_ARG:
		return -EINVAL;
	case QCOM_SCM_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case QCOM_SCM_ENOMEM:
		return -ENOMEM;
	case QCOM_SCM_V2_EBUSY:
		return -EBUSY;
	}
	return -EINVAL;
}

#endif
