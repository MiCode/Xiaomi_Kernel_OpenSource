#
# Makefile for kernel Silead Fingerprint drivers.
#

ifdef CONFIG_ARCH_MSM
	 ccflags-y += -DBSP_SIL_PLAT_QCOM
endif

ifdef CONFIG_ARCH_QCOM
	 ccflags-y += -DBSP_SIL_PLAT_QCOM
endif

ifdef CONFIG_ARCH_SPRD
	 ccflags-y += -DBSP_SIL_PLAT_SPRD
endif

ifdef CONFIG_ARCH_MTK_PROJECT
   #include $(srctree)/drivers/misc/mediatek/Makefile.custom
   ccflags-y += -DBSP_SIL_PLAT_MTK
   ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
   subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include \
					-I$(srctree)/drivers/misc/mediatek/include/mt-plat
 ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
   subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
 endif
 ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
 endif
endif
KDIR	:= /lib/modules/$(shell uname -r)/build
ccflags-y += -DCONFIG_SILEAD_FP_PLATFORM
obj-$(CONFIG_FINGERPRINT_SILEAD)     += silead_fp.o


