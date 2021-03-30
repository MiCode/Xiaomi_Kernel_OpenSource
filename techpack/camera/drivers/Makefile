# SPDX-License-Identifier: GPL-2.0-only

cameratree := $(srctree)/techpack/camera/drivers

cam_include_dirs := $(shell dirname `find $(cameratree) -name '*.h'` | uniq)
ccflags-y += $(addprefix -I,$(cam_include_dirs))

ifneq (,$(filter $(CONFIG_MSM_GLOBAL_SYNX), y m))
ccflags-y += -I$(srctree)/drivers/media/platform/msm/synx
endif

ccflags-y += -I$(srctree)/techpack/camera/include/uapi/camera
ccflags-y += -I$(srctree)

camera-y := \
	cam_req_mgr/cam_req_mgr_core.o \
	cam_req_mgr/cam_req_mgr_dev.o \
	cam_req_mgr/cam_req_mgr_util.o \
	cam_req_mgr/cam_mem_mgr.o \
	cam_req_mgr/cam_req_mgr_workq.o \
	cam_req_mgr/cam_req_mgr_timer.o \
	cam_req_mgr/cam_req_mgr_debug.o \
	cam_utils/cam_soc_util.o \
	cam_utils/cam_io_util.o \
	cam_utils/cam_packet_util.o \
	cam_utils/cam_debug_util.o \
	cam_utils/cam_trace.o \
	cam_utils/cam_common_util.o \
	cam_utils/cam_compat.o \
	cam_core/cam_context.o \
	cam_core/cam_context_utils.o \
	cam_core/cam_node.o \
	cam_core/cam_subdev.o \
	cam_smmu/cam_smmu_api.o \
	cam_sync/cam_sync.o \
	cam_sync/cam_sync_util.o \
	cam_cpas/cpas_top/cam_cpastop_hw.o \
	cam_cpas/camss_top/cam_camsstop_hw.o \
	cam_cpas/cam_cpas_soc.o \
	cam_cpas/cam_cpas_intf.o \
	cam_cpas/cam_cpas_hw.o \
	cam_cdm/cam_cdm_soc.o \
	cam_cdm/cam_cdm_util.o \
	cam_cdm/cam_cdm_intf.o \
	cam_cdm/cam_cdm_core_common.o \
	cam_cdm/cam_cdm_virtual_core.o \
	cam_cdm/cam_cdm_hw_core.o

ifneq (,$(filter $(CONFIG_QCOM_CX_IPEAK),y m))
camera-y += cam_utils/cam_cx_ipeak.o
endif

ifneq (,$(filter $(CONFIG_QCOM_BUS_SCALING),y m))
camera-y += cam_utils/cam_soc_bus.o
endif

ifneq (,$(filter $(CONFIG_INTERCONNECT_QCOM),y m))
camera-y += cam_utils/cam_soc_icc.o
endif

camera-$(CONFIG_SPECTRA_ISP) += \
	cam_isp/isp_hw_mgr/hw_utils/cam_tasklet_util.o \
	cam_isp/isp_hw_mgr/hw_utils/cam_isp_packet_parser.o \
	cam_isp/isp_hw_mgr/hw_utils/irq_controller/cam_irq_controller.o \
	cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_core.o \
	cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid17x.o \
	cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_lite17x.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_core.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver2.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_rd_ver1.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver3.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver2.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_common.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver3.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver2.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver2.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver3.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_rdi.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_fe_ver1.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver3.o \
	cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe17x/cam_vfe.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_core.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_top/cam_sfe_top.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_rd.o \
	cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_wr.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_core.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_ver1.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_ver2.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg_ver3.o \
	cam_isp/isp_hw_mgr/isp_hw/top_tpg/cam_top_tpg.o \
	cam_isp/isp_hw_mgr/cam_isp_hw_mgr.o \
	cam_isp/isp_hw_mgr/cam_ife_hw_mgr.o \
	cam_isp/cam_isp_dev.o \
	cam_isp/cam_isp_context.o

camera-$(CONFIG_SPECTRA_ICP) += \
	cam_icp/icp_hw/icp_hw_mgr/cam_icp_hw_mgr.o \
	cam_icp/icp_hw/ipe_hw/ipe_dev.o \
	cam_icp/icp_hw/ipe_hw/ipe_core.o \
	cam_icp/icp_hw/ipe_hw/ipe_soc.o \
	cam_icp/icp_hw/a5_hw/a5_dev.o \
	cam_icp/icp_hw/a5_hw/a5_core.o \
	cam_icp/icp_hw/a5_hw/a5_soc.o \
	cam_icp/icp_hw/bps_hw/bps_dev.o \
	cam_icp/icp_hw/bps_hw/bps_core.o \
	cam_icp/icp_hw/bps_hw/bps_soc.o \
	cam_icp/cam_icp_subdev.o \
	cam_icp/cam_icp_context.o \
	cam_icp/hfi.o

camera-$(CONFIG_SPECTRA_JPEG) += \
	cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_dev.o \
	cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_core.o \
	cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_soc.o \
	cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_dev.o \
	cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_core.o \
	cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_soc.o \
	cam_jpeg/jpeg_hw/cam_jpeg_hw_mgr.o \
	cam_jpeg/cam_jpeg_dev.o \
	cam_jpeg/cam_jpeg_context.o

