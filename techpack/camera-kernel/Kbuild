# SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_QCOM_CAMERA_DEBUG), y)
$(info "CAMERA_KERNEL_ROOT is: $(CAMERA_KERNEL_ROOT)")
$(info "KERNEL_ROOT is: $(KERNEL_ROOT)")
endif

# Include Architecture configurations
ifeq ($(CONFIG_ARCH_KALAMA), y)
include $(CAMERA_KERNEL_ROOT)/config/kalama.mk
endif

ifeq ($(CONFIG_ARCH_WAIPIO), y)
include $(CAMERA_KERNEL_ROOT)/config/waipio.mk
endif

ifeq ($(CONFIG_ARCH_LAHAINA), y)
include $(CAMERA_KERNEL_ROOT)/config/lahaina.mk
endif

ifeq ($(CONFIG_ARCH_KONA), y)
include $(CAMERA_KERNEL_ROOT)/config/kona.mk
endif

ifeq ($(CONFIG_ARCH_BENGAL), y)
include $(CAMERA_KERNEL_ROOT)/config/holi.mk
endif

ifeq ($(CONFIG_ARCH_HOLI), y)
include $(CAMERA_KERNEL_ROOT)/config/holi.mk
endif

ifeq ($(CONFIG_ARCH_LITO), y)
include $(CAMERA_KERNEL_ROOT)/config/lito.mk
endif

ifeq ($(CONFIG_ARCH_SHIMA), y)
include $(CAMERA_KERNEL_ROOT)/config/shima.mk
endif

ifeq ($(CONFIG_ARCH_DIWALI), y)
include $(CAMERA_KERNEL_ROOT)/config/diwali.mk
endif

ifeq ($(CONFIG_ARCH_CAPE), y)
include $(CAMERA_KERNEL_ROOT)/config/cape.mk
endif

ifeq ($(CONFIG_ARCH_PARROT), y)
include $(CAMERA_KERNEL_ROOT)/config/parrot.mk
endif

# List of all camera-kernel headers
cam_include_dirs := $(shell dirname `find $(CAMERA_KERNEL_ROOT) -name '*.h'` | uniq)

# Include UAPI headers
USERINCLUDE +=                              \
	-I$(CAMERA_KERNEL_ROOT)/include/uapi/
# Include Kernel headers
LINUXINCLUDE +=                                 \
	-I$(KERNEL_ROOT)                            \
	$(addprefix -I,$(cam_include_dirs))         \
	-I$(CAMERA_KERNEL_ROOT)/include/uapi/camera \
	-I$(CAMERA_KERNEL_ROOT)/
# Optional include directories
ccflags-$(CONFIG_MSM_GLOBAL_SYNX) += -I$(KERNEL_ROOT)/drivers/media/platform/msm/synx

# After creating lists, add content of 'ccflags-m' variable to 'ccflags-y' one.
ccflags-y += ${ccflags-m}
ccflags-y += -I$(srctree)/drivers/misc/hwid/

camera-y := \
	drivers/cam_req_mgr/cam_req_mgr_core.o \
	drivers/cam_req_mgr/cam_req_mgr_dev.o \
	drivers/cam_req_mgr/cam_req_mgr_util.o \
	drivers/cam_req_mgr/cam_mem_mgr.o \
	drivers/cam_req_mgr/cam_req_mgr_workq.o \
	drivers/cam_req_mgr/cam_req_mgr_timer.o \
	drivers/cam_req_mgr/cam_req_mgr_debug.o \
	drivers/cam_utils/cam_soc_util.o \
	drivers/cam_utils/cam_packet_util.o \
	drivers/cam_utils/cam_debug_util.o \
	drivers/cam_utils/cam_trace.o \
	drivers/cam_utils/cam_common_util.o \
	drivers/cam_utils/cam_compat.o \
	drivers/cam_core/cam_context.o \
	drivers/cam_core/cam_context_utils.o \
	drivers/cam_core/cam_node.o \
	drivers/cam_core/cam_subdev.o \
	drivers/cam_smmu/cam_smmu_api.o \
	drivers/cam_sync/cam_sync.o \
	drivers/cam_sync/cam_sync_util.o \
	drivers/cam_cpas/cpas_top/cam_cpastop_hw.o \
	drivers/cam_cpas/camss_top/cam_camsstop_hw.o \
	drivers/cam_cpas/cam_cpas_soc.o \
	drivers/cam_cpas/cam_cpas_intf.o \
	drivers/cam_cpas/cam_cpas_hw.o \
	drivers/cam_cdm/cam_cdm_soc.o \
	drivers/cam_cdm/cam_cdm_util.o \
	drivers/cam_cdm/cam_cdm_intf.o \
	drivers/cam_cdm/cam_cdm_core_common.o \
	drivers/cam_cdm/cam_cdm_virtual_core.o \
	drivers/cam_cdm/cam_cdm_hw_core.o

