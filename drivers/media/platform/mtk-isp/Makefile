# SPDX-License-Identifier: GPL-2.0
obj-y += camsys/
obj-m += mtk-hcp/
obj-m += mtk-aov/
obj-m += imgsys/
obj-m += mtk-ipesys-me/

subdir-ccflags-y += -D__XIAOMI_CAMERA__

ifeq ($(CONFIG_BUILD_XIAOMI_CAMERA_IMAGE_NAMES),"zircon")
subdir-ccflags-y += -DZIRCON_CAM
endif
