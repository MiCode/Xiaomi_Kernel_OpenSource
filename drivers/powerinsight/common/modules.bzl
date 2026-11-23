def register_modules(registry):
    registry.register(
        name = "drivers/powerinsight/common",
        out = "common.ko",
        config = "CONFIG_POWERINSIGHT",
        srcs = [
            # do not sort
            "drivers/powerinsight/common/powerinsight_modem_stats.c",
            "drivers/powerinsight/common/powerinsight_modem_stats.h",
            "drivers/powerinsight/common/powerinsight_gpu_stats.c",
            "drivers/powerinsight/common/powerinsight_gpu_stats.h",
            "drivers/powerinsight/common/powerinsight_gpu.h",
            "drivers/powerinsight/common/powerinsight_ioctl.h",
            "drivers/powerinsight/common/powerinsight_plat.h",
            "drivers/powerinsight/common/utils/powerinsight_utils.c",
            "drivers/powerinsight/common/utils/powerinsight_utils.h",
            "drivers/powerinsight/common/utils/powerinsight_hashtable.c",
            "drivers/powerinsight/common/utils/powerinsight_hashtable.h",
            "drivers/powerinsight/common/powerinsight.c",
        ],
        hdrs = [
            "drivers/powerinsight/common/powerinsight_modem_stats.h",
            "drivers/powerinsight/common/powerinsight_ioctl.h",
        ],
    )
