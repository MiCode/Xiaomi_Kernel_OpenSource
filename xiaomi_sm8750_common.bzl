load("//build/kernel/kleaf:kernel.bzl", "ddk_headers")
xiaomi_common_in_tree_modules = [
    "drivers/input/fingerprint/mi_fp/mi_fp.ko"
]
def export_xiaomi_headers():
    ddk_headers(
        name = "hwid_headers",
        hdrs = [
            "drivers/misc/hwid/hwid.h",
        ],
        includes = [
            "drivers/misc/hwid",
        ],
        visibility = ["//visibility:public"],
    )
    ddk_headers(
        name = "mi_irq_headers",
        hdrs = native.glob(["kernel/irq/*.h"]),
        includes = [
            "kernel/irq",
        ],
        visibility = ["//visibility:public"],
    )
    ddk_headers(
        name = "debug_symbol_headers",
        hdrs = [
            "drivers/soc/qcom/debug_symbol.h"
        ],
        includes = [
            "drivers/soc/qcom",
        ],
        visibility = ["//visibility:public"],
    )
    ddk_headers(
        name = "miev_headers",
        hdrs = [
            "include/miev/mievent.h",
        ],
        includes = [
            "include/miev",
        ],
        visibility = ["//visibility:public"],
    )
    ddk_headers(
        name = "mi_ubt_headers",
        hdrs = [
            "include/mi_ubt/mi_ubt.h",
        ],
        includes = [
            "include/mi_ubt",
        ],
        visibility = ["//visibility:public"],
    )
    ddk_headers(
        name = "metis_headers",
        hdrs = [
            "include/linux/mi_sched.h",
        ],
        includes = [
            "include/linux",
        ],
        visibility = ["//visibility:public"],
    )
