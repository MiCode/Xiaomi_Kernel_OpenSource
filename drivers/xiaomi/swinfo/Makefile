# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_MI_SOFTWARE_INFO)         += swinfo.o
swinfo-y := swinfo_func.o

# Export platform kernel config
$(obj)/swinfo_func.o: $(obj)/config_platform_data.gz

$(obj)/config_platform_data.gz: $(obj)/config_platform_data FORCE
	$(call if_changed,gzip)

filechk_cat = cat $<

$(obj)/config_platform_data: $(KCONFIG_CONFIG) FORCE
	$(call filechk,cat)
