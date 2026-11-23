def register_modules(registry):
    registry.register(
        name = "drivers/xiaomi/fault_report",
        out = "fault_report.ko",
        config = "CONFIG_FAULT_REPORT",
        srcs = [
            "drivers/xiaomi/fault_report/fault_report.c",
        ],
        deps = [
            # do not sort
        ],
    )