ifeq (,$(filter $(CONFIG_CAM_PRESIL),y m))
	camera-y += drivers/cam_presil/stub/cam_presil_hw_access_stub.o
	camera-y += drivers/cam_utils/cam_io_util.o
else
	camera-y += drivers/cam_presil/presil/cam_presil_io_util.o
	camera-y += drivers/cam_presil/presil/cam_presil_hw_access.o
	camera-y += drivers/cam_presil/presil_framework_dev/cam_presil_framework_dev.o
	ccflags-y += -DCONFIG_CAM_PRESIL=1
endif

camera-$(CONFIG_QCOM_CX_IPEAK) += drivers/cam_utils/cam_cx_ipeak.o
camera-$(CONFIG_QCOM_BUS_SCALING) += drivers/cam_utils/cam_soc_bus.o
camera-$(CONFIG_INTERCONNECT_QCOM) += drivers/cam_utils/cam_soc_icc.o

camera-$(CONFIG_SPECTRA_ISP) += \
	drivers/cam_isp/isp_hw_mgr/hw_utils/cam_tasklet_util.o \
	drivers/cam_isp/isp_hw_mgr/hw_utils/cam_isp_packet_parser.o \
	drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller/cam_irq_controller.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_soc.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_common.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver1.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_hw_ver2.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_mod.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw/cam_ife_csid_lite_mod.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_soc.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/cam_sfe_core.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_top/cam_sfe_top.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_rd.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/sfe_hw/sfe_bus/cam_sfe_bus_wr.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_soc.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/cam_vfe_core.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver2.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_rd_ver1.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/cam_vfe_bus_ver3.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver2.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_common.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver4.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver3.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_top_ver2.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver2.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_ver3.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_rdi.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_fe_ver1.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/cam_vfe_camif_lite_ver3.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe17x/cam_vfe.o \
	drivers/cam_isp/isp_hw_mgr/cam_isp_hw_mgr.o \
	drivers/cam_isp/isp_hw_mgr/cam_ife_hw_mgr.o \
	drivers/cam_isp/cam_isp_dev.o \
	drivers/cam_isp/cam_isp_context.o

camera-$(CONFIG_SPECTRA_ICP) += \
	drivers/cam_icp/icp_hw/icp_hw_mgr/cam_icp_hw_mgr.o \
	drivers/cam_icp/icp_hw/ipe_hw/ipe_dev.o \
	drivers/cam_icp/icp_hw/ipe_hw/ipe_core.o \
	drivers/cam_icp/icp_hw/ipe_hw/ipe_soc.o \
	drivers/cam_icp/icp_hw/a5_hw/a5_dev.o \
	drivers/cam_icp/icp_hw/a5_hw/a5_core.o \
	drivers/cam_icp/icp_hw/a5_hw/a5_soc.o \
	drivers/cam_icp/icp_hw/lx7_hw/lx7_dev.o \
	drivers/cam_icp/icp_hw/lx7_hw/lx7_core.o \
	drivers/cam_icp/icp_hw/lx7_hw/lx7_soc.o \
	drivers/cam_icp/icp_hw/bps_hw/bps_dev.o \
	drivers/cam_icp/icp_hw/bps_hw/bps_core.o \
	drivers/cam_icp/icp_hw/bps_hw/bps_soc.o \
	drivers/cam_icp/cam_icp_subdev.o \
	drivers/cam_icp/cam_icp_context.o \
	drivers/cam_icp/hfi.o \
	drivers/cam_icp/utils/cam_icp_utils.o