camera-$(CONFIG_SPECTRA_FD) += \
	cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_dev.o \
	cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_core.o \
	cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_soc.o \
	cam_fd/fd_hw_mgr/cam_fd_hw_mgr.o \
	cam_fd/cam_fd_dev.o \
	cam_fd/cam_fd_context.o

camera-$(CONFIG_SPECTRA_LRME) += \
	cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_dev.o \
	cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_core.o \
	cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_soc.o \
	cam_lrme/lrme_hw_mgr/cam_lrme_hw_mgr.o \
	cam_lrme/cam_lrme_dev.o \
	cam_lrme/cam_lrme_context.o

camera-$(CONFIG_SPECTRA_SENSOR) += \
	cam_sensor_module/cam_actuator/cam_actuator_dev.o \
	cam_sensor_module/cam_actuator/cam_actuator_core.o \
	cam_sensor_module/cam_actuator/cam_actuator_soc.o \
	cam_sensor_module/cam_cci/cam_cci_dev.o \
	cam_sensor_module/cam_cci/cam_cci_core.o \
	cam_sensor_module/cam_cci/cam_cci_soc.o \
	cam_sensor_module/cam_csiphy/cam_csiphy_soc.o \
	cam_sensor_module/cam_csiphy/cam_csiphy_dev.o \
	cam_sensor_module/cam_csiphy/cam_csiphy_core.o \
	cam_sensor_module/cam_eeprom/cam_eeprom_dev.o \
	cam_sensor_module/cam_eeprom/cam_eeprom_core.o  \
	cam_sensor_module/cam_eeprom/cam_eeprom_soc.o \
	cam_sensor_module/cam_ois/cam_ois_dev.o \
	cam_sensor_module/cam_ois/cam_ois_core.o \
	cam_sensor_module/cam_ois/cam_ois_soc.o \
	cam_sensor_module/cam_ois/Sem1215.o \
	cam_sensor_module/cam_ois/Lc898128.o \
	cam_sensor_module/cam_sensor/cam_sensor_dev.o \
	cam_sensor_module/cam_sensor/cam_sensor_core.o \
	cam_sensor_module/cam_sensor/cam_sensor_soc.o \
	cam_sensor_module/cam_sensor_io/cam_sensor_io.o \
	cam_sensor_module/cam_sensor_io/cam_sensor_cci_i2c.o \
	cam_sensor_module/cam_sensor_io/cam_sensor_qup_i2c.o \
	cam_sensor_module/cam_sensor_io/cam_sensor_spi.o \
	cam_sensor_module/cam_sensor_utils/cam_sensor_util.o \
	cam_sensor_module/cam_res_mgr/cam_res_mgr.o

camera-$(CONFIG_LEDS_QPNP_FLASH_V2) += \
	cam_sensor_module/cam_flash/cam_flash_dev.o \
	cam_sensor_module/cam_flash/cam_flash_core.o \
	cam_sensor_module/cam_flash/cam_flash_soc.o

ifneq (,$(filter $(CONFIG_LEDS_QTI_FLASH),y m))
camera-y += \
	cam_sensor_module/cam_flash/cam_flash_dev.o \
	cam_sensor_module/cam_flash/cam_flash_core.o \
	cam_sensor_module/cam_flash/cam_flash_soc.o
endif

camera-$(CONFIG_SPECTRA_CUSTOM) += \
	cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_soc.o \
	cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_dev.o \
	cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_core.o \
	cam_cust/cam_custom_hw_mgr/cam_custom_csid/cam_custom_csid_dev.o \
	cam_cust/cam_custom_hw_mgr/cam_custom_hw_mgr.o \
	cam_cust/cam_custom_dev.o \
	cam_cust/cam_custom_context.o

camera-$(CONFIG_SPECTRA_OPE) += \
	cam_ope/cam_ope_subdev.o \
	cam_ope/cam_ope_context.o \
	cam_ope/ope_hw_mgr/cam_ope_hw_mgr.o \
	cam_ope/ope_hw_mgr/ope_hw/ope_dev.o \
	cam_ope/ope_hw_mgr/ope_hw/ope_soc.o \
	cam_ope/ope_hw_mgr/ope_hw/ope_core.o \
	cam_ope/ope_hw_mgr/ope_hw/top/ope_top.o \
	cam_ope/ope_hw_mgr/ope_hw/bus_rd/ope_bus_rd.o\
	cam_ope/ope_hw_mgr/ope_hw/bus_wr/ope_bus_wr.o

camera-$(CONFIG_SPECTRA_TFE) += \
	cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi_core.o \
	cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi100.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_core.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_bus.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_dev.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_soc.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_core.o \
	cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid530.o \
	cam_isp/isp_hw_mgr/cam_tfe_hw_mgr.o


camera-y += camera_main.o

obj-$(CONFIG_SPECTRA_CAMERA) += camera.o

ifeq ($(CONFIG_SPECTRA_CAMERA),m)
$(info "SPECTRA CAMERA IS BUILT AS DLKM")
BOARD_VENDOR_KERNEL_MODULES +=  $(KERNEL_MODULES_OUT)/camera.ko
endif
