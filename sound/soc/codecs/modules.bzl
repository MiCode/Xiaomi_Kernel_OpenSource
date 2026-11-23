def register_modules(registry):
    registry.register(
        name = "sound/soc/codecs/snd-soc-hdmi-codec",
        out = "snd-soc-hdmi-codec.ko",
        config = "CONFIG_SND_SOC_HDMI_CODEC",
        srcs = [
            # do not sort
            "sound/soc/codecs/hdmi-codec.c",
        ],
    )

    registry.register(
        name = "sound/soc/codecs/snd-soc-cs40l26",
        out = "snd-soc-cs40l26.ko",
        config = "CONFIG_SND_SOC_CS40L26",
        srcs = [
            # do not sort
            "sound/soc/codecs/cs40l26.c",
            "include/linux/mfd/cs40l26.h",
        ],
        deps = [
            # do not sort
            "drivers/input/misc/cs40l26_haptic/cs40l26-core",
            "drivers/firmware/cirrus/cl_dsp-core",
        ],
    )
