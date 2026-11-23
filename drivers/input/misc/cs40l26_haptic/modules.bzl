def register_modules(registry):
    registry.register(
        name = "drivers/input/misc/cs40l26_haptic/cs40l26-core",
        out = "cs40l26-core.ko",
        config = "CONFIG_INPUT_CS40L26",
        srcs = [
            # do not sort
            "drivers/input/misc/cs40l26_haptic/cs40l26.c",
            "drivers/input/misc/cs40l26_haptic/cs40l26-debugfs.c",
            "drivers/input/misc/cs40l26_haptic/cs40l26-sysfs.c",
            "drivers/input/misc/cs40l26_haptic/cs40l26-tables.c",
            "include/linux/mfd/cs40l26.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/cirrus/cl_dsp-core",
        ],
    )

    registry.register(
        name = "drivers/input/misc/cs40l26_haptic/cs40l26-i2c",
        out = "cs40l26-i2c.ko",
        config = "CONFIG_INPUT_CS40L26_I2C",
        srcs = [
            # do not sort
            "drivers/input/misc/cs40l26_haptic/cs40l26-i2c.c",
            "include/linux/mfd/cs40l26.h",
        ],
        deps = [
            # do not sort
            "drivers/input/misc/cs40l26_haptic/cs40l26-core",
            "drivers/firmware/cirrus/cl_dsp-core",
        ],
    )
