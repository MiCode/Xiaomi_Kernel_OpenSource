ifneq ($(CONFIG_ARCH_QTI_VM), y)
ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi-overlay.dtbo \
	lahaina-mtp-overlay.dtbo \
	lahaina-mtp-hsp-overlay.dtbo \
	lahaina-cdp-overlay.dtbo \
	lahaina-qrd-overlay.dtbo \
	lahaina-qrd-hsp-overlay.dtbo \
	lahaina-qrd-module-overlay.dtbo \
	lahainap-mtp-overlay.dtbo \
	lahainap-cdp-overlay.dtbo \
	lahainap-qrd-overlay.dtbo

lahaina-rumi-overlay.dtbo-base := lahaina.dtb
lahaina-mtp-hsp-overlay.dtbo-base := lahaina.dtb
lahaina-mtp-overlay.dtbo-base := lahaina.dtb
lahaina-cdp-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-hsp-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-module-overlay.dtbo-base := lahaina.dtb
lahainap-mtp-overlay.dtbo-base := lahainap.dtb
lahainap-cdp-overlay.dtbo-base := lahainap.dtb
lahainap-qrd-overlay.dtbo-base := lahainap.dtb
else
dtb-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi.dtb \
	lahaina-mtp-hsp.dtb \
	lahaina-mtp.dtb \
	lahaina-cdp.dtb \
	lahaina-qrd-hsp.dtb \
	lahaina-qrd.dtb \
	lahaina-qrd-module.dtb \
	lahainap-mtp.dtb \
	lahainap-cdp.dtb \
	lahainap-qrd.dtb
endif

dtb-$(CONFIG_ARCH_WAIPIO) += waipio-rumi.dtb \
	waipio-mtp.dtb \
	waipio-cdp.dtb \
	waipio-qrd.dtb

dtb-$(CONFIG_ARCH_QTI_VM) += trustedvm.dtb
ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_HOLI) +=  holi-rumi-overlay.dtbo

holi-rumi-overlay.dtbo-base := holi.dtb
else
dtb-$(CONFIG_ARCH_HOLI) += holi-rumi.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_SHIMA) += \
	shima-rumi-overlay.dtbo

shima-rumi-overlay.dtbo-base := shima.dtb
else
dtb-$(CONFIG_ARCH_SHIMA) += shima-rumi.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_SM8150) += \
                sm8150-cdp-overlay.dtbo \
                sa8155-adp-star-overlay.dtbo \
                sa8155p-adp-star-overlay.dtbo \
                sa8155-v2-adp-air-overlay.dtbo \
                sa8155p-v2-adp-air-overlay.dtbo

sm8150-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sa8155-adp-star-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-adp-star-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
sa8155-v2-adp-air-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-v2-adp-air-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
else
dtb-$(CONFIG_ARCH_SM8150)       += sm8150-cdp.dtb \
        sa8155-adp-star.dtb \
        sa8155p-adp-star.dtb \
        sa8155-v2-adp-star.dtb \
        sa8155p-v2-adp-star.dtb \
        sa8155-v2-adp-air.dtb \
        sa8155p-v2-adp-air.dtb
endif
endif

ifeq ($(CONFIG_ARCH_LAHAINA), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) += trustedvm.dtb
endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
