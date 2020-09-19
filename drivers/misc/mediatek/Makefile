#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#$(call all-subdir-src-or-makefile)
export MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -I$(srctree)/drivers/mmc/host/mediatek/$(MTK_PLATFORM)

obj-$(CONFIG_MTK_L3C_PART) += l3c_part/
obj-$(CONFIG_MTK_EMI) += emi/
obj-$(CONFIG_MTK_EMI_BWL)	+= emi_bwl/
obj-$(CONFIG_MTK_EMI_MPU) += emi_mpu/
obj-$(CONFIG_MMPROFILE) += mmp/
obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_SERIAL)	+= uart/
obj-$(CONFIG_MTK_PMIC_NEW_ARCH)	+= pmic/
obj-$(CONFIG_MTK_M4U)   += m4u/
obj-$(CONFIG_MTK_PSEUDO_M4U)   += pseudo_m4u/
obj-$(CONFIG_MTK_ICE_DEBUG) += ice_debug/
obj-y += mmdvfs/
obj-$(CONFIG_MTK_SYSTRACKER_V2)   += systracker/

obj-$(CONFIG_MTK_IRRX) += irrx/

obj-$(CONFIG_NFC_CHIP_SUPPORT) += nfc/

obj-$(CONFIG_MTK_COMBO) += connectivity/
obj-$(CONFIG_MTK_BTIF) += btif/
obj-$(CONFIG_MTK_CONN_MD) += conn_md/

obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) +=masp/

obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += gud/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += trusted_mem/
obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += geniezone/
obj-$(CONFIG_MTK_TEE_SANITY) += tee_sanity/
obj-y += atf/
obj-y += devinfo/
obj-y += sspm/
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apusys/
obj-$(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) += mcupm/
obj-y += timer/
obj-$(CONFIG_MTK_ACCDET) += accdet/
obj-$(CONFIG_MTK_DRAMC) += dramc/
obj-$(CONFIG_MTK_DRAMC_WA) += dramc/
obj-$(CONFIG_MTK_DBGTOP) += dbgtop/
obj-$(CONFIG_MTK_DEBUG_LATCH) += debug_latch/
obj-$(CONFIG_MTK_DEBUG_TRACER)	+= debug_tracer/
obj-$(CONFIG_MTK_GIC_EXT) += ext_gic/
obj-$(CONFIG_MTK_SYS_CIRQ) += cirq/
obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) +=masp/
obj-$(CONFIG_MTPROF)	+= mtprof/
obj-$(CONFIG_MEDIATEK_SOLUTION) += sched/
obj-$(CONFIG_MTK_FREQ_HOPPING) += freqhopping/
obj-$(CONFIG_MTK_GPU_SUPPORT) += gpu/
obj-y += cameraisp/
obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor/src/
obj-$(CONFIG_MTK_CAM_CAL) += cam_cal/src/
obj-$(CONFIG_MTK_LENS)   += lens/
obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_RT_FLASHLIGHT)     += flashlight/richtek/
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu/
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += mtee/
obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += gcpu/
obj-$(CONFIG_MTK_MLOG) += mlog/
obj-$(CONFIG_MTK_AUXADC)	+= auxadc/
obj-$(CONFIG_MTK_GPT_SCHEME_SUPPORT)	+= partition/
obj-$(CONFIG_MTK_PWM)	    += pwm/
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT)	    += irtx/
ifeq ($(subst ",,$(CONFIG_MTK_SENSOR_ARCHITECTURE)),1.0)
obj-y += sensors-1.0/
else
obj-y += sensor/
endif
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp/
obj-$(CONFIG_MTK_HIFIXDSP_SUPPORT) += hifi_dsp/
obj-$(CONFIG_MTK_HIFIXDSP_SUPPORT) += hifidsp_audio_ipi/
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp/
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi/
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += audio_scp/
obj-$(CONFIG_MTK_CCCI_DEVICES)	+=  ccci_util/
obj-$(CONFIG_MTK_NET_CCMNI) +=  ccmni/
obj-$(CONFIG_MTK_ECCCI_DRIVER) +=  eccci/
obj-$(CONFIG_MTK_SSMR) += memory-ssmr/
obj-y += cache-auditor/
obj-$(CONFIG_MTK_AMMS) += memory-amms/
obj-$(CONFIG_ZONE_MOVABLE_CMA) += zone_movable_cma/
obj-$(CONFIG_MTK_VIBRATOR) += vibrator/
obj-$(CONFIG_MTK_BLOCK_TAG) += blocktag/
obj-$(CONFIG_MTK_LEDS)	+= leds/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
obj-$(CONFIG_MTK_USBFSH) += usb11/
obj-$(CONFIG_MTK_BOOT) += boot/
obj-$(CONFIG_MTK_BOOT_REASON) += boot_reason/
obj-$(CONFIG_MTK_CHIP) += chip/
obj-$(CONFIG_MTK_DEVAPC) += devapc/
obj-$(CONFIG_MTK_SYNC) += sync/
obj-$(CONFIG_MICROTRUST_TEE_SUPPORT) += teei/
obj-$(CONFIG_TRUSTKERNEL_TEE_SUPPORT) += tkcore/
obj-$(CONFIG_MTK_DEVMPU) += devmpu/
obj-y += pidmap/
obj-$(CONFIG_MTK_RAM_CONSOLE) += ram_console/
obj-$(CONFIG_MEDIATEK_SOLUTION)	+= aee/
obj-$(CONFIG_MEDIATEK_SOLUTION) += sched/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
obj-$(CONFIG_MU3_PHY)   += mu3phy/
obj-$(CONFIG_USB_MU3D_DRV)  += mu3d/
obj-$(CONFIG_MTK_RTC)	+= rtc/
obj-$(CONFIG_MTK_XO)	+= xo/
obj-$(CONFIG_MTK_DCXO)	+= dcxo/
obj-$(CONFIG_MTK_CMDQ) += cmdq/
obj-$(CONFIG_MTK_CMDQ) += mdp/
obj-$(CONFIG_MTK_SMI_EXT) += smi/
obj-$(CONFIG_MTK_LCM) += lcm/
obj-$(CONFIG_MTK_LCM) += ext_disp/
ifneq ($(CONFIG_CUSTOM_KERNEL_HDMI), "")
obj-$(CONFIG_MTK_HDMI_SUPPORT) += hdmi/
endif
obj-$(CONFIG_MTK_FB) += video/
obj-$(CONFIG_MTK_DYNAMIC_FPS_FRAMEWORK_SUPPORT) += dfrc/
obj-$(CONFIG_MTK_PMIC_WRAP_HAL) += pmic_wrap/
obj-$(CONFIG_MTK_DFD_INTERNAL_DUMP) += dfd/
obj-$(CONFIG_RT_REGMAP) += rt-regmap/
obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += videocodec/
obj-$(CONFIG_MTK_JPEG) += jpeg/
obj-$(CONFIG_MTK_CCU) += ccu/src/

obj-$(CONFIG_MTK_CQDMA) += cqdma/

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
obj-y += power/
else
obj-$(CONFIG_MACH_MT8167)+= power/
endif
obj-y += log_store/
obj-$(CONFIG_MTK_ECCCI_C2K) += c2k_usb/
obj-$(CONFIG_USB) += usb_boost/
obj-$(CONFIG_MTK_USB_TYPEC) += typec/
ifneq ($(wildcard $(srctree)/../vendor/mediatek/internal/selinux_warning_enable),)
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING) += selinux_warning/
endif
obj-$(CONFIG_THERMAL)   += thermal/
obj-$(CONFIG_MTK_MTD_NAND) += nand/
obj-y += performance/
obj-$(CONFIG_MEDIATEK_SOLUTION) += cmo/
obj-$(CONFIG_MTK_VOW_SUPPORT)  += vow/
obj-y += io_boost/
obj-y += mtprintk/
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/met_drv/),)
obj-y += met_drv/
endif
obj-y += perf/
obj-$(CONFIG_MTK_JPEG) += jpeg/
obj-$(CONFIG_MTK_SPMTWAM) += twam/
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += lpm/
obj-$(CONFIG_MTK_TASK_TURBO) += task_turbo/
obj-$(CONFIG_MTK_UNIFIED_DEBUG_INTERFACE) += udi/
obj-y += rps/
obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp/
obj-$(CONFIG_MTK_USB2JTAG_SUPPORT) += usb2jtag/
obj-y += subpmic/
obj-$(CONFIG_MTK_OTP) += otp/
