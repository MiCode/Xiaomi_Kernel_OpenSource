def register_modules(registry):
    registry.register(
        name = "kernel/sched/walt/sched-walt-debug",
        out = "sched-walt-debug.ko",
        config = "CONFIG_SCHED_WALT_DEBUG",
        srcs = [
            # do not sort
            "kernel/sched/walt/walt_debug.c",
            "kernel/sched/walt/preemptirq_long.c",
            "kernel/sched/walt/preemptirq_long.h",
            "kernel/sched/walt/walt.h",
            "kernel/sched/walt/walt_debug.h",
            "kernel/sched/walt/voter.h",
        ],
    )

    registry.register(
        name = "kernel/sched/walt/sched-walt",
        out = "sched-walt.ko",
        config = "CONFIG_SCHED_WALT",
        srcs = [
            # do not sort
            "kernel/sched/walt/walt.c",
            "kernel/sched/walt/boost.c",
            "kernel/sched/walt/sched_avg.c",
            "kernel/sched/walt/walt_halt.c",
            "kernel/sched/walt/core_ctl.c",
            "kernel/sched/walt/trace.c",
            "kernel/sched/walt/input-boost.c",
            "kernel/sched/walt/sysctl.c",
            "kernel/sched/walt/cpufreq_walt.c",
            "kernel/sched/walt/fixup.c",
            "kernel/sched/walt/walt_lb.c",
            "kernel/sched/walt/walt_rt.c",
            "kernel/sched/walt/walt_cfs.c",
            "kernel/sched/walt/walt_tp.c",
            "kernel/sched/walt/walt_config.c",
            "kernel/sched/walt/voter.c",
            "kernel/sched/walt/walt_cpufreq_cycle_cntr_driver.c",
            "kernel/sched/walt/walt_gclk_cycle_counter_driver.c",
            "kernel/sched/walt/walt_cycles.c",
            "kernel/sched/walt/debugfs.c",
            "kernel/sched/walt/pipeline.c",
            "kernel/sched/walt/smart_freq.c",
            "kernel/sched/walt/walt_storage_lb.c",
            "kernel/sched/walt/mvp_locking.c",
            "kernel/sched/walt/midpoint.c",
            "kernel/sched/walt/perf_trace_counters.h",
            "kernel/sched/walt/trace.h",
            "kernel/sched/walt/walt.h",
            "kernel/sched/walt/voter.h",
            "kernel/sched/walt/sysctl_walt_stats.h",
            "drivers/misc/hwid/hwid.h",
            "drivers/mihw/include/mi_module.h",
            "drivers/mihw/include/qos_inherit.h",
        ],
        conditional_srcs = {
            "CONFIG_SCHED_WALT_STATS": {
                True: [
                    "kernel/sched/walt/sysctl_walt_stats.c",
                ],
            },
            "CONFIG_SCHED_WALT_MIDPOINT": {
               True: [
                    "kernel/sched/walt/midpoint.c",
               ],
            },
            "CONFIG_OS_CPU_SCHED_VIP": {
                True: [
                    "kernel/sched/xiaomi_sched/common/vip_sched/vip_sched.h",
                ],
            },
            "CONFIG_OS_CPU_RTG_WALT": {
                True: [
                    "kernel/sched/xiaomi_sched/qcom/rtg/rtg_pfr_cpu.h",
                ],
            }
        },

        deps = [
            # do not sort
            "drivers/soc/qcom/socinfo",
            "drivers/soc/qcom/smem",
            "drivers/misc/hwid",
            "drivers/mihw/mi_schedule",
            "drivers/mihw/metis",
            "drivers/mihw/mist",
            "drivers/xiaomi/perf_actuator",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/os_cpu_qi",
            "kernel/sched/xiaomi_sched/common/plat_common",
            "kernel/sched/xiaomi_sched/common/vip_sched",
            "kernel/sched/xiaomi_sched/qcom/rtg",
        ],
    )