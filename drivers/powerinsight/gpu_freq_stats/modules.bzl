def register_modules(registry):
    registry.register(
        name = "drivers/powerinsight/gpu_freq_stats",
        out = "gpu_freq_stats.ko",
        config = "CONFIG_POWERINSIGHT_GPU_FREQ_STATS",
        srcs = [
            # do not sort
            "drivers/powerinsight/gpu_freq_stats/gpu_freq_stats.c",
            "drivers/powerinsight/gpu_freq_stats/powerinsight_gpu_freq.c",
            "drivers/powerinsight/gpu_freq_stats/powerinsight_gpu_freq.h",
            "drivers/powerinsight/common/powerinsight_plat.h",
            "drivers/powerinsight/common/powerinsight_gpu.h",
            "drivers/powerinsight/gpu_stats/gpu_stats.h",
            "drivers/powerinsight/common/powerinsight_gpu_stats.h",
        ],
        deps = [
            # do not sort
            "drivers/powerinsight/common",
            "drivers/powerinsight/gpu_stats",
        ],
    )
