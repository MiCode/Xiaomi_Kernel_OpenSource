export MTK_PLATFORM := $(subst $(quote),,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y := -Werror $(call cc-disable-warning, unused-but-set-variable)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)

ifeq ($(CONFIG_MACH_MT6768),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6739),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6781),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6877),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6833),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6885),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else ifeq ($(CONFIG_MACH_MT6893),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/v1
else
obj-$(CONFIG_MTK_POWER_THROTTLING_COMMON) += power_throttling/
endif

obj-$(CONFIG_MTK_RTC)	+= rtc/
obj-$(CONFIG_MTK_ACCDET) += accdet/
obj-$(CONFIG_MTK_AUXADC) += auxadc/
obj-$(CONFIG_MTK_PMIC_NEW_ARCH)	+= pmic/
obj-$(CONFIG_MTK_PMIC_WRAP_HAL) += pmic_wrap/
obj-y += devinfo/
obj-y += subpmic/
obj-$(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) += mcupm/
obj-$(CONFIG_MTK_MET) += met_drv/
obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_SERIAL)	+= uart/
obj-$(CONFIG_MTK_PWM)	    += pwm/
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT) += irtx/
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp/
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi/
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp/
ifeq ($(subst $(quote),,$(CONFIG_MTK_SENSOR_ARCHITECTURE)), 1.0)
obj-y += sensors-1.0/
else
obj-y += sensor/
endif

obj-$(CONFIG_MTK_CCCI_DEVICES)	+=  ccci_util/
obj-$(CONFIG_MTK_NET_CCMNI) +=  ccmni/

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6739" "mt6785"))
obj-$(CONFIG_MTK_DRAMC) += dramc/
else
obj-$(CONFIG_MTK_DRAMC_LEGACY) += dramc/
endif

obj-$(CONFIG_MTK_ECCCI_DRIVER) +=  eccci/
obj-$(CONFIG_MTK_EMI_LEGACY) += emi/
obj-$(CONFIG_MTK_EMI_BWL)	+= emi_bwl/
obj-$(CONFIG_MTK_EMI_MPU) += emi_mpu/
obj-$(CONFIG_MMPROFILE) += mmp/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += trusted_mem/
obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += geniezone/
obj-$(CONFIG_MTK_TEE_SANITY) += tee_sanity/
obj-$(CONFIG_MTK_CMDQ) += cmdq/
obj-$(CONFIG_MTK_CMDQ_TAB) += cmdq/
obj-$(CONFIG_MTK_CMDQ) += mdp/
obj-$(CONFIG_MTK_CMDQ_TAB) += mdp/
obj-$(CONFIG_MTK_VDEC_FMT) += vdec_fmt/
obj-$(CONFIG_MTK_LCM) += lcm/
obj-$(CONFIG_MTK_LCM) += ext_disp/
ifeq (,$(filter $(CONFIG_MTK_PLATFORM_NAME), "mt6768" "mt6785"))
obj-y += spi_slave_drv/
endif
obj-$(CONFIG_MTK_MT6382_BDG) += hifi4dsp_spi/
obj-$(CONFIG_MTK_FB) += video/
obj-$(CONFIG_MTK_SYNC) += sync/
obj-${CONFIG_MTK_ATF_LOGGER} += atf/
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apusys/
obj-$(CONFIG_MTK_AMMS) += memory-amms/
obj-$(CONFIG_MTPROF) += mtprof/
obj-${CONFIG_MTK_ICE_DEBUG} += ice_debug/
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm/
obj-$(CONFIG_MTK_SPMTWAM) += twam/
obj-$(CONFIG_MTK_DVFSRC) += dvfsrc/
obj-$(CONFIG_MTK_AEE_FEATURE) += aee/
obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf/
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += qos/
obj-$(CONFIG_MTK_SLBC) += slbc/
ifneq ($(CONFIG_MACH_MT6893), y)
ifneq ($(CONFIG_MACH_MT6771), y)
obj-$(CONFIG_MTK_CM_MGR) += cm_mgr/
endif
endif
ifneq ($(CONFIG_MTK_BASE_POWER), y)
obj-$(CONFIG_MTK_PPM) += ppm_v3/
endif
ifneq (y, $(filter y, $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6779)))
obj-$(CONFIG_MTK_PPM) += ppm_v3/
endif