camera-$(CONFIG_SPECTRA_JPEG) += \
	drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_dev.o \
	drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_core.o \
	drivers/cam_jpeg/jpeg_hw/jpeg_enc_hw/jpeg_enc_soc.o \
	drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_dev.o \
	drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_core.o \
	drivers/cam_jpeg/jpeg_hw/jpeg_dma_hw/jpeg_dma_soc.o \
	drivers/cam_jpeg/jpeg_hw/cam_jpeg_hw_mgr.o \
	drivers/cam_jpeg/cam_jpeg_dev.o \
	drivers/cam_jpeg/cam_jpeg_context.o

camera-$(CONFIG_SPECTRA_FD) += \
	drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_dev.o \
	drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_core.o \
	drivers/cam_fd/fd_hw_mgr/fd_hw/cam_fd_hw_soc.o \
	drivers/cam_fd/fd_hw_mgr/cam_fd_hw_mgr.o \
	drivers/cam_fd/cam_fd_dev.o \
	drivers/cam_fd/cam_fd_context.o

camera-$(CONFIG_SPECTRA_LRME) += \
	drivers/cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_dev.o \
	drivers/cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_core.o \
	drivers/cam_lrme/lrme_hw_mgr/lrme_hw/cam_lrme_hw_soc.o \
	drivers/cam_lrme/lrme_hw_mgr/cam_lrme_hw_mgr.o \
	drivers/cam_lrme/cam_lrme_dev.o \
	drivers/cam_lrme/cam_lrme_context.o

camera-$(CONFIG_SPECTRA_SENSOR) += \
	drivers/cam_sensor_module/cam_actuator/cam_actuator_dev.o \
	drivers/cam_sensor_module/cam_actuator/cam_actuator_core.o \
	drivers/cam_sensor_module/cam_actuator/cam_actuator_soc.o \
	drivers/cam_sensor_module/cam_actuator/cam_actuator_parklens_thread.o \
	drivers/cam_sensor_module/cam_cci/cam_cci_dev.o \
	drivers/cam_sensor_module/cam_cci/cam_cci_core.o \
	drivers/cam_sensor_module/cam_cci/cam_cci_soc.o \
	drivers/cam_sensor_module/cam_tpg/cam_tpg_dev.o \
	drivers/cam_sensor_module/cam_tpg/cam_tpg_core.o \
	drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw.o \
	drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_0/tpg_hw_v_1_0.o \
	drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_2/tpg_hw_v_1_2.o \
	drivers/cam_sensor_module/cam_tpg/tpg_hw/tpg_hw_v_1_3/tpg_hw_v_1_3.o \
	drivers/cam_sensor_module/cam_csiphy/cam_csiphy_soc.o \
	drivers/cam_sensor_module/cam_csiphy/cam_csiphy_dev.o \
	drivers/cam_sensor_module/cam_csiphy/cam_csiphy_core.o \
	drivers/cam_sensor_module/cam_eeprom/cam_eeprom_dev.o \
	drivers/cam_sensor_module/cam_eeprom/cam_eeprom_core.o  \
	drivers/cam_sensor_module/cam_eeprom/cam_eeprom_soc.o \
	drivers/cam_sensor_module/cam_ois/cam_ois_dev.o \
	drivers/cam_sensor_module/cam_ois/cam_ois_core.o \
	drivers/cam_sensor_module/cam_ois/cam_ois_soc.o \
	drivers/cam_sensor_module/cam_ois/sem1217s.o \
	drivers/cam_sensor_module/cam_ois/rumbas4h.o \
	drivers/cam_sensor_module/cam_sensor/cam_sensor_dev.o \
	drivers/cam_sensor_module/cam_sensor/cam_sensor_core.o \
	drivers/cam_sensor_module/cam_sensor/cam_sensor_soc.o \
	drivers/cam_sensor_module/cam_sensor_io/cam_sensor_io.o \
	drivers/cam_sensor_module/cam_sensor_io/cam_sensor_cci_i2c.o \
	drivers/cam_sensor_module/cam_sensor_io/cam_sensor_qup_i2c.o \
	drivers/cam_sensor_module/cam_sensor_io/cam_sensor_spi.o \
	drivers/cam_sensor_module/cam_sensor_utils/cam_sensor_util.o \
	drivers/cam_sensor_module/cam_res_mgr/cam_res_mgr.o \
	drivers/cam_sensor_module/cam_flash/cam_flash_dev.o \
	drivers/cam_sensor_module/cam_flash/cam_flash_core.o \
	drivers/cam_sensor_module/cam_flash/cam_flash_soc.o

