def register_modules(registry):
    registry.register(
        name = "drivers/mimon/mimon",
        out = "mimon.ko",
        config = "CONFIG_XM_MIMON",
        srcs = [
            # do not sort
            "drivers/mimon/cpu_load_monitor.h",
            "drivers/mimon/cpu_load_monitor.c",
            "drivers/mimon/cpu_netlink.h",
            "drivers/mimon/mimon.c",
            "drivers/mimon/mimon_trace.h",
            "drivers/mimon/cpu_genetlink.c",
        ],
    )
