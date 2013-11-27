dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb

targets += dtbs
targets += $(dtb-y)

#used for cleaning - not building
subdir- := qti

dtbs: $(addprefix $(obj)/, $(dtb-y))

clean-files := *.dtb
