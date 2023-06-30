# SPDX-License-Identifier: GPL-2.0
obj-m += mtk-hcp/
obj-m += imgsys/
obj-y += camsys/
obj-m += mtk-ipesys-me/

subdir-ccflags-y += -D__XIAOMI_CAMERA__
ifeq ($(CONFIG_BUILD_XIAOMI_CAMERA_IMAGE_NAMES),"yuechu")
subdir-ccflags-y += -DYUECHU_CAM
endif
ifeq ($(CONFIG_BUILD_XIAOMI_CAMERA_IMAGE_NAMES),"aristotle")
subdir-ccflags-y += -DARISTOTLE_CAM
endif
