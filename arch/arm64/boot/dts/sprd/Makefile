# SPDX-License-Identifier: GPL-2.0

ifeq ($(BSP_BUILD_DT_OVERLAY),y)

ifeq ($(BSP_BUILD_ANDROID_OS),y)

ifeq ($(BSP_BUILD_FAMILY),sharkl3)

dtb-$(CONFIG_ARCH_SPRD) += sp9863a-1h10-overlay.dtbo
dtb-$(CONFIG_ARCH_SPRD) += sp9863a-1h10-go-overlay.dtbo

sp9863a-1h10-overlay.dtbo-base := sp9863a-base.dtb
sp9863a-1h10-go-overlay.dtbo-base := sp9863a-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5Pro)

dtb-$(CONFIG_ARCH_SPRD) += ums512-1h10-overlay.dtbo
dtb-$(CONFIG_ARCH_SPRD) += ums512-1h10-go-overlay.dtbo

ums512-1h10-overlay.dtbo-base := ums512-base.dtb
ums512-1h10-go-overlay.dtbo-base := ums512-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),sharkl5)

dtb-$(CONFIG_ARCH_SPRD) += ums312-2h10-overlay.dtbo
dtb-$(CONFIG_ARCH_SPRD) += ums312-2h10-go-overlay.dtbo

ums312-2h10-overlay.dtbo-base := ums312-base.dtb
ums312-2h10-go-overlay.dtbo-base := ums312-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirl6)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9230-1h10-overlay.dtbo \
	ums9230-1h10_go-overlay.dtbo \
	ums9230-4h10-overlay.dtbo \
	ums9230-4h10-go-overlay.dtbo \
	ums9230-6h10-overlay.dtbo \
	arctic-overlay.dtbo \
	uis7863-6h10-overlay.dtbo \
	uis7865-6h10-overlay.dtbo \
	ums9230-6h10-umb9230s-overlay.dtbo \
	ums9230-6h10-customer-overlay.dtbo \
	ums9230-6h10-go-overlay.dtbo \
	uis7863-6h10-go-overlay.dtbo \
	uis7865-6h10-go-overlay.dtbo \
	ums9230-14c10-go-overlay.dtbo

ums9230-1h10-overlay.dtbo-base := ums9230-base.dtb
ums9230-1h10_go-overlay.dtbo-base := ums9230-base.dtb
ums9230-4h10-overlay.dtbo-base := ums9230-base.dtb
ums9230-4h10-go-overlay.dtbo-base := ums9230-base.dtb
ums9230-6h10-overlay.dtbo-base := ums9230-base.dtb
arctic-overlay.dtbo-base := ums9230-base.dtb
uis7863-6h10-overlay.dtbo-base := ums9230-base.dtb
uis7865-6h10-overlay.dtbo-base := ums9230-base.dtb
ums9230-6h10-umb9230s-overlay.dtbo-base := ums9230-base.dtb
ums9230-6h10-customer-overlay.dtbo-base := ums9230-base.dtb
ums9230-6h10-go-overlay.dtbo-base := ums9230-base.dtb
uis7863-6h10-go-overlay.dtbo-base := ums9230-base.dtb
uis7865-6h10-go-overlay.dtbo-base := ums9230-base.dtb
ums9230-14c10-go-overlay.dtbo-base := ums9230-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn6pro)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9620-1h10-overlay.dtbo \
	ums9620-2h10-overlay.dtbo \
	uis7870sc-2h10-overlay.dtbo \
	uis7885-2h10-overlay.dtbo \
	ums9620-2h10_uob-overlay.dtbo \
	ums9620-2h10_uob_marlin3-overlay.dtbo \
	ums9620-2h10_w6dl_UOB-overlay.dtbo

ums9620-1h10-overlay.dtbo-base := ums9620-base.dtb
ums9620-2h10-overlay.dtbo-base := ums9620-base.dtb
uis7870sc-2h10-overlay.dtbo-base := ums9620-base.dtb
uis7885-2h10-overlay.dtbo-base := ums9620-base.dtb
ums9620-2h10_uob-overlay.dtbo-base := ums9620-base.dtb
ums9620-2h10_uob_marlin3-overlay.dtbo-base := ums9620-base.dtb
ums9620-2h10_w6dl_UOB-overlay.dtbo-base := ums9620-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn6l)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9610-haps-overlay.dtbo \
	ums9610-zebu-flash-overlay.dtbo \
	ums9621-1h10-overlay.dtbo \
	ums9621-1h10_3le-overlay.dtbo \
	ums9621-1h10_go-overlay.dtbo \
	ums9621-1h10_uob-overlay.dtbo \
	ums9610-zebu-overlay.dtbo

ums9610-haps-overlay.dtbo-base := ums9610-base.dtb
ums9621-1h10-overlay.dtbo-base := ums9621-base.dtb
ums9621-1h10_3le-overlay.dtbo-base := ums9621-base.dtb
ums9621-1h10_go-overlay.dtbo-base := ums9621-base.dtb
ums9610-zebu-flash-overlay.dtbo-base := ums9610-base.dtb
ums9610-zebu-overlay.dtbo-base := ums9610-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),qogirn7)
dtb-$(CONFIG_ARCH_SPRD) += \
	ums9650-haps-overlay.dtbo

ums9650-haps-overlay.dtbo-base := ums9650-base.dtb
endif

ifeq ($(BSP_BUILD_FAMILY),uis8520)
dtb-$(CONFIG_ARCH_SPRD) += \
	uis8520-k515-zebu-overlay.dtbo \
	uis8520-1h10-emmc-overlay.dtbo \
	uis8520-1h10-nand-overlay.dtbo \
	uis8520-haps-spi-nand-overlay.dtbo

uis8520-k515-zebu-overlay.dtbo-base := uis8520-base.dtb
uis8520-1h10-emmc-overlay.dtbo-base := uis8520-base.dtb
uis8520-1h10-nand-overlay.dtbo-base := uis8520-base.dtb
uis8520-haps-spi-nand-overlay.dtbo-base := uis8520-base.dtb
endif

endif #//BSP_BUILD_ANDROID_OS

else #//For UPSTREAM or AiAiAi

dtb-$(CONFIG_ARCH_SPRD) += sc9836-openphone.dtb \
			sp9860g-1h10.dtb	\
			sp9863a-1h10.dtb	\
			ums512-1h10.dtb

endif #//BSP_BUILD_DT_OVERLAY
