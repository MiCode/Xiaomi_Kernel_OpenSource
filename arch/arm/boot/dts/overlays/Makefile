ifeq ($(strip $(MTK_DTBO_FEATURE)), yes)
ifeq ($(CONFIG_OF),y)
# Overlays
dtb-y += $(addsuffix -overlay.dtb, $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES)))

targets += dtbs dtbs_install
targets += $(dtb-y)
endif

DTB_LIST += overlays/$(dtb-y)
always		:= $(dtb-y)
clean-files	:= *.dtb

ifeq ($(CONFIG_OF_RESOLVE),y)
DTC_FLAGS += -@
endif
endif
