#$(call all-subdir-src-or-makefile)

include $(srctree)/drivers/misc/mediatek/Makefile.custom

# For MT6795 FPGA bring-up
ifeq ($(CONFIG_ARCH_MT6795),y)
include $(srctree)/drivers/misc/mediatek/Makefile.mt6795
else

ifneq (,$(filter $(CONFIG_ARCH_MT6735) $(CONFIG_ARCH_MT6735M) $(CONFIG_ARCH_MT6753), y))
include $(srctree)/drivers/misc/mediatek/Makefile.mt6735
else

ifeq ($(CONFIG_MTK_L2C_SHARE),y)
obj-y   += l2c_share/
endif

ifeq ($(CONFIG_MTK_SYS_CIRQ),y)
obj-y += cirq/
else
obj-$(CONFIG_MTK_CIRQ) += cirq/
endif

ifneq ($(CONFIG_MTK_GIC),y)
obj-y   += irq/
endif

obj-$(CONFIG_MTK_CPU_STRESS)	+= cpu_stress/
obj-$(CONFIG_MTK_LASTPC)	    += lastpc/
obj-$(CONFIG_MTK_DBG_DUMP) += dbg_dump/

ifeq ($(CONFIG_MTK_EXTMEM),y)
obj-y	+= exm/
endif

obj-$(CONFIG_MTK_SYSTRACKER)   += systracker/
obj-y   += cqdma/

obj-y   += watchpoint/
obj-$(CONFIG_MTK_ETM)   += etm/
obj-y	+= mach/
obj-y	+= kernel/
obj-y	+= aee/
obj-$(CONFIG_MTK_EMI_MPU)	+= emi_mpu/
obj-$(CONFIG_MTK_EMI_BWL)	+= emi_bwl/

obj-$(CONFIG_MTK_FMRADIO)	+= fmradio/
obj-$(CONFIG_MTK_GPS)	    += gps/

obj-y	+= m4u/
#obj-y	+= mhl/
obj-y	+= mtprof/
obj-y	+= sched/
obj-$(CONFIG_MTK_CMDQ) += cmdq/
obj-y	+= mmp/
obj-y	+= dispsys/
obj-$(CONFIG_MTK_VIDEOX) += videox/

obj-y	+= auxadc/

#obj-y	+= bthid/
#obj-y	+= ccmni/

ifdef CONFIG_CUSTOM_KERNEL_EXTMD
ifneq ($(strip $(CONFIG_CUSTOM_KERNEL_EXTMD)),"")
obj-y	+= emd_ctl/
endif
endif

obj-$(CONFIG_MTK_WFD_SUPPORT)  += uibc/

ifneq ($(CONFIG_GATOR_DRIVER),)
 ifneq ($(wildcard $(srctree)/$(src)/gator-driver/),)
  obj-$(CONFIG_GATOR_DRIVER) += gator-driver/
 endif
endif


obj-$(CONFIG_MTK_XHCI)   +=  xhci/

#obj-y	+=  hdmitx/
obj-y	+= ram_console/
obj-y	+= thermal/
obj-y	+= videocodec/
obj-$(CONFIG_MTK_MJC_DRIVER)   += mjc/
obj-y	+= xlog/
ifneq ($(filter y, $(CONFIG_ARM_PSCI) $(CONFIG_ARM64)),)
	obj-y   +=  atf_log/
endif
obj-y   += wdk/
#obj-y	+= evdo_support/
obj-$(CONFIG_MTK_MT_LOGGER)	+= mt_logger/
obj-$(CONFIG_MD32_SUPPORT)   += md32/
obj-y += dramc/
obj-y	+= lcm/

ifeq ($(CONFIG_MTK_SENSOR_SUPPORT),y)
obj-$(CONFIG_MTK_HWMON)	    += hwmon/
obj-$(CONFIG_CUSTOM_KERNEL_ALSPS) += alsps/
obj-$(CONFIG_CUSTOM_KERNEL_ACCELEROMETER) += accelerometer/
obj-$(CONFIG_CUSTOM_KERNEL_GYROSCOPE) += gyroscope/
obj-$(CONFIG_CUSTOM_KERNEL_MAGNETOMETER) += magnetometer/
obj-$(CONFIG_CUSTOM_KERNEL_BAROMETER) += barometer/

