
def register_modules(registry):
    registry.register(
        name = "drivers/ntag/fm_ntag_i2c",
        out = "fm_ntag_i2c.ko",
        config = "CONFIG_FM_NTAG_I2C",
        srcs = [
            # do not sort
            "drivers/ntag/fm19511/ntag_common.c",
            "drivers/ntag/fm19511/ntag_common.h",
            "drivers/ntag/fm19511/ntag_i2c_drv.c",
            "drivers/ntag/fm19511/ntag_i2c_drv.h",
        ],
    )
