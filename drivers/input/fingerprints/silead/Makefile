#
# Makefile for kernel Silead Fingerprint drivers.
#

#ifdef CONFIG_ARCH_MSM
	 #ccflags-y += -DBSP_SIL_PLAT_QCOM
#endif

#ifdef CONFIG_ARCH_QCOM
	 #ccflags-y += -DBSP_SIL_PLAT_QCOM
#endif

#ifdef CONFIG_ARCH_SPRD
	 #ccflags-y += -DBSP_SIL_PLAT_SPRD
#endif

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

KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS += drivers/input/fingerprints/fpc/Module.symvers
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS += drivers/input/fingerprints/goodix/Module.symvers
export KBUILD_EXTRA_SYMBOLS

obj-m += sil_fingerprint.o
sil_fingerprint-objs:= silead_fp.o silead_fp_stub.o silead_fp_spi.o