obj-$(CONFIG_MTK_CQDMA) += cqdma/

obj-$(CONFIG_MTK_SRCLKEN_RC) += srclken_rc/
obj-$(CONFIG_MTK_COMBO) += connectivity/
obj-$(CONFIG_MTK_BTIF) += btif/
obj-$(CONFIG_MTK_CONN_MD) += conn_md/
ifneq ($(CONFIG_MACH_MT6779), y)
obj-$(CONFIG_MTK_DCM_COMMON) += dcm/
endif
obj-$(CONFIG_MTK_SMI) += smi/
obj-$(CONFIG_MTK_SMI_VARIANT) += smi/
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += lpm/
#ifneq ($(CONFIG_MTK_PLATFORM), "mt6771")
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += performance/
#endif
obj-y += mmdvfs/
obj-$(CONFIG_MTK_IOMMU_MISC) += iommu/
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON) += mmqos/
obj-$(CONFIG_MTK_PERF_COMMON) += perf_common/
obj-$(CONFIG_MTK_SYSTRACKER_V2) += systracker/
obj-$(CONFIG_MTK_HANG_DETECT) += monitor_hang/
ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
obj-y += power/
else
obj-$(CONFIG_MACH_MT8167)+= power/
endif

obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal/
#obj-$(CONFIG_THERMAL) += thermalv1/
obj-$(CONFIG_MTK_PLAT_SRAM_FLAG) += debug_latch/
obj-$(CONFIG_MTK_DEBUG_TRACER)	+= debug_tracer/
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
obj-$(CONFIG_MTK_PSEUDO_M4U)   += pseudo_m4u/
obj-y += pmic/
obj-$(CONFIG_RT_REGMAP) += rt-regmap/
obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += videocodec/
obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_RT_FLASHLIGHT)     += flashlight/richtek/
obj-$(CONFIG_USB) += usb_boost/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
ifeq ($(CONFIG_MACH_MT6781),y)
obj-$(CONFIG_MTK_MUSB_PHY) += usb20/mt6781
else ifeq ($(CONFIG_MACH_MT6761),y)
obj-$(CONFIG_MTK_MUSB_PHY) += usb20/mt6765
else
obj-$(CONFIG_MTK_MUSB_PHY) += usb20/$(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
obj-$(CONFIG_EXTCON_MTK_USB) += extcon/
obj-$(CONFIG_MTK_DRAM_LOG_STORE) += log_store/
obj-$(CONFIG_MTK_ECCCI_C2K) += c2k_usb/
obj-$(CONFIG_MTK_USB_TYPEC) += typec/
obj-$(CONFIG_MTK_MDLA_SUPPORT) += mdla/
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu/
obj-$(CONFIG_MTK_CCU)	+= ccu/src/
obj-$(CONFIG_MTK_BLOCK_TAG) += blocktag/
obj-$(CONFIG_MTK_VIBRATOR) += vibrator/
obj-$(CONFIG_MTK_PID_MAP) += pidmap/
obj-$(CONFIG_MTK_JPEG) += jpeg/
obj-$(CONFIG_MTK_VOW_SUPPORT)  += vow/
ifneq ($(wildcard $(srctree)/../vendor/mediatek/internal/selinux_warning_enable),)
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING)	+= selinux_warning/
endif
ifeq ($(CONFIG_MACH_MT6833),y)
obj-$(CONFIG_THERMAL)   += thermal/
endif
obj-y += timer/
obj-$(CONFIG_MTK_DBGTOP) += dbgtop/
obj-$(CONFIG_MTK_DFD_INTERNAL_DUMP) += dfd/
obj-$(CONFIG_MTK_GIC_EXT) += ext_gic/
obj-$(CONFIG_MTK_SYS_CIRQ) += cirq/
obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) += masp/
obj-y += rps/
ifeq ($(CONFIG_MACH_MT6768),y)
obj-y += spi_slave_drv/
endif
obj-$(CONFIG_MTK_MT6382_BDG) += spi_slave_drv/
obj-$(CONFIG_MTK_SDA) += sda/
obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp/
obj-$(CONFIG_MTK_GATE_IC) += gate_ic/
obj-$(CONFIG_MTK_FREQ_HOPPING) += freqhopping/
obj-$(CONFIG_MTK_TASK_TURBO) += task_turbo/
