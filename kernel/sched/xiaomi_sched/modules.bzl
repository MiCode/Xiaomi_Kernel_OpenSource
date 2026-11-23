def register_modules(registry):
    registry.register(
        name = "kernel/sched/xiaomi_sched/common/plat_common",
        out = "plat_common.ko",
        config = "CONFIG_OS_CPU_PLAT_COMMON",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/common/plat_common/plat_hook.c",
        ],

    conditional_srcs = {
            "CONFIG_OS_CPU_DYNAMIC_TASK_SIZE": {
                False: [
                    "kernel/sched/xiaomi_sched/common/plat_common/plat_fork.c",
                ],
            },
        },

        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "kernel/sched/xiaomi_sched/qcom/rtg",
        out = "rtg.ko",
        config = "CONFIG_OS_CPU_RTG_WALT",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/qcom/rtg/rtg_pfr_cpu.c",
            "kernel/sched/xiaomi_sched/qcom/rtg/rtg_pfr_cpu.h",
        ],

        deps = [
            "kernel/sched/xiaomi_sched/qcom/perf_actuator",
            "kernel/sched/xiaomi_sched/common/plat_common",
        ],
    )

    registry.register(
        name = "kernel/sched/xiaomi_sched/common/vip_sched",
        out = "vip_sched.ko",
        config = "CONFIG_OS_CPU_SCHED_VIP",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/common/vip_sched/vip_sched.c",
            "kernel/sched/xiaomi_sched/common/vip_sched/trace_vip_sched.h",
            "kernel/sched/xiaomi_sched/common/vip_sched/vip_sched.h",
        ],

        conditional_srcs = {
            "CONFIG_OS_CPU_RTG_WALT": {
                True: [
                    "kernel/sched/xiaomi_sched/qcom/rtg/rtg_pfr_cpu.h",
                ],
            }
        },

        deps = [
            "kernel/sched/xiaomi_sched/qcom/perf_actuator",
            "kernel/sched/xiaomi_sched/common/plat_common",
            "kernel/sched/xiaomi_sched/qcom/rtg",
        ],
    )

    registry.register(
        name = "kernel/sched/xiaomi_sched/common/qis/v2/qi/os_cpu_qi",
        out = "os_cpu_qi.ko",
        config = "CONFIG_OS_CPU_QOS_INHERIT",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_binder.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_fuse.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_futex.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_mutex.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_percpu_rwsem.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_rcu.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_rwsem.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_sysctl.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_utils.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_stats.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/qi_internal.h",
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/trace_qi.h",
            "kernel/sched/xiaomi_sched/common/vip_sched/vip_sched.h",
        ],

        deps = [
                "kernel/sched/xiaomi_sched/qcom/perf_actuator",
                "kernel/sched/xiaomi_sched/common/plat_common",
                "kernel/sched/xiaomi_sched/common/vip_sched",
        ],
    )

    registry.register(
        name = "kernel/sched/xiaomi_sched/common/qis/v2/qs/os_cpu_qs",
        out = "os_cpu_qs.ko",
        config = "CONFIG_OS_CPU_QOS_SCHED",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/common/qis/v2/qs/qs.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qs/sl.c",
            "kernel/sched/xiaomi_sched/common/qis/v2/qs/qs_internal.h",
            "kernel/sched/xiaomi_sched/common/qis/v2/qs/trace_qs.h",
            "kernel/sched/xiaomi_sched/common/vip_sched/vip_sched.h",
        ],

        deps = [
            "kernel/sched/xiaomi_sched/common/qis/v2/qi/os_cpu_qi",
            "kernel/sched/xiaomi_sched/common/vip_sched",
            "kernel/sched/xiaomi_sched/common/plat_common",
            "kernel/sched/xiaomi_sched/qcom/rtg",
        ],
    )

    registry.register(
        name = "kernel/sched/xiaomi_sched/qcom/perf_actuator",
        out = "perf_actuator.ko",
        config = "CONFIG_OS_CPU_PERF_ACTUATOR",
        srcs = [
            # do not sort
            "kernel/sched/xiaomi_sched/qcom/perf_actuator/perf_actuator.c",
            "kernel/sched/xiaomi_sched/qcom/perf_actuator/perf_fbt.h",
        ],

       conditional_srcs = {
            "CONFIG_OS_CPU_FBT": {
                True: [
                    "kernel/sched/xiaomi_sched/qcom/perf_actuator/perf_fbt.c",
                ],
            },
            "CONFIG_METIS": {
                True: [
                    "kernel/sched/xiaomi_sched/qcom/perf_actuator/fbt_to_metis.c",
                ],
            }
        },
    )
