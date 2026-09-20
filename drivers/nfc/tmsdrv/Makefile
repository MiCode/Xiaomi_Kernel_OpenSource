#
# Makefile for TMS devices
#
#CONFIG_TMS_NFC_DEVICE=y
#CONFIG_TMS_ESE_DEVICE=y
#CONFIG_TMS_GUIDE_DEVICE=y
ccflags-y := -DCONFIG_TMS_NFC_DEVICE=1
#ccflags-y += -DCONFIG_TMS_ESE_DEVICE=1
ccflags-y += -DCONFIG_TMS_GUIDE_DEVICE=1
ccflags-y += -DTMS_DEBUGER_LOGGER=1
ccflags-y += -DTMS_DEBUGER_CHECKER=1
$(info   CONFIG_TMS_NFC_DEVICE = $(CONFIG_TMS_NFC_DEVICE))
$(info   CONFIG_TMS_ESE_DEVICE = $(CONFIG_TMS_ESE_DEVICE))
$(info   CONFIG_TMS_GUIDE_DEVICE = $(CONFIG_TMS_GUIDE_DEVICE))
TMS_DEVICES = $(findstring y, $(CONFIG_TMS_NFC_DEVICE) $(CONFIG_TMS_ESE_DEVICE))
ifneq ($(TMS_DEVICES), y)
TMS_DEVICES = $(findstring m, $(CONFIG_TMS_NFC_DEVICE) $(CONFIG_TMS_ESE_DEVICE))
endif
$(info   TMS_DEVICES = $(TMS_DEVICES))
obj-$(TMS_DEVICES)       += tms_device_modules.o
tms_device_modules-objs  := tms_common.o
tms_device_modules-objs += debuger/debuger.o \
                           debuger/logger.o \
                           debuger/checker.o
# TMS NFC driver
ifdef CONFIG_TMS_NFC_DEVICE
tms_device_modules-objs += nfc/nfc_common.o \
                           nfc/nfc_driver.o
# Guidev driver suport
ifdef CONFIG_TMS_GUIDE_DEVICE
tms_device_modules-objs += guidev/guide_driver.o
endif
endif
# TMS eSE driver
ifdef CONFIG_TMS_ESE_DEVICE
tms_device_modules-objs += ese/ese_common.o \
                           ese/ese_driver.o
endif