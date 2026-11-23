def register_modules(registry):
    registry.register(
        name = "drivers/firmware/cirrus/cl_dsp-core",
        out = "cl_dsp-core.ko",
        config = "CONFIG_CL_DSP",
        srcs = [
            # do not sort
            "drivers/firmware/cirrus/cl_dsp.c",
            "drivers/firmware/cirrus/cl_dsp-debugfs.c",
            "include/linux/firmware/cirrus/cl_dsp.h",
        ],
    )
