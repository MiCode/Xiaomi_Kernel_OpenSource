# combo driver: MT6620
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	include $(srctree)/drivers/misc/mediatek/Makefile.custom
	subdir-ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct
	
	COMBO_CHIP_SUPPORT := false
	ifneq ($(filter "MT6620E3",$(CONFIG_MTK_COMBO_CHIP)),)
		COMBO_CHIP_SUPPORT := true
	endif
	
	ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
		COMBO_CHIP_SUPPORT := true
	endif
	
	ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
		COMBO_CHIP_SUPPORT := true
	endif
	
	ifeq ($(COMBO_CHIP_SUPPORT), true)
		subdir-ccflags-y += -D MTK_WCN_COMBO_CHIP_SUPPORT
		COMBO_FOLDER := ../combo/common
		ccflags-y += -I$(src)/$(COMBO_FOLDER)/linux/include
	endif
	
	SOC_CHIP_SUPPORT := false
	ifneq ($(filter "CONSYS_6592",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif
	ifneq ($(filter "CONSYS_6582",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif
	ifneq ($(filter "CONSYS_8127",$(CONFIG_MTK_COMBO_CHIP)),)
                SOC_CHIP_SUPPORT := true
        endif
	ifneq ($(filter "CONSYS_6572",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif
	ifneq ($(filter "CONSYS_6571",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif
	ifneq ($(filter "CONSYS_6752",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif
	ifneq ($(filter "CONSYS_6735",$(CONFIG_MTK_COMBO_CHIP)),)
		SOC_CHIP_SUPPORT := true
	endif

	ifeq ($(SOC_CHIP_SUPPORT), true)
		subdir-ccflags-y += -D MTK_WCN_SOC_CHIP_SUPPORT
		CONN_COMMON := ../conn_soc/common
		ccflags-y += -I$(src)/$(CONN_COMMON)/linux/include
	endif
	
	subdir-ccflags-y += -DWMT_PLAT_ALPS=1
	
ifeq ($(CONFIG_MTK_COMBO),y)
	subdir-ccflags-y += -DWMT_PLAT_ALPS=1
	ccflags-y += -I$(src)/drv_init/inc 	
	obj-y += mtk_wcn_stub_alps.o
	obj-y += wmt_stp_exp.o
	
	obj-y += wmt_detect.o
	obj-y += sdio_detect.o
	obj-y += wmt_detect_pwr.o
	
	obj-y += drv_init/
	
endif
	
ifeq ($(CONFIG_MTK_COMBO),m)
	obj-y += mtk_wcn_stub_alps.o
	obj-y += wmt_stp_exp.o
	
	obj-$(CONFIG_MTK_COMBO) += mtk_wmt_detect.o
	mtk_wmt_detect-objs := wmt_detect.o
	mtk_wmt_detect-objs += sdio_detect.o
	mtk_wmt_detect-objs += wmt_detect_pwr.o

endif
  
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