ifneq ($(CONFIG_CUSTOM_KERNEL_SENSORHUB),"")
obj-y	+= sensorHub/
ifneq ($(CONFIG_CUSTOM_KERNEL_STEP_COUNTER),"")
obj-y	+= step_counter/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_SIGNIFICANT_MOTION_SENSOR),"")
obj-y	+= step_counter/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_ACTIVITY_SENSOR),"")
obj-y	+= activity_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_PICK_UP_SENSOR),"")
obj-y	+= pick_up_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_SHAKE_SENSOR),"")
obj-y	+= shake_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_FACE_DOWN_SENSOR),"")
obj-y	+= face_down_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_IN_POCKET_SENSOR),"")
obj-y	+= in_pocket_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_PEDOMETER),"")
obj-y	+= pedometer/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_HEART_RATE_SENSOR),"")
obj-y	+= heart_rate_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_TILT_DETECTOR_SENSOR),"")
obj-y	+= tilt_detector_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_WAKE_GESTURE_SENSOR),"")
obj-y	+= wake_gesture_sensor/
endif
ifneq ($(CONFIG_CUSTOM_KERNEL_GLANCE_GESTURE_SENSOR),"")
obj-y	+= glance_gesture_sensor/
endif
endif
endif
#obj-y	+= matv/
#obj-y	+= touchpanel/
#

obj-$(CONFIG_MTK_VOW_SUPPORT)	+= vow/

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include
obj-$(CONFIG_MTK_COMBO)			+= connectivity/
obj-$(CONFIG_MTK_CONN_MD)	+= conn_md/

obj-y	+=  ccci_util/
obj-$(CONFIG_MTK_CCCI_DRIVER)  +=  ccci/
obj-$(CONFIG_MTK_CCCI_EXT)     +=  dual_ccci/
obj-$(CONFIG_MTK_ECCCI_DRIVER) +=  eccci/
obj-$(CONFIG_MTK_EEMCS_DRIVER) +=  eemcs/
obj-$(CONFIG_MTK_NFC) += nfc/
obj-$(CONFIG_ZRAM)	+= mlog/

#obj-y	+= cam_cal/src/$(MTK_PLATFORM)/dummy_eeprom/

obj-$(CONFIG_MTK_CAM_CAL_GT24C32A_SUPPORT)	+= cam_cal/src/$(MTK_PLATFORM)/GT24c32a_eeprom/
obj-$(CONFIG_MTK_CAM_CAL_BRCC064GWZ_3_SUPPORT) += cam_cal/src/$(MTK_PLATFORM)/BRCC064GWZ_3_eeprom/
obj-$(CONFIG_MTK_CAM_CAL_BRCB032GWZ_3_SUPPORT) += cam_cal/src/$(MTK_PLATFORM)/BRCB032GWZ_3_eeprom/
#add tianqiang
obj-$(CONFIG_MTK_CAM_CAL_OV13853OTP_SUPPORT) += cam_cal/src/$(MTK_PLATFORM)/ov13853otp/
obj-y	+= lens/
obj-y	+= imgsensor/src/
obj-y	+= laser/

ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/cam_cal),)
obj-$(CONFIG_MTK_CAM_CAL)	+= cam_cal/src/
endif

#exclude mt6735
ifeq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6735"))
obj-y	+=  flashlight/src/
endif

obj-y	+= sync/

#rtc must compile before power, because they are both in fs_initcall, rtc need initial first for PMIC alarm call back
obj-y	+= rtc/
obj-y	+= boot/
obj-y	+= boot_reason/
obj-y	+= chip/
obj-y	+= gpio/
obj-y	+= eint/
obj-y	+= uart/
obj-y	+= pmic_wrap/

ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y	+= hdmi/
obj-y	+= ext_disp/
endif

obj-$(CONFIG_MTK_HIBERNATION) += hibcore/
ifeq ($(CONFIG_CUSTOM_KERNEL_FLASHLIGHT),"dummy_flashlight")
#obj-y	+= flashlight/dummy_flashlight/
endif

obj-y	+= accdet/
obj-y	+= keypad/
obj-$(CONFIG_MTK_DUM_CHAR)	+= dum-char/
obj-$(CONFIG_MTK_SYSENV)	+= sysenv/

