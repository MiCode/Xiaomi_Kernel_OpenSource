/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015,2020-2021 The Linux Foundation. All rights reserved.
 */
#ifndef __QCOM_SCM_INT_H
#define __QCOM_SCM_INT_H

// SIP Services and Function IDs
#define QCOM_SCM_SVC_BOOT			0x01
#define QCOM_SCM_BOOT_SET_ADDR			0x01
#define QCOM_SCM_BOOT_TERMINATE_PC		0x02
#define QCOM_SCM_BOOT_SEC_WDOG_DIS		0x07
#define QCOM_SCM_BOOT_SEC_WDOG_TRIGGER		0x08
#define QCOM_SCM_BOOT_WDOG_DEBUG_PART		0x09
#define QCOM_SCM_BOOT_SET_REMOTE_STATE		0x0a
#define QCOM_SCM_BOOT_SPIN_CPU			0x0d
#define QCOM_SCM_BOOT_SWITCH_MODE		0x0f
#define QCOM_SCM_BOOT_SET_DLOAD_MODE		0x10
#define QCOM_SCM_BOOT_SET_ADDR_MC		0x11
#define QCOM_SCM_BOOT_CONFIG_CPU_ERRATA		0x12
#define QCOM_SCM_QUSB2PHY_LVL_SHIFTER_CMD_ID	0x1B
extern int __qcom_scm_set_cold_boot_addr(struct device *dev, void *entry,
		const cpumask_t *cpus);
extern int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
		const cpumask_t *cpus);
extern int __qcom_scm_set_warm_boot_addr_mc(struct device *dev, void *entry,
		u32 aff0, u32 aff1, u32 aff2, u32 flags);
extern void __qcom_scm_cpu_power_down(struct device *dev, u32 flags);
extern void __qcom_scm_cpu_hp(struct device *dev, u32 flags);
extern int __qcom_scm_sec_wdog_deactivate(struct device *dev);
extern int __qcom_scm_sec_wdog_trigger(struct device *dev);
#ifdef CONFIG_TLB_CONF_HANDLER
extern int __qcom_scm_tlb_conf_handler(struct device *dev, unsigned long addr);
#endif
extern void __qcom_scm_disable_sdi(struct device *dev);
extern int __qcom_scm_set_remote_state(struct device *dev, u32 state, u32 id);
extern int __qcom_scm_spin_cpu(struct device *dev);
extern int __qcom_scm_set_dload_mode(struct device *dev,
				     enum qcom_download_mode mode);
extern int __qcom_scm_config_cpu_errata(struct device *dev);
extern void __qcom_scm_phy_update_scm_level_shifter(struct device *dev, u32 val);
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
#define QCOM_SCM_MP_CP_FEAT_ID			0x0c

#define QCOM_SCM_SVC_PWR			0x09
#define QCOM_SCM_PWR_IO_DISABLE_PMIC_ARBITER	0x01
#define QCOM_SCM_PWR_IO_DEASSERT_PS_HOLD	0x02
#define QCOM_SCM_PWR_MMU_SYNC			0x08
extern void __qcom_scm_halt_spmi_pmic_arbiter(struct device *dev);
extern void __qcom_scm_deassert_ps_hold(struct device *dev);
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
#define QCOM_SCM_MP_CMD_SD_CTRL			0x18
#define QCOM_SCM_MP_CP_SMMU_APERTURE_ID		0x1b
#define QCOM_SCM_MEMP_SHM_BRIDGE_ENABLE		0x1c
#define QCOM_SCM_MEMP_SHM_BRIDGE_DELETE		0x1d
#define QCOM_SCM_MEMP_SHM_BRDIGE_CREATE		0x1e
#define QCOM_SCM_MP_SMMU_PREPARE_ATOS_ID	0x21
#define QCOM_SCM_MP_MPU_LOCK_NS_REGION		0x25
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
extern int __qcom_scm_mem_protect_sd_ctrl(struct device *dev, u32 devid,
				phys_addr_t mem_addr, u64 mem_size, u32 vmid);
extern int __qcom_scm_kgsl_set_smmu_aperture(struct device *dev,
						unsigned int num_context_bank);
extern int __qcom_scm_enable_shm_bridge(struct device *dev);
extern int __qcom_scm_delete_shm_bridge(struct device *dev, u64 handle);
extern int __qcom_scm_create_shm_bridge(struct device *dev,
			u64 pfn_and_ns_perm_flags, u64 ipfn_and_s_perm_flags,
			u64 size_and_flags, u64 ns_vmids, u64 *handle);
extern int __qcom_scm_smmu_prepare_atos_id(struct device *dev, u64 dev_id,
						int cb_num, int operation);
extern int __qcom_mdf_assign_memory_to_subsys(struct device *dev,
		u64 start_addr, u64 end_addr, phys_addr_t paddr, u64 size);
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

#define QCOM_SCM_SVC_LMH			0x13
#define QCOM_SCM_LMH_DEBUG_SET			0x08
#define QCOM_SCM_LMH_DEBUG_READ_BUF_SIZE	0x09
#define QCOM_SCM_LMH_LIMIT_DCVSH		0x10
#define QCOM_SCM_LMH_DEBUG_READ			0x0A
#define QCOM_SCM_LMH_DEBUG_GET_TYPE		0x0B
#define QCOM_SCM_LMH_DEBUG_FETCH_DATA		0x0D
extern int __qcom_scm_lmh_read_buf_size(struct device *dev, int *size);
extern int __qcom_scm_lmh_limit_dcvsh(struct device *dev, phys_addr_t payload,
			uint32_t payload_size, u64 limit_node, uint32_t node_id,
			u64 version);
