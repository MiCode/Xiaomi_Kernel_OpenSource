load(":target_variants.bzl", "la_variants")
load(":msm_kernel_la.bzl", "define_msm_la")
load(":image_opts.bzl", "boot_image_opts")
load(":pineapple.bzl", "target_arch", "target_arch_in_tree_modules", "target_arch_consolidate_in_tree_modules", "target_arch_kernel_vendor_cmdline_extras", "target_arch_board_kernel_cmdline_extras", "target_arch_board_bootconfig_extras")
load(":xiaomi_sm8650_common.bzl", "xiaomi_common_in_tree_modules", "xiaomi_common_consolidate_in_tree_modules")

target_name = "chenfeng"

def define_chenfeng():
    _target_in_tree_modules = target_arch_in_tree_modules + \
        xiaomi_common_in_tree_modules + [
        # keep sorted
        "drivers/block/zram/zram.ko",
	"mm/zsmalloc.ko",
	"drivers/gpio/gpio-tiantong.ko",
    	"drivers/staging/binder_prio/binder_prio.ko",
	"drivers/tty/n_gsm.ko",
	"drivers/gpio/gpio-mi-t1.ko",
        "drivers/input/fingerprint/goodix_fod/goodix_fod.ko",
        "drivers/mihw/mi_sched/mi_schedule.ko",
	"drivers/staging/miev/miev.ko",
        "drivers/mihw/game/migt.ko",
	"drivers/mihw/metis/metis.ko",
        "drivers/mihw/soc/mist.ko",
        "drivers/staging/mi-log/mi_log.ko",
        "drivers/staging/mi-log/mi_exception_log.ko",
        "drivers/misc/mpbe/mpbe.ko",
        "drivers/staging/mi-perf/mi_mempool/mi_mempool.ko",
	"drivers/staging/mi-perf/mi_proc_exit/proc_exit.ko",
        "drivers/soc/qcom/fsa4480-i2c.ko",
        "drivers/input/misc/aw8622x_haptic/haptic.ko",
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
