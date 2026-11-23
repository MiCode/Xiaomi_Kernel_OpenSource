load("//build/kernel/kleaf:kernel.bzl", "kernel_module_group")
load(":configs/canoe_consolidate.bzl", "canoe_consolidate_config")
load(":configs/canoe_perf.bzl", "canoe_perf_config")
load(":configs/popsicle_perf.bzl", "popsicle_perf_config")
load(":configs/popsicle_consolidate.bzl", "popsicle_consolidate_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

load(":kleaf-scripts/targets/canoe.bzl", 
        "target_arch",
        "consolidate_board_kernel_cmdline_extras",
        "consolidate_board_bootconfig_extras",
        "consolidate_kernel_vendor_cmdline_extras",
        "perf_board_kernel_cmdline_extras",
        "perf_board_bootconfig_extras",
        "perf_kernel_vendor_cmdline_extras",
)

target_name = "popsicle"

def define_popsicle():
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
        arch = target_arch,
        consolidate_config = canoe_perf_config | canoe_consolidate_config | popsicle_perf_config | popsicle_consolidate_config,
        perf_config = canoe_perf_config | popsicle_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
    )
