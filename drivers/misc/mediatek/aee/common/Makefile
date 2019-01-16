include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-y	+= aee.o
aee-y	:=dummy.o
aee-$(CONFIG_MTK_AEE_FEATURE)	+= aee-common.o reboot-reason.o
ifneq ($(filter y,$(CONFIG_ARM_PSCI) $(CONFIG_ARM64)),)
aee-$(CONFIG_MTK_AEE_FEATURE)	+= wdt-atf.o
else
aee-$(CONFIG_MTK_AEE_FEATURE)	+= wdt-handler.o
endif
