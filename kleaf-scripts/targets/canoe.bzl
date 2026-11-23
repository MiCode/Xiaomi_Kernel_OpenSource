load("//build/kernel/kleaf:kernel.bzl", "kernel_abi", "kernel_module_group")
load(":configs/canoe_consolidate.bzl", "canoe_consolidate_config")
load(":configs/canoe_perf.bzl", "canoe_perf_config")
load(":configs/canoe_tuivm.bzl", "canoe_tuivm_config")
load(":configs/canoe_tuivm_debug.bzl", "canoe_tuivm_debug_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

target_name = "canoe"
target_arch = "canoe"

consolidate_kernel_vendor_cmdline_extras = ["bootconfig"] + [
                #"console=ttyMSM0,115200n8",
                #"qcom_geni_serial.con_enabled=1",
                #"earlycon",
]

perf_kernel_vendor_cmdline_extras = ["bootconfig"] + [
                #"nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"
]

consolidate_board_bootconfig_extras = [
            "androidboot.serialconsole=1",
]

perf_board_bootconfig_extras = [
            "androidboot.serialconsole=0",
]

consolidate_board_kernel_cmdline_extras = [
                # do not sort
                #"console=ttyMSM0,115200n8",
                #"qcom_geni_serial.con_enabled=1",
                #"earlycon",
                "ufshcd_core.uic_cmd_timeout=2000",
]

perf_board_kernel_cmdline_extras = [#"nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"
]

def define_canoe():

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
        name = "canoe",
        consolidate_config = canoe_consolidate_config,
        perf_config = canoe_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
        consolidate_kwargs = {
            "config_path": "configs/canoe_consolidate.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/canoe_perf.bzl",
        },
    )

    kernel_abi(
        name = "canoe_perf_abi",
        kernel_build = "//common:kernel_aarch64",
        kernel_modules = [
            ":canoe_perf_all_modules",
        ],
    )

def define_canoe_tuivm():
    define_typical_vm_build(
        name = "canoe-tuivm",
        config = canoe_tuivm_config,
        debug_config = canoe_tuivm_debug_config,
        dtb_target = "canoe-tuivm",
        debug_kwargs = {
            "config_path": "configs/canoe_tuivm_debug.bzl",
        },
        config_kwargs = {
            "config_path": "configs/canoe_tuivm.bzl",
        },
    )

def define_canoe_oemvm():
    define_typical_vm_build(
        name = "canoe-oemvm",
        config = canoe_tuivm_config,
        debug_config = canoe_tuivm_debug_config,
        dtb_target = "canoe-oemvm",
        # Do not set config_path because it conflicts with canoe-tuivm
    )
