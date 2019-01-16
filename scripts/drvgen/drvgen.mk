PRIVATE_CUSTOM_KERNEL_DCT:= $(if $(CUSTOM_KERNEL_DCT),$(CUSTOM_KERNEL_DCT),dct)
DRVGEN_TOOL := $(PWD)/tools/dct/DrvGen
ifneq ($(wildcard $(PWD)/arch/arm/mach-$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws),)
    DWS_FILE := $(PWD)/arch/arm/mach-$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)/codegen.dws
ifndef DRVGEN_OUT
    DRVGEN_OUT := $(objtree)/arch/arm/mach-$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)
endif
else
    DWS_FILE := $(PWD)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(if $(CUSTOM_DCT),$(CUSTOM_DCT),dct)/codegen.dws
ifndef DRVGEN_OUT
    DRVGEN_OUT := $(objtree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(MTK_PROJECT)/dct/$(PRIVATE_CUSTOM_KERNEL_DCT)
endif
endif
export DRVGEN_OUT

DRVGEN_OUT_PATH := $(DRVGEN_OUT)/inc

DRVGEN_FILE_LIST := $(DRVGEN_OUT)/inc/cust_kpd.h \
		    $(DRVGEN_OUT)/inc/cust_eint.h \
		    $(DRVGEN_OUT)/inc/cust_gpio_boot.h \
		    $(DRVGEN_OUT)/inc/cust_gpio_usage.h \
		    $(DRVGEN_OUT)/inc/cust_adc.h \
		    $(DRVGEN_OUT)/inc/cust_power.h \
		    $(DRVGEN_OUT)/inc/pmic_drv.h \
		    $(DRVGEN_OUT)/pmic_drv.c

ifneq ($(MTK_PLATFORM),mt8127)
  DRVGEN_FILE_LIST += $(DRVGEN_OUT)/inc/cust_eint_md1.h
endif


ifeq ($(MTK_PLATFORM),mt6735)
  DRVGEN_FILE_LIST += $(DRVGEN_OUT)/inc/cust_eint_md2.h \
		    $(DRVGEN_OUT)/inc/cust_i2c.h \
		    $(DRVGEN_OUT)/inc/cust_clk_buf.h \
		    $(DRVGEN_OUT)/cust_eint.dtsi
endif

ifeq ($(MTK_PLATFORM),mt6752)
  DRVGEN_FILE_LIST += $(DRVGEN_OUT)/inc/cust_eint_md2.h \
		    $(DRVGEN_OUT)/inc/cust_i2c.h \
		    $(DRVGEN_OUT)/inc/cust_clk_buf.h \
		    $(DRVGEN_OUT)/cust_eint.dtsi
endif
ifeq ($(MTK_PLATFORM),mt6795)
  DRVGEN_FILE_LIST += $(DRVGEN_OUT)/inc/cust_i2c.h \
		    $(DRVGEN_OUT)/inc/cust_clk_buf.h \
		    $(DRVGEN_OUT)/inc/cust_gpio_suspend.h \
		    $(DRVGEN_OUT)/cust_eint.dtsi
endif

ifeq ($(MTK_PLATFORM),mt8127)
  DRVGEN_FILE_LIST += $(DRVGEN_OUT)/inc/cust_eint_ext.h
endif

archprepare: $(DRVGEN_FILE_LIST)
dtbs: $(DRVGEN_FILE_LIST)
drvgen: $(DRVGEN_FILE_LIST)

$(DRVGEN_OUT)/inc/cust_kpd.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) kpd_h

$(DRVGEN_OUT)/inc/cust_eint.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) eint_h

$(DRVGEN_OUT)/inc/cust_gpio_boot.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) gpio_boot_h

$(DRVGEN_OUT)/inc/cust_gpio_usage.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) gpio_usage_h

$(DRVGEN_OUT)/inc/cust_adc.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) adc_h

$(DRVGEN_OUT)/inc/cust_eint_md1.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) md1_eint_h

$(DRVGEN_OUT)/inc/cust_power.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) power_h

$(DRVGEN_OUT)/inc/pmic_drv.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) pmic_h

$(DRVGEN_OUT)/pmic_drv.c: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT) $(DRVGEN_OUT_PATH) pmic_c

$(DRVGEN_OUT)/inc/cust_i2c.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) i2c_h

$(DRVGEN_OUT)/inc/cust_clk_buf.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) clk_buf_h

$(DRVGEN_OUT)/inc/cust_eint_md2.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) md2_eint_h

$(DRVGEN_OUT)/inc/cust_gpio_suspend.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) $(DRVGEN_OUT_PATH) suspend_h

$(DRVGEN_OUT)/inc/cust_eint_ext.h: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT_PATH) eint_ext_h

$(DRVGEN_OUT)/cust_eint.dtsi: $(DRVGEN_TOOL) $(DWS_FILE)
	@mkdir -p $(dir $@)
	@$(DRVGEN_TOOL) $(DWS_FILE) $(DRVGEN_OUT) $(DRVGEN_OUT_PATH) eint_dtsi