ifneq (,$(filter $(CONFIG_ISPV3),y m))
camera-$(CONFIG_SPECTRA_SENSOR) += \
	drivers/cam_sensor_module/cam_ispv3/cam_ispv3_dev.o \
	drivers/cam_sensor_module/cam_ispv3/cam_ispv3_core.o
endif

camera-$(CONFIG_SPECTRA_CUSTOM) += \
	drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_soc.o \
	drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_dev.o \
	drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/cam_custom_sub_mod_core.o \
	drivers/cam_cust/cam_custom_hw_mgr/cam_custom_csid/cam_custom_csid_dev.o \
	drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw_mgr.o \
	drivers/cam_cust/cam_custom_dev.o \
	drivers/cam_cust/cam_custom_context.o

camera-$(CONFIG_SPECTRA_OPE) += \
	drivers/cam_ope/cam_ope_subdev.o \
	drivers/cam_ope/cam_ope_context.o \
	drivers/cam_ope/ope_hw_mgr/cam_ope_hw_mgr.o \
	drivers/cam_ope/ope_hw_mgr/ope_hw/ope_dev.o \
	drivers/cam_ope/ope_hw_mgr/ope_hw/ope_soc.o \
	drivers/cam_ope/ope_hw_mgr/ope_hw/ope_core.o \
	drivers/cam_ope/ope_hw_mgr/ope_hw/top/ope_top.o \
	drivers/cam_ope/ope_hw_mgr/ope_hw/bus_rd/ope_bus_rd.o\
	drivers/cam_ope/ope_hw_mgr/ope_hw/bus_wr/ope_bus_wr.o

camera-$(CONFIG_SPECTRA_CRE) += \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_core.o \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_soc.o \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/cre_dev.o \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/top/cre_top.o \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/bus_rd/cre_bus_rd.o \
	drivers/cam_cre/cam_cre_hw_mgr/cre_hw/bus_wr/cre_bus_wr.o \
	drivers/cam_cre/cam_cre_hw_mgr/cam_cre_hw_mgr.o \
	drivers/cam_cre/cam_cre_dev.o \
	drivers/cam_cre/cam_cre_context.o

camera-$(CONFIG_SPECTRA_TFE) += \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi_core.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/ppi_hw/cam_csid_ppi100.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_soc.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_core.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe_bus.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_hw/cam_tfe.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_dev.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_soc.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid_core.o \
	drivers/cam_isp/isp_hw_mgr/isp_hw/tfe_csid_hw/cam_tfe_csid.o \
	drivers/cam_isp/isp_hw_mgr/cam_tfe_hw_mgr.o

camera-y += drivers/camera_main.o

obj-m += camera.o
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/camera.ko

cameralog-y := drivers/cam_log/cam_log.o
obj-m+= cameralog.o
BOARD_VENDOR_KERNEL_MODULES +=  $(KERNEL_MODULES_OUT)/cameralog.ko