extern int __qcom_scm_lmh_debug_read(struct device *dev, phys_addr_t payload,
					uint32_t size);
extern int __qcom_scm_lmh_debug_config_write(struct device *dev, u64 cmd_id,
			phys_addr_t payload, int payload_size, uint32_t *buf,
			int buf_size);
extern int __qcom_scm_lmh_get_type(struct device *dev, phys_addr_t payload,
			u64 payload_size, u64 debug_type, uint32_t get_from,
			uint32_t *size);
extern int __qcom_scm_lmh_fetch_data(struct device *dev,
		u32 node_id, u32 debug_type, uint32_t *peak, uint32_t *avg);

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
#define QCOM_SCM_CAMERA_PROTECT_ALL		0x06
#define QCOM_SCM_CAMERA_PROTECT_PHY_LANES	0x07
extern int __qcom_scm_camera_protect_all(struct device *dev, uint32_t protect,
						uint32_t param);
extern int __qcom_scm_camera_protect_phy_lanes(struct device *dev,
						bool protect, u64 regmask);

extern int __qcom_scm_qseecom_do(struct device *dev, u32 cmd_id,
				 struct scm_desc *desc, bool retry);

#define QCOM_SCM_SVC_TSENS		0x1E
#define QCOM_SCM_TSENS_INIT_ID		0x5
extern int __qcom_scm_tsens_reinit(struct device *dev, int *tsens_ret);

//SMMU Paravirt driver
#define SMMU_PARAVIRT_OP_ATTACH         0
#define SMMU_PARAVIRT_OP_DETACH		1
#define SMMU_PARAVIRT_OP_INVAL_ASID     2
#define SMMU_PARAVIRT_OP_INVAL_VA       3
#define SMMU_PARAVIRT_OP_ALL_S1_BYPASS  4
#define SMMU_PARAVIRT_OP_CFGI_CD_ALL    5
#define SMMU_PARAVIRT_OP_TLBI_NH_ALL    6

#define ARM_SMMU_PARAVIRT_CMD		0x6
#define ARM_SMMU_PARAVIRT_DESCARG	0x22200a

extern int __qcom_scm_paravirt_smmu_attach(struct device *dev, u64 sid,
				    u64 asid, u64 ste_pa, u64 ste_size,
				    u64 cd_pa, u64 cd_size);

extern int __qcom_scm_paravirt_tlb_inv(struct device *dev, u64 asid);

extern int __qcom_scm_paravirt_smmu_detach(struct device *dev,
						u64 sid);

// OEM Services and Function IDs
#define QCOM_SCM_SVC_OEM_POWER		0x09
#define QCOM_SCM_OEM_POWER_REBOOT	0x22
extern int __qcom_scm_reboot(struct device *dev);

// TOS Services and Function IDs
#define QCOM_SCM_SVC_QSEELOG		0x01
#define QCOM_SCM_QSEELOG_REGISTER	0x06
extern int __qcom_scm_register_qsee_log_buf(struct device *dev, phys_addr_t buf,
					   size_t len);
#define QCOM_SCM_FEAT_LOG_ID		0x0a
#define QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID	0x0b
extern int __qcom_scm_query_encrypted_log_feature(struct device *dev,
						u64 *enabled);
#define QCOM_SCM_REQUEST_ENCR_LOG_ID	0x0c
extern int __qcom_scm_request_encrypted_log(struct device *dev, phys_addr_t buf,
					   size_t len, uint32_t log_id);

#define QCOM_SCM_SVC_KEYSTORE		0x05
#define QCOM_SCM_ICE_RESTORE_KEY_ID	0x06
extern int __qcom_scm_ice_restore_cfg(struct device *dev);

#define QCOM_SCM_SVC_SMCINVOKE		0x06
#define QCOM_SCM_SMCINVOKE_INVOKE_LEGACY   0x00
#define QCOM_SCM_SMCINVOKE_INVOKE	0x02
#define QCOM_SCM_SMCINVOKE_CB_RSP	0x01
extern int __qcom_scm_invoke_smc_legacy(struct device *dev, phys_addr_t in_buf,
		size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size,
		int32_t *result, u64 *response_type, unsigned int *data);
extern int __qcom_scm_invoke_smc(struct device *dev, phys_addr_t in_buf,
		size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size,
		int32_t *result, u64 *response_type, unsigned int *data);
extern int __qcom_scm_invoke_callback_response(struct device *dev,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data);

extern void __qcom_scm_init(void);
extern int __qcom_scm_mem_protect_audio(struct device *dev, phys_addr_t paddr,
					size_t size);

#ifdef CONFIG_QCOM_RTIC

#define SCM_SVC_RTIC				0x19
extern int __init scm_mem_protection_init_do(struct device *dev);

#endif

#define TZ_SVC_BW_PROF_ID		0x07 /* ddr profiler */
extern int __qcom_scm_ddrbw_profiler(struct device *dev, phys_addr_t in_buf,
	size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size);

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
