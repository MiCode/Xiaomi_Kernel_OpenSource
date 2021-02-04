obj-$(CONFIG_SPECTRA_CAMERA) += cam_req_mgr/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_hyp_intf/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_utils/
#sdg add arcsoft Calibration 2018.12.07
ifeq ($(VERSION_TYPE),longcheer_factory)
subdir-ccflags-$(CONFIG_SPECTRA_CAMERA) += -DLC_ARCSOFT_CALIBRATION
endif
#end sdg
obj-$(CONFIG_SPECTRA_CAMERA) += cam_core/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_sync/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_smmu/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cpas/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cdm/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_isp/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_sensor_module/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_icp/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_jpeg/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_fd/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_lrme/
