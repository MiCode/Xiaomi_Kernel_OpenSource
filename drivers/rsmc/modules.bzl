def register_modules(registry):
    registry.register(
        name = "drivers/rsmc/xiaomi_rsmc",
        out = "xiaomi_rsmc.ko",
        config = "CONFIG_CETC_RSMC",
        srcs = [
            "drivers/rsmc/rsmc_chiptype_select.c",
            "drivers/rsmc/x801/rsmc_device_x801.c",
            "drivers/rsmc/x801/rsmc_msg_loop_x801.c",
            "drivers/rsmc/x801/rsmc_spi_x801.c",
            "drivers/rsmc/x801/rsmc_start_x801.c",
            "drivers/rsmc/x801/rsmc_stdlib_s.c",
        ],
        hdrs = [
            "drivers/rsmc/module_type.h",
            "drivers/rsmc/x801/rsmc_device_x801.h",
            "drivers/rsmc/x801/rsmc_msg_loop_x801.h",
            "drivers/rsmc/x801/rsmc_spi_x801.h",
            "drivers/rsmc/x801/rsmc_start_x801.h",
            "drivers/rsmc/x801/rsmc_stdlib_s.h",
        ],
        deps = [
            # do not sort
            "drivers/pinctrl/qcom/pinctrl-msm",
        ],
    )
