load("//build/kernel/kleaf:kernel.bzl", "kernel_module_group")
load(":configs/sun_consolidate.bzl", "sun_consolidate_config")
load(":configs/sun_perf.bzl", "sun_perf_config")
load(":configs/sun_tuivm.bzl", "sun_tuivm_config")
load(":configs/sun_tuivm_debug.bzl", "sun_tuivm_debug_config")
load(":configs/haotian_consolidate.bzl", "haotian_consolidate_config")
load(":configs/haotian_perf.bzl", "haotian_perf_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

target_name = "sun"
target_arch = "sun"

consolidate_kernel_vendor_cmdline_extras = ["bootconfig"] + [
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
]

perf_kernel_vendor_cmdline_extras = ["bootconfig"] + [
                "nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"
]

consolidate_board_bootconfig_extras = [
            "androidboot.serialconsole=1",
]

perf_board_bootconfig_extras = [
            "androidboot.serialconsole=0",
]

consolidate_board_kernel_cmdline_extras = [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
]

perf_board_kernel_cmdline_extras = ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]

def define_sun():
    for variant in la_variants:
        board_kernel_cmdline_extras = []
        board_bootconfig_extras = []

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
        name = "sun",
        consolidate_config = sun_consolidate_config,
        perf_config = sun_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
        consolidate_kwargs = {
            "config_path": "configs/sun_consolidate.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/sun_perf.bzl",
        },
    )

def define_sun_tuivm():
    define_typical_vm_build(
        name = "sun-tuivm",
        config = sun_tuivm_config,
        debug_config = sun_tuivm_debug_config,
        dtb_target = "sun-tuivm",
        debug_kwargs = {
            "config_path": "configs/sun_tuivm_debug.bzl",
        },
        config_kwargs = {
            "config_path": "configs/sun_tuivm.bzl",
        },
    )

def define_sun_oemvm():
    define_typical_vm_build(
        name = "sun-oemvm",
        config = sun_tuivm_config,
        debug_config = sun_tuivm_debug_config,
        dtb_target = "sun-oemvm",
        # Do not set config_path because it conflicts with sun-tuivm
    )
