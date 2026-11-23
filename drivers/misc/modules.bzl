load(":drivers/misc/isl97900_led/modules.bzl", register_isl97900_led = "register_modules")
load(":drivers/misc/lkdtm/modules.bzl", register_lkdtm = "register_modules")
load(":drivers/misc/hwid/modules.bzl", register_hwid = "register_modules")
load(":drivers/misc/dio_dma_mapper/modules.bzl", register_dio_dma_mapper= "register_modules")
def register_modules(registry):
    register_lkdtm(registry)
    register_isl97900_led(registry)
    register_hwid(registry)
    register_dio_dma_mapper(registry)

    registry.register(
        name = "drivers/misc/qseecom_proxy",
        out = "qseecom_proxy.ko",
        config = "CONFIG_QSEECOM_PROXY",
        srcs = [
            # do not sort
            "drivers/misc/qseecom_proxy.c",
        ],
    )
    registry.register(
        name = "drivers/misc/mi_ntsync",
        # GKI was using 6.12 and has a broken version of this driver. rename it to prevent conflict build.
        out = "mi_ntsync.ko",
        config = "CONFIG_MI_NTSYNC",
        srcs = [
            # do not sort
            "drivers/misc/ntsync.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/debug_symbol",
        ],
    )
