export MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)

obj-$(CONFIG_MTK_MET) += met_drv/
obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_PWM)	    += pwm/
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT) += irtx/
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp/
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi/
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp/
ifeq ($(subst ",,$(CONFIG_MTK_SENSOR_ARCHITECTURE)), 1.0)
obj-y += sensors-1.0/
endif

obj-$(CONFIG_MTK_CCCI_DEVICES)	+=  ccci_util/
obj-$(CONFIG_MTK_NET_CCMNI) +=  ccmni/
obj-$(CONFIG_MTK_DRAMC_LEGACY) += dramc/
obj-$(CONFIG_MTK_ECCCI_DRIVER) +=  eccci/
obj-$(CONFIG_MTK_EMI_LEGACY) += emi/
obj-$(CONFIG_MMPROFILE) += mmp/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += trusted_mem/
obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += geniezone/
obj-$(CONFIG_MTK_TEE_SANITY) += tee_sanity/
obj-$(CONFIG_MTK_CMDQ) += cmdq/
obj-$(CONFIG_MTK_LCM) += lcm/
obj-$(CONFIG_MTK_LCM) += ext_disp/
obj-$(CONFIG_MTK_FB) += video/
obj-$(CONFIG_MTK_SYNC) += sync/
obj-${CONFIG_MTK_ATF_LOGGER} += atf/
obj-$(CONFIG_MTK_AMMS) += memory-amms/
obj-$(CONFIG_MTPROF) += mtprof/
obj-${CONFIG_MTK_ICE_DEBUG} += ice_debug/
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm/
obj-$(CONFIG_MTK_SPMTWAM) += twam/
obj-$(CONFIG_MTK_DVFSRC) += dvfsrc/
obj-$(CONFIG_MTK_AEE_FEATURE) += aee/
obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf/
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += qos/
obj-$(CONFIG_MTK_CM_MGR) += cm_mgr/
ifneq ($(CONFIG_MTK_BASE_POWER), y)
obj-$(CONFIG_MTK_PPM) += ppm_v3/
endif
ifneq (y, $(filter y, $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6779)))
obj-$(CONFIG_MTK_PPM) += ppm_v3/
endif

obj-$(CONFIG_MTK_SRCLKEN_RC) += srclken_rc/
obj-$(CONFIG_MTK_COMBO) += connectivity/
obj-$(CONFIG_MTK_BTIF) += btif/
obj-$(CONFIG_MTK_CONN_MD) += conn_md/
ifneq ($(CONFIG_MACH_MT6779), y)
obj-$(CONFIG_MTK_DCM_COMMON) += dcm/
endif
obj-$(CONFIG_MTK_SMI) += smi/
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += lpm/
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += performance/
obj-y += mmdvfs/
obj-$(CONFIG_MTK_IOMMU_MISC) += iommu/
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON) += mmqos/
obj-$(CONFIG_MTK_PERF_COMMON) += perf_common/
obj-$(CONFIG_MTK_SYSTRACKER_V2) += systracker/
obj-$(CONFIG_MTK_HANG_DETECT) += monitor_hang/
obj-$(CONFIG_MTK_POWER_THROTTLING_COMMON) += power_throttling/

obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal/
obj-$(CONFIG_MTK_PLAT_SRAM_FLAG) += debug_latch/
obj-$(CONFIG_MEDIATEK_CACHE_API) += cache/

ifneq ($(CONFIG_MTK_BASE_POWER), y)
obj-$(CONFIG_MTK_STATIC_POWER) += leakage_table_v2/
obj-$(CONFIG_MTK_UNIFY_POWER) += upower_v2/
endif

obj-$(CONFIG_PRINTK_MTK) += mtprintk/
obj-$(CONFIG_MTK_LEDS) += leds/
obj-$(CONFIG_MT6360_DBG) += subpmic/
obj-$(CONFIG_MTK_CAMERA_ISP) += cameraisp/
obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor/src/
ifdef CONFIG_MTK_V4L2_LENS
obj-$(CONFIG_MTK_V4L2_LENS) += lens/
else
obj-$(CONFIG_MTK_LENS)      += lens/
endif
obj-$(CONFIG_MTK_CAM_CAL)   += cam_cal/
obj-$(CONFIG_NFC_CHIP_SUPPORT) += nfc/
obj-$(CONFIG_MTK_M4U) += m4u/
obj-y += pmic/
obj-$(CONFIG_RT_REGMAP) += rt-regmap/
obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += videocodec/
obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_RT_FLASHLIGHT)     += flashlight/richtek/
obj-$(CONFIG_USB) += usb_boost/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
obj-$(CONFIG_EXTCON_MTK_USB) += extcon/
obj-$(CONFIG_MTK_DRAM_LOG_STORE) += log_store/
obj-$(CONFIG_MTK_ECCCI_C2K) += c2k_usb/
obj-$(CONFIG_MTK_USB_TYPEC) += typec/
obj-$(CONFIG_MTK_MDLA_SUPPORT) += mdla/
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu/
obj-$(CONFIG_MTK_CCU)	+= ccu/src/
obj-$(CONFIG_MTK_BLOCK_TAG) += blocktag/
obj-$(CONFIG_MTK_PID_MAP) += pidmap/
obj-$(CONFIG_MTK_JPEG) += jpeg/
ifneq ($(wildcard $(srctree)/../vendor/mediatek/internal/selinux_warning_enable),)
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING)	+= selinux_warning/
endif
obj-y += timer/
obj-$(CONFIG_MTK_DBGTOP) += dbgtop/
obj-$(CONFIG_MTK_DFD_INTERNAL_DUMP) += dfd/
