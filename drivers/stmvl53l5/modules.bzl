def register_modules(registry):
    registry.register(
        name = "drivers/stmvl53l5/stmvl53l5",
        out = "stmvl53l5.ko",
        config = "CONFIG_STMVL53L5",
        srcs = [
            # do not sort
            "drivers/stmvl53l5/stmvl53l5_error_codes.h",
            "drivers/stmvl53l5/stmvl53l5_ioctl_defs.h",
            "drivers/stmvl53l5/stmvl53l5_load_firmware.c",
            "drivers/stmvl53l5/stmvl53l5_load_firmware.h",
            "drivers/stmvl53l5/stmvl53l5_logging.h",
            "drivers/stmvl53l5/stmvl53l5_module_dev.h",
            "drivers/stmvl53l5/stmvl53l5_module.c",
            "drivers/stmvl53l5/stmvl53l5_platform.c",
            "drivers/stmvl53l5/stmvl53l5_platform.h",
            "drivers/stmvl53l5/stmvl53l5_power.c",
            "drivers/stmvl53l5/stmvl53l5_power.h",
            "drivers/stmvl53l5/stmvl53l5_register_utils.c",
            "drivers/stmvl53l5/stmvl53l5_register_utils.h",
            "drivers/stmvl53l5/stmvl53l5_version.h",
        ],
    )