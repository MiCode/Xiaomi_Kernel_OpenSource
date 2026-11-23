load(":drivers/powerinsight/gpu_stats/modules.bzl", register_gpu_stats= "register_modules")
load(":drivers/powerinsight/gpu_freq_stats/modules.bzl", register_gpu_freq_stats = "register_modules")
load(":drivers/powerinsight/common/modules.bzl", register_common = "register_modules")
load(":drivers/powerinsight/powerinsight_interface/modules.bzl", register_interface = "register_modules")

def register_modules(registry):
    register_gpu_stats(registry)
    register_gpu_freq_stats(registry)
    register_common(registry)
    register_interface(registry)