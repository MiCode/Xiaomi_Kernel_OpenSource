load(":target_variants.bzl", "la_variants")
load(":msm_kernel_la.bzl", "define_msm_la")
load(":image_opts.bzl", "boot_image_opts")
load(":pineapple.bzl", "target_arch", "target_arch_in_tree_modules", "target_arch_consolidate_in_tree_modules", "target_arch_kernel_vendor_cmdline_extras", "target_arch_board_kernel_cmdline_extras", "target_arch_board_bootconfig_extras")
load(":xiaomi_sm8650_common.bzl", "xiaomi_common_in_tree_modules", "xiaomi_common_consolidate_in_tree_modules")
target_name = "aurora"

def define_aurora():
    _target_in_tree_modules = target_arch_in_tree_modules + \
        xiaomi_common_in_tree_modules + [
        # keep sorted
        "drivers/block/zram/zram.ko",
	"mm/zsmalloc.ko",
        "drivers/gpio/gpio-jingshang.ko",
        "drivers/gpio/gpio-mi-t1.ko",
	"drivers/input/fingerprint/goodix_fod/goodix_fod.ko",
	"drivers/mihw/game/migt.ko",
	"drivers/mihw/metis/metis.ko",
	"drivers/mihw/mi_sched/mi_schedule.ko",
	"drivers/mihw/soc/mist.ko",
        "drivers/input/stmvl53l5/stmvl53l5.ko",
        "drivers/staging/miev/miev.ko",
        "drivers/rpmsg/virtio_rpmsg_bus.ko",
        "drivers/rpmsg/rpmsg_ns.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_pcie.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_ctrl.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_rproc.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_mbox.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_busmon.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_thermal.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_memdump.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_pmic.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_mbox_test.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_regops.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_ionmap.ko",
        "drivers/media/platform/xiaomi/xm_ispv4_manager.ko",
        "drivers/staging/mi-log/mi_log.ko",
        "drivers/staging/mi-log/mi_exception_log.ko",
        "drivers/staging/mi-perf/mi_mempool/mi_mempool.ko",
	"drivers/staging/binder_prio/binder_prio.ko",
        "drivers/staging/mi-perf/mi_proc_exit/proc_exit.ko",
        ]

    _target_consolidate_in_tree_modules = _target_in_tree_modules + \
            target_arch_consolidate_in_tree_modules + \
            xiaomi_common_consolidate_in_tree_modules + [
        # keep sorted
        ]
    kernel_vendor_cmdline_extras = list(target_arch_kernel_vendor_cmdline_extras)
    board_kernel_cmdline_extras = list(target_arch_board_kernel_cmdline_extras)
    board_bootconfig_extras = list(target_arch_board_bootconfig_extras)

    for variant in la_variants:
        if variant == "consolidate":
            mod_list = _target_consolidate_in_tree_modules
        else:
            mod_list = _target_in_tree_modules
            board_kernel_cmdline_extras += ["nosoftlockup"]
            kernel_vendor_cmdline_extras += ["nosoftlockup"]
            board_bootconfig_extras += ["androidboot.console=0"]
        define_msm_la(
            msm_target = target_name,
            msm_arch = target_arch,
            variant = variant,
            in_tree_module_list = mod_list,
            boot_image_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9C000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )
        )
