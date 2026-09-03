# SPDX-License-Identifier: GPL-2.0

ccflags-y := -I $(srctree)/$(src)
subdir-ccflags-y += -I $(srctree)/$(src)/../../../devfreq/apsys/

sprd-drm-y := \
	sprd_drm.o \
	sprd_gem.o \
	sprd_crtc.o \
	sprd_plane.o \
	disp_lib.o

sprd-drm-$(CONFIG_TRUSTY_VIRTIO_IPC) += disp_trusty.o

sprd-drm-$(CONFIG_DRM_SPRD_DUMMY) += \
	dummy/dummy_crtc.o \
	dummy/dummy_connector.o

sprd-drm-$(CONFIG_DRM_SPRD_DPU0) += \
	sprd_dpu.o \
	sprd_bl.o \
	sprd_corner.o \
	dpu/dpu_lite_r1p0.o \
	dpu/dpu_lite_r2p0.o \
	dpu/dpu_r2p0.o \
	dpu/dpu_r4p0.o \
	dpu/dpu_r5p0.o \
	dpu/dpu_r6p0.o

sprd-drm-$(CONFIG_DRM_SPRD_DSI) += \
	sprd_dsi.o \
	sprd_dphy.o \
	sprd_dsi_panel.o \
	dsi/sprd_dsi_api.o \
	dsi/core/dsi_ctrl_r1p0.o \
	dsi/core/dsi_ctrl_r1p0_ppi.o \
	dphy/sprd_dphy_api.o \
	dphy/pll/megacores_sharkle.o \
	dphy/pll/megacores_sharkl5.o

sprd-drm-y += \
	pike2/global_dpu.o \
	pike2/global_dsi.o \
	pike2/global_dphy.o \
	sharkl5/global_dispc.o \
	sharkl5/global_dsi.o \
	sharkl5/global_dphy.o \
	sharkle/global_dpu.o \
	sharkle/global_dsi.o \
	sharkle/global_dphy.o \
	sharkl3/global_dispc.o \
	sharkl3/global_dsi.o \
	sharkl3/global_dphy.o \
	sharkl5pro/global_dpu.o \
	sharkl5pro/global_dsi.o \
	sharkl5pro/global_dphy.o \
	qogirl6/global_dpu.o \
	qogirl6/global_dsi.o \
	qogirl6/global_dphy.o \
	qogirn6pro/global_dpu.o \
	qogirn6pro/global_dsi.o \
	qogirn6pro/global_dphy.o

sprd-drm-y += \
	sysfs/sysfs_class.o \
	sysfs/sysfs_dpu.o \
	sysfs/sysfs_dphy.o \
	sysfs/sysfs_dsi.o \
	sysfs/sysfs_panel.o \
	sysfs/sysfs_backlight.o

obj-$(CONFIG_UNISOC_DISP) += sprd-drm.o

obj-$(CONFIG_UNISOC_GSP) += gsp/

#sprd-drm-$(CONFIG_COMPAT) += sprd_ioc32.o
