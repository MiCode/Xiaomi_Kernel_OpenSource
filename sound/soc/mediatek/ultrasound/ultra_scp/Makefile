# SPDX-License-Identifier: GPL-2.0

# MTK Platform Support

MTK_PLATFORM := mt6885

# include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/utility
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/utility
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -Werror -I$(srctree)/include/uapi
subdir-ccflags-y += -Werror -I$(srctree)/include/linux/soc/mediatek
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/rv33
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/scp/rv33/v02
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/ultrasound/ultra_common


# common code
snd-soc-mtk-scp-ultra-objs := \
	mtk-scp-ultra-pcm.o \
	mtk-scp-ultra-mem-control.o \
	mtk-scp-ultra_dump.o \
	mtk-scp-ultra-platform-driver.o \
	mtk-scp-ultra-common.o \
	ultra_ipi.o

obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += snd-soc-mtk-scp-ultra.o
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
