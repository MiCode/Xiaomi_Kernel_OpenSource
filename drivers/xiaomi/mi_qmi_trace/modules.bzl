def register_modules(registry):
    registry.register(
        name = "drivers/xiaomi/mi_qmi_trace",
        out = "mi_qmi_trace.ko",
        config = "CONFIG_MI_QMI_TRACE",
        srcs = [
            # do not sort
            "drivers/xiaomi/mi_qmi_trace/mi_qmi_trace.c",
            "drivers/xiaomi/mi_qmi_trace/mi_qmi_trace.h",
        ],
        hdrs = [
            "drivers/xiaomi/mi_qmi_trace/mi_qmi_trace.h",
        ],
        includes = ["."],
    )
