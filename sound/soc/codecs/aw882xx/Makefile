#for AWINIC AW882XX Smart PA
# MTK include path
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/audio_dsp
subdir-ccflags-y += -I$(srctree)/sound/soc/mediatek/common
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

ccflags-y += -I$(srctree)/sound/soc/codecs/aw882xx

snd-soc-aw882xx-objs += aw882xx.o
snd-soc-aw882xx-objs += aw882xx_monitor.o
snd-soc-aw882xx-objs += aw882xx_init.o
snd-soc-aw882xx-objs += aw882xx_dsp.o
snd-soc-aw882xx-objs += aw882xx_device.o
snd-soc-aw882xx-objs += aw882xx_calib.o
snd-soc-aw882xx-objs += aw882xx_bin_parse.o
snd-soc-aw882xx-objs += aw882xx_spin.o

obj-m += snd-soc-aw882xx.o
