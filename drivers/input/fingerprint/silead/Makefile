#
# Makefile for kernel Silead Fingerprint drivers.
#

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

obj-$(CONFIG_SILEAD_FP) += silead_fingerprint.o
silead_fingerprint-objs := silead_fp.o silead_fp_stub.o silead_fp_spi.o


