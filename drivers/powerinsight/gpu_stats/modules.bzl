def register_modules(registry):
    registry.register(
        name = "drivers/powerinsight/gpu_stats",
        out = "gpu_stats.ko",
        config = "CONFIG_POWERINSIGHT_GPU_OPP",
        srcs = [
            # do not sort
            "drivers/powerinsight/gpu_stats/gpu_stats.c",
            "drivers/powerinsight/gpu_stats/gpu_stats.h",
        ],
    )
