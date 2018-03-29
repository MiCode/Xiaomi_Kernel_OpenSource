ifdef MTK_PLATFORM

PRIVATE_CUSTOM_KERNEL_DCT := $(if $(CUSTOM_KERNEL_DCT),$(CUSTOM_KERNEL_DCT),dct)
ifneq ($(wildcard $(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws),)
  DRVGEN_PATH := arch/arm/mach-$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)
  DWS_FILE := $(srctree)/$(DRVGEN_PATH)/$(MTK_PROJECT).dws
else
  ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws),)
    DRVGEN_PATH := drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)
    DWS_FILE := $(srctree)/$(DRVGEN_PATH)/codegen.dws
  else
    DRVGEN_PATH := drivers/misc/mediatek/dws/$(MTK_PLATFORM)
    DWS_FILE := $(srctree)/$(DRVGEN_PATH)/$(MTK_PROJECT).dws
  endif
endif
ifndef DRVGEN_OUT
#DRVGEN_OUT := $(objtree)/$(DRVGEN_PATH)
DRVGEN_OUT := $(objtree)/arch/$(SRCARCH)/boot/dts
endif
export DRVGEN_OUT

ALL_DRVGEN_FILE := cust.dtsi

ifneq ($(wildcard $(DWS_FILE)),)
DRVGEN_FILE_LIST := $(addprefix $(DRVGEN_OUT)/,$(ALL_DRVGEN_FILE))
else
DRVGEN_FILE_LIST :=
endif
DRVGEN_TOOL := $(srctree)/tools/dct/DrvGen

.PHONY: drvgen
drvgen: $(DRVGEN_FILE_LIST)
$(DRVGEN_OUT)/cust.dtsi: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	$(DRVGEN_TOOL) $(DWS_FILE) $(dir $@) $(dir $@) cust_dtsi

DTB_OVERLAY_IMAGE_TAGERT := $(objtree)/arch/$(SRCARCH)/boot/dts/overlays/dtbo.img
$(DTB_OVERLAY_IMAGE_TAGERT) : PRIVATE_DTB_OVERLAY_OBJ:=$(objtree)/arch/$(SRCARCH)/boot/dts/overlays/$(MTK_PROJECT)-overlay.dtb
$(DTB_OVERLAY_IMAGE_TAGERT) : PRIVATE_DTB_OVERLAY_HDR:=$(srctree)/scripts/dtbo.cfg
$(DTB_OVERLAY_IMAGE_TAGERT) : PRIVATE_MKIMAGE_TOOL:=$(srctree)/scripts/mkimage
$(DTB_OVERLAY_IMAGE_TAGERT) : $(PRIVATE_DTB_OVERLAY_OBJ) dtbs $(PRIVATE_DTB_OVERLAY_HDR) | $(PRIVATE_MKIMAGE_TOOL)
	@echo Singing the generated overlay dtbo.
	$(PRIVATE_MKIMAGE_TOOL) $(PRIVATE_DTB_OVERLAY_OBJ) $(PRIVATE_DTB_OVERLAY_HDR)  > $@
.PHONY: dtboimage
dtboimage : $(DTB_OVERLAY_IMAGE_TAGERT)

endif#MTK_PLATFORM
