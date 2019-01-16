SSW_DIR=$(srctree)/drivers/misc/mediatek/ssw/$(subst ",,$(CONFIG_CUSTOM_KERNEL_SSW))
ssw_exist=$(shell if [ -d ${SSW_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(ssw_exist), y)
obj-y += $(subst ",,$(CONFIG_CUSTOM_KERNEL_SSW))/
$(warning "SSW_DIR=$(SSW_DIR)ssw_exist=$(ssw_exist)")
else
$(warning "no $(SSW_DIR) folder, need check config ssw_exist=$(ssw_exist)")
obj-  := dummy.o # avoid build error
endif

