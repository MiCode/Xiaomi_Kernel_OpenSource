load("//build/kernel/kleaf:kernel.bzl", "kernel_module_group")
load(":configs/sun_consolidate.bzl", "sun_consolidate_config")
load(":configs/sun_perf.bzl", "sun_perf_config")
load(":configs/haotian_consolidate.bzl", "haotian_consolidate_config")
load(":configs/haotian_perf.bzl", "haotian_perf_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

load(":kleaf-scripts/targets/sun.bzl", 
        "target_arch",
        "consolidate_board_kernel_cmdline_extras",
        "consolidate_board_bootconfig_extras",
        "consolidate_kernel_vendor_cmdline_extras",
        "perf_board_kernel_cmdline_extras",
        "perf_board_bootconfig_extras",
        "perf_kernel_vendor_cmdline_extras",
)

target_name = "haotian"

def define_haotian():
    for variant in la_variants:

        if variant == "consolidate":
            consolidate_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = consolidate_kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = consolidate_board_kernel_cmdline_extras,
                board_bootconfig_extras = consolidate_board_bootconfig_extras,
            )

        else:
            perf_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = perf_kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = perf_board_kernel_cmdline_extras,
                board_bootconfig_extras = perf_board_bootconfig_extras,
            )

    define_typical_android_build(
        name = target_name, 
        arch = target_name, 
        consolidate_config = sun_perf_config | sun_consolidate_config | haotian_perf_config | haotian_consolidate_config,
        perf_config = sun_perf_config | haotian_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
    )
