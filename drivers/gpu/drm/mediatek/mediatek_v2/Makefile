subdir-ccflags-y += -Werror

mediatek-drm-y := mtk_drm_drv.o \
		  mtk_disp_tdshp.o \
		  mtk_disp_color.o \
		  mtk_disp_ccorr.o \
		  mtk_disp_c3d.o   \
		  mtk_disp_gamma.o \
		  mtk_disp_aal.o \
		  mtk_disp_dither.o \
		  mtk_disp_chist.o \
		  mtk_disp_ovl.o \
		  mtk_disp_rdma.o \
		  mtk_disp_rsz.o \
		  mtk_disp_wdma.o \
		  mtk_drm_crtc.o \
		  mtk_drm_ddp.o \
		  mtk_drm_ddp_addon.o \
		  mtk_drm_ddp_comp.o \
		  mtk_drm_fb.o \
		  mtk_drm_gem.o \
		  mtk_drm_plane.o \
		  mtk_dsi.o \
		  mtk_mipi_tx.o \
		  mtk_writeback.o \
		  mtk_fence.o \
		  mtk_drm_session.o \
		  mtk_dump.o \
		  mtk_debug.o \
		  mtk_layering_rule.o \
		  mtk_layering_rule_base.o \
		  mtk_rect.o \
		  mtk_drm_mmp.o \
		  mtk_drm_trace.o \
		  mtk_drm_helper.o \
		  mtk_drm_lowpower.o \
		  mtk_disp_postmask.o \
		  mtk_drm_assert.o \
		  mtk_drm_fbconsole.o \
		  mtk_disp_recovery.o \
		  mtk_disp_pmqos.o \
		  mtk_disp_dsc.o \
		  mtk_disp_cm.o \
		  mtk_disp_spr.o \
		  mtk_drm_arr.o \
		  mtk_disp_merge.o \
		  mtk_dmdp_aal.o \
		  mtk_fbconfig_kdebug.o \
		  mtk_disp_y2r0.o \
		  mtk_disp_dlo_async.o \
		  mtk_disp_dli_async.o \
		  mtk_disp_inlinerotate.o \
		  mtk_mmlsys_bypass_mode.o \
		  platform/mtk_drm_6789.o \

#ccflags-y += -DDRM_CMDQ_DISABLE
#mediatek-drm-y += mtk_cmdq_dummy.o

ifeq ($(CONFIG_DRM_MEDIATEK_DEBUG_FS),y)
mediatek-drm-y += mtk_drm_debugfs.o
endif

mediatek-drm-y += mtk_dp_intf.o \
	mtk_dp.o \
	mtk_dp_hal.o \
	mtk_dp_debug.o

ifeq (yy, $(CONFIG_MTK_TEE_GP_SUPPORT)$(CONFIG_TRUSTONIC_TEE_SUPPORT))
obj-y += ca/
ccflags-y += -DDPTX_HDCP_ENABLE
obj-y += mtk_dp_hdcp1x.o \
	mtk_dp_hdcp2.o
endif

mediatek-drm-$(CONFIG_DRM_FBDEV_EMULATION) += mtk_drm_fbdev.o

ifeq ($(CONFIG_MI_DISP), y)
include $(src)/mi_disp/Makefile
endif

obj-$(CONFIG_DRM_MEDIATEK) += mediatek-drm.o
obj-$(CONFIG_DRM_MEDIATEK) += mtk_sync.o

obj-$(CONFIG_DRM_MEDIATEK) += mtk_panel_ext.o
obj-$(CONFIG_DRM_MEDIATEK) += mtk_disp_notify.o
obj-$(CONFIG_DRM_MEDIATEK) += mtk_aod_scp.o

obj-$(CONFIG_DRM_MEDIATEK) += mtk_disp_sec.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(srctree)/drivers/misc/mediatek/smi/			\
             -I$(srctree)/drivers/misc/mediatek/mmp/			\
             -I$(srctree)/drivers/misc/mediatek/mmdvfs/			\
             -I$(srctree)/drivers/misc/mediatek/pmic/			\
             -I$(srctree)/drivers/iommu/				\
             -I$(srctree)/drivers/devfreq/				\
             -I$(srctree)/drivers/gpu/drm/				\
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(srctree)/drivers/misc/mediatek/leds/ \
             -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/        \
             -I$(srctree)/drivers/misc/mediatek/iommu/		\
             -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/        \
             -I$(srctree)/drivers/gpu/drm/mediatek \
             -I$(srctree)/drivers/misc/mediatek/slbc/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(srctree)/drivers/misc/mediatek/clkbuf/v1/inc/ \
             -I$(srctree)/drivers/dma-buf/heaps/ \

ccflags-$(CONFIG_MTK_CMDQ_MBOX_EXT) += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox

#ccflags-y += -DDRM_MMPATH
ccflags-y += -DNOT_SUPPORT_CABC_HW
ccflags-y += -DDISP_MDP_COLOR_ON
ccflags-y += -DCCORR_SUPPORT


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mtk-interconnect/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
subdir-ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