ifdef CONFIG_CUSTOM_KERNEL_SSW
ifneq ($(strip $(CONFIG_CUSTOM_KERNEL_SSW)),"")
obj-y	+= ssw/
endif
endif

#obj-$(CONFIG_MTK_CMMB) += cmmbdrv/
obj-y	+= devapc/
obj-y	+= devinfo/
obj-y	+= gpu/

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6582" "mt6592" "mt6595" "mt6752" "mt6795"))
obj-y	+= jpeg/
endif

obj-y	+= leds/
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6572" "mt6582" "mt6592" "mt6595" "mt6752" "mt6795" "mt8127"))
obj-y	+= masp/
endif
obj-$(CONFIG_MTK_MMC)	+= mmc-host/
#obj-$(CONFIG_MTK_MTD_NAND)	+= nand/
obj-$(CONFIG_MTK_GPT_SCHEME_SUPPORT)	+= partition/
obj-$(CONFIG_MTK_MMC)	+= pmt/
obj-$(CONFIG_MTK_SOUND) += smartpa/

#charge pump
ifeq ($(CONFIG_CUSTOM_KERNEL_CHARGEPUMP),y)
obj-y	+= chargepump/
endif

obj-y	+= power/
obj-y	+= i2c/
obj-y	+= pwm/
obj-$(CONFIG_MTK_SMI)	+= smi/
obj-$(CONFIG_MTK_BTCVSD) += btcvsd/
obj-$(CONFIG_MTK_SOUND)	+= sound/
obj-$(CONFIG_MTK_SPI)	+= spi/
obj-y	+= spm/

# USB need to after gpio
obj-$(CONFIG_USB_MTK_HDRC)	+= usb20/
obj-y	+= usb11/
obj-$(CONFIG_USB_MU3D_DRV)	+= mu3d/
obj-$(CONFIG_USB_MU3D_DRV)	+= mu3phy/

obj-$(CONFIG_MTK_TEST_XHCI)   +=  xhci_test/

obj-y	+= vibrator/
obj-y	+= wdt/
obj-y	+= freqhopping/
obj-y	+= selinux_warning/
# MTK PASR SW flow
obj-$(CONFIG_MTKPASR)	+= mtkpasr/

obj-$(CONFIG_MTK_TC1_FEATURE)	+= tc1_interface/

obj-$(CONFIG_MTK_SWITCH_TX_POWER) += sw_tx_power/

#charge pump
#obj-y	+= chargepump/

#obj-$(CONFIG_MTK_SOUND)	+= smartpa/

obj-$(CONFIG_MTK_BTIF)	+= btif/
obj-$(CONFIG_MTK_POWER_GS)	+= power_gs/
obj-$(CONFIG_MTK_VIDEO) += video/
obj-$(CONFIG_MTK_AEE_MRDUMP)	+= mrdump/
obj-$(CONFIG_MTK_WMT_CCCI)	+= wmt_ccci/
obj-$(CONFIG_MTK_VCOREFS)	+= vcorefs/

obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += gud/
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += secmem/
obj-$(CONFIG_EVDO_DT_VIA_SUPPORT) += misc_via/
obj-$(CONFIG_MTK_SCHED_CMP_POWER_AWARE_CONTROLLER)	+=  pa/
obj-$(CONFIG_HMP_POWER_AWARE_CONTROLLER)	+=  pa_hmp/

#widevine drm
$(warning "CONFIG_TRUSTONIC_TEE_SUPPORT = $(CONFIG_TRUSTONIC_TEE_SUPPORT)")
obj-$(CONFIG_TRUSTONIC_TEE_SUPPORT) += secwidevine/

obj-$(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) += trustzone/

#power-loss-test
obj-$(CONFIG_PWR_LOSS_MTK_TEST) += power-loss-test/

endif           # end of CONFIG_ARCH_MT6735
endif		# end of CONFIG_ARCH_MT6795

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6752"))
obj-y	+= ddr/$(CONFIG_MTK_PLATFORM)/
endif

#add by lizhiye for hongwai
obj-y		+= consumerir/

obj-y		+= sltdevinfo/

obj-$(CONFIG_MTK_HALL_AH1903)           += hall/

obj-y	+= step_counter/
