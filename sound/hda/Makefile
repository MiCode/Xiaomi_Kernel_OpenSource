snd-hda-controller-objs := hda_controller.o hda_dma.o

snd-hda-codec-y := hda_codec.o hda_jack.o hda_auto_parser.o hda_eld.o hda_sysfs.o
snd-hda-codec-$(CONFIG_PROC_FS) += hda_proc.o
snd-hda-codec-$(CONFIG_SND_CORE_HDA_HWDEP) += hda_hwdep.o
snd-hda-codec-$(CONFIG_SND_CORE_HDA_INPUT_BEEP) += hda_beep.o

# for trace-points
CFLAGS_hda_codec.o := -I$(src)
CFLAGS_hda_controller.o := -I$(src)

snd-hda-codec-generic-objs :=	hda_generic.o

# common driver
obj-$(CONFIG_SND_CORE_HDA) := snd-hda-codec.o
obj-$(CONFIG_SND_CORE_HDA) += snd-hda-controller.o

# codec drivers
obj-$(CONFIG_SND_CORE_HDA_GENERIC) += snd-hda-codec-generic.o
