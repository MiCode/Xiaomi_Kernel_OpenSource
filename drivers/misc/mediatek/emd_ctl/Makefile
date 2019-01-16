#
# external modem control driver
#
# if not define CUSTOM_KERNEL_EXTMD,then use dummy file
ifdef CONFIG_CUSTOM_KERNEL_EXTMD
ifneq ($(strip $(CONFIG_CUSTOM_KERNEL_EXTMD)),"")

EMDCTL_DIR=$(srctree)/drivers/misc/mediatek/emd_ctl/$(subst ",,$(CONFIG_CUSTOM_KERNEL_EXTMD))
emdctl_exist=$(shell if [ -d ${EMDCTL_DIR} ]; then echo "y"; else echo "n"; fi;)
ifeq ($(emdctl_exist), y)
obj-y += emd_ctl.o
emd_ctl-y := emd_ctl_init.o emd_ctl_cfifo.o emd_ctl_node.o emd_ctl_chr.o emd_ctl_spm.o
obj-y += $(subst ",,$(CONFIG_CUSTOM_KERNEL_EXTMD))/
else
$(warning "no $(EMDCTL_DIR) folder, need check config emdctl_exist=$(emdctl_exist)")
obj-  := dummy.o # avoid build error
endif

endif
endif
