#inlcude SPI Interface Start
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6799/
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif
#include SPI Interface End

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
obj-$(CONFIG_REGULATOR_ISL91302A) += vproc2_ipi.o
else
obj-$(CONFIG_REGULATOR_ISL91302A) += isl91302a-spi.o
endif
obj-$(CONFIG_REGULATOR_ISL91302A) += isl91302a-regulator.o
obj-$(CONFIG_REGULATOR_RT5734) += rt5734-spi.o rt5734-regulator.o
obj-$(CONFIG_REGULATOR_RT5738) += rt5738-regulator.o
obj-$(CONFIG_EXTBUCK_MT6311) += mt6311-i2c.o mt6311-regulator.o mt6311_api.o
obj-$(CONFIG_MTK_EXTBUCK) += extbuck.o
