def register_modules(registry):
    registry.register(
        name = "drivers/input/fingerprint/mi_fp",
        out = "mi_fp.ko",
        config = "CONFIG_FINGERPRINT_MI_FP",
        srcs = [
            # do not sort
            "drivers/input/fingerprint/mi_fp/fp_driver.c",
            "drivers/input/fingerprint/mi_fp/fp_platform.c",
            "drivers/input/fingerprint/mi_fp/fp_netlink.c",
            "drivers/input/fingerprint/mi_fp/fp_input.c",
            "drivers/input/fingerprint/mi_fp/fp_driver.h",
            "drivers/input/fingerprint/mi_fp/fp_ultra.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/panel_event_notifier",
        ],
    )