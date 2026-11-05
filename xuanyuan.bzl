load(":target_variants.bzl", "la_variants")
load(":msm_kernel_la.bzl", "define_msm_la")
load(":image_opts.bzl", "boot_image_opts")
load(":xiaomi_sm8750_common.bzl", "xiaomi_common_in_tree_modules")
load(":sun.bzl", 
        "target_arch",
        "target_arch_in_tree_modules",
        "target_arch_consolidate_in_tree_modules",
        "consolidate_board_kernel_cmdline_extras",
        "consolidate_board_bootconfig_extras",
        "consolidate_kernel_vendor_cmdline_extras",
        "perf_board_kernel_cmdline_extras",
        "perf_board_bootconfig_extras",
        "perf_kernel_vendor_cmdline_extras",
)

target_name = "xuanyuan"

def define_xuanyuan():
    for variant in la_variants:
        _target_in_tree_modules = target_arch_in_tree_modules + xiaomi_common_in_tree_modules + [
            # keep sorted
            "drivers/media/rc/ir-spi.ko",
            # MIUI ADD: Stability_DebugEnhance
            "drivers/xiaomi/boottime/boottime.ko",
            "drivers/xiaomi/fault_report/fault_report.ko",
            # END Stability_DebugEnhance
            "drivers/xiaomi/mi_stack/mi_stack.ko",
            # MIUI ADD: tiantong msc06a
            "drivers/regulator/et5907mv-regulator.ko",
            "drivers/gpio/gpio-tiantong-msc06a-o1.ko",
            "drivers/regulator/hl7603-regulator.ko",
            # END tiantong msc06a
            # T1 chip
	    "drivers/xiaomi/mi_wq_dynamic_priority/mi_wq_dynamic_priority.ko",
            "drivers/gpio/gpio-mi-t1.ko",
            # END T1 chip
            "drivers/i2c/i2c-dev.ko",
            "drivers/input/stmvl53l5/stmvl53l5.ko",
            "drivers/block/zram/zram.ko",
            "drivers/xiaomi/lz4asm/lz4asm.ko",
            "block/cpq/cpq.ko",
            "drivers/xiaomi/hangdetect/hangdetect.ko",
            "mm/zsmalloc.ko",
            "drivers/xiaomi/node_cache/node_cache.ko",
            "mm/dynamic_readahead.ko",
            "drivers/staging/mi-perf/mi_mempool/mi_mempool.ko",
            "drivers/staging/mi-perf/mi_mem_all_in_one/mi_mtmr.ko",
            "drivers/staging/mi-perf/mi_mem_all_in_one/mem_fault_events.ko",
            "drivers/staging/mi-perf/mi_proc_exit/proc_exit.ko",
            #"drivers/mihw/group_load/miloadtrace.ko",
            #"drivers/mihw/game/migt.ko",
	    "drivers/mihw/dcvs_arbi/dcvs_main.ko",
	    "drivers/mihw/dcvs_arbi/sched_entity_freq.ko",
            "drivers/mihw/millet/millet_core.ko",
            "drivers/mihw/millet/millet_binder.ko",
            "drivers/mihw/millet/binder_gki.ko",
            "drivers/mihw/millet/millet_oem_cgroup.ko",
            "drivers/mihw/millet/millet_hs.ko",
            "drivers/mihw/millet/millet_sig.ko",
            "drivers/mihw/millet/millet_pkg.ko",
	    "drivers/mihw/powersave/powersave.ko",
            "drivers/mtd/mtd_blkdevs.ko",
            "drivers/mtd/parsers/ofpart.ko",
            "drivers/mtd/mtdoops.ko",
            "drivers/mtd/devices/block2mtd.ko",
            "drivers/mtd/chips/chipreg.ko",
            "drivers/mtd/mtdblock.ko",
            "drivers/mtd/mtd.ko",
            "drivers/xiaomi/bootmonitor/bootmonitor.ko",
            "drivers/xiaomi/mi_trace/mi_trace.ko",
            "drivers/xiaomi/rss_monitor/rss_monitor.ko",
            "drivers/xiaomi/mi_ubt/mi_ubt.ko",
            "drivers/xiaomi/mi_ubt/test/mi_ubt_test.ko",
            "mm/launch_boost/lb.ko",
            "drivers/xiaomi/printk_enhance/printk_enhance.ko",
            ]

        _target_consolidate_in_tree_modules = _target_in_tree_modules + \
                target_arch_consolidate_in_tree_modules + [
            # keep sorted
            "drivers/dma-buf/dma-buf-ref.ko",
            ]

        if variant == "consolidate":
            mod_list = _target_consolidate_in_tree_modules
            board_kernel_cmdline_extras = consolidate_board_kernel_cmdline_extras
            board_bootconfig_extras = consolidate_board_bootconfig_extras
            kernel_vendor_cmdline_extras = consolidate_kernel_vendor_cmdline_extras
        else:
            mod_list = _target_in_tree_modules
            board_kernel_cmdline_extras = perf_board_kernel_cmdline_extras
            board_bootconfig_extras = perf_board_bootconfig_extras
            kernel_vendor_cmdline_extras = perf_kernel_vendor_cmdline_extras

        define_msm_la(
            msm_target = target_name,
            msm_arch = target_arch,
            variant = variant,
            in_tree_module_list = mod_list,
            boot_image_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            ),
        )

