SUPPORT_MT6620 := no
SUPPORT_MT6628 := no
SUPPORT_MT6630 := no

SUPPORT_MT6620 := $(if $(filter "MT6620", $(CONFIG_MTK_COMBO_CHIP)),yes,no)
SUPPORT_MT6628 := $(if $(filter "MT6628", $(CONFIG_MTK_COMBO_CHIP)),yes,no)
SUPPORT_MT6630 := $(if $(filter "MT6630", $(CONFIG_MTK_COMBO_CHIP)),yes,no)

ifeq ($(SUPPORT_MT6620), yes)
    obj-$(CONFIG_MTK_COMBO_WIFI) += mt6620/
    $(warning include mt6620)
endif    

ifeq ($(SUPPORT_MT6628), yes)
    obj-$(CONFIG_MTK_COMBO_WIFI) += mt6628/
    $(warning include mt6628)
endif    

ifeq ($(SUPPORT_MT6630), yes)
    obj-$(CONFIG_MTK_COMBO_WIFI) += mt6630/
    $(warning include mt6630)
endif
