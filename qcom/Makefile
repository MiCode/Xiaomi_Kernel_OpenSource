ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi-overlay.dtbo \
	lahaina-mtp-overlay.dtbo \
	lahaina-cdp-overlay.dtbo \
	lahaina-qrd-overlay.dtbo \
	lahaina-qrd-module-overlay.dtbo

lahaina-rumi-overlay.dtbo-base := lahaina.dtb
lahaina-mtp-overlay.dtbo-base := lahaina.dtb
lahaina-cdp-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-module-overlay.dtbo-base := lahaina.dtb
else
dtb-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi.dtb \
	lahaina-mtp.dtb \
	lahaina-cdp.dtb \
	lahaina-qrd.dtb \
	lahaina-qrd-module.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
