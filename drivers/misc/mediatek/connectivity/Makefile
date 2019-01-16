MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifneq ($(CONFIG_MTK_COMBO),)
	ifeq ($(CONFIG_MTK_LTE_SUPPORT),y)	
	subdir-ccflags-y += -D WMT_IDC_SUPPORT=1
	else
	subdir-ccflags-y += -D WMT_IDC_SUPPORT=0
	endif
	subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include
	subdir-ccflags-y += -D MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
	subdir-ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE
	ifeq ($(CONFIG_ARM64), y)
	subdir-ccflags-y += -D CONFIG_MTK_WCN_ARM64
	endif
	ifneq ($(filter "MT6620E3",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  combo/
	endif
	ifneq ($(filter "MT6628",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  combo/
		subdir-ccflags-y += -D MT6628
		subdir-ccflags-y += -D MTK_MERGE_INTERFACE_SUPPORT
	endif
	ifneq ($(filter "MT6630",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  combo/
		subdir-ccflags-y += -D MT6630
		subdir-ccflags-y += -D MTK_MERGE_INTERFACE_SUPPORT
	endif
	ifneq ($(filter "CONSYS_6572",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  conn_soc/
	endif
	ifneq ($(filter "CONSYS_6582",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  conn_soc/
	endif
    ifneq ($(filter "CONSYS_8127",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  conn_soc/
	endif
    	ifneq ($(filter "CONSYS_6752",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  conn_soc/
	endif
	ifneq ($(filter "CONSYS_6592",$(CONFIG_MTK_COMBO_CHIP)),)
		obj-y   +=  conn_soc/
	endif
  ifneq ($(filter "CONSYS_6735",$(CONFIG_MTK_COMBO_CHIP)),)
     obj-y   +=  conn_soc/
  endif
	obj-y   +=  common_detect/
endif
