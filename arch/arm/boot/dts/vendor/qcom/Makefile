# SPDX-License-Identifier: GPL-2.0-only

include $(srctree)/arch/arm64/boot/dts/vendor/qcom/Makefile
$(obj)/%.dtb: $(src)/../../../../../arm64/boot/dts/vendor/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
$(obj)/%.dtbo:$(src)/../../../../../arm64/boot/dts/vendor/qcom/%.dts FORCE
	$(call if_changed_dep,dtc)
	$(call if_changed,dtbo_verify)

dtbs: $(addprefix $(obj)/,$(dtb-y)) $(addprefix $(obj)/,$(dtbo-y))
else
dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
