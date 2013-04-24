dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb

targets += dtbs
targets += $(dtb-y)

#used for cleaning - not building
subdir- := qcom

dtbs: $(addprefix $(obj)/, $(dtb-y))

clean-files := *.dtb
