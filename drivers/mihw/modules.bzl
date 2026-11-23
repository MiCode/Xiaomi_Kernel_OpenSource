def register_modules(registry):

    registry.register(
        name = "drivers/mihw/mi_schedule",
        out = "mi_schedule.ko",
        config = "CONFIG_MI_SCHED",
        srcs = [
            # do not sort
            "drivers/mihw/mi_sched/mi_sched.c",
            "drivers/mihw/mi_sched/pkg_interface.c",
            "drivers/mihw/mi_sched/pkg.h",
            "drivers/mihw/include/mi_module.h",
            "drivers/mihw/mi_sched/group.h",
            "kernel/sched/walt/walt.h",
        ],
        conditional_srcs = {
            "CONFIG_MI_GROUP_ASSIST": {
                True: [
                    "drivers/mihw/mi_sched/mi_group.c",
                ],
            },
        },
    )

    registry.register(
        name = "drivers/mihw/metis",
        out = "metis.ko",
        config = "CONFIG_METIS",
        srcs = [
            # do not sort
            "drivers/mihw/include/mi_module.h",
            "drivers/mihw/metis/fas.h",
            "drivers/mihw/metis/gki_def.h",
            "drivers/mihw/mi_sched/group.h",
            "drivers/mihw/metis/metis_cluaff.c",
            "drivers/mihw/metis/metis_csprotect.c",
            "drivers/mihw/metis/metis_csprotect.h",
            "drivers/mihw/metis/metis_doublecyc.c",
            "drivers/mihw/metis/metis_fboost.c",
            "drivers/mihw/metis/metis_lb.c",
            "drivers/mihw/metis/metis_link.c",
            "drivers/mihw/metis/metis_sched.c",
            "drivers/mihw/metis/metis_schlat.c",
            "drivers/mihw/metis/metis_tp.c",
            "drivers/mihw/metis/metis_wakeup.c",
            "drivers/mihw/metis/metis.h",
            "drivers/mihw/metis/trace.c",
            "drivers/mihw/metis/trace.h",
            "drivers/mihw/metis/metis_common_api.c",
            "kernel/sched/walt/preemptirq_long.h",
            "kernel/sched/walt/walt.h",
            "kernel/sched/walt/walt_debug.h",
            "kernel/sched/walt/voter.h",
            "drivers/mihw/include/qos_inherit.h",
            "drivers/mihw/include/qos_sched.h",
            "drivers/mihw/include/mqos_sched.h",
        ],
        deps = [
            # do not sort
            "drivers/mihw/mi_schedule",
            "drivers/mihw/mist",
            "drivers/mihw/qis/qi/xr-qi"
        ],
    )

    registry.register(
        name = "drivers/mihw/mist",
        out = "mist.ko",
        config = "CONFIG_MI_SOC",
        srcs = [
            # do not sort
            "drivers/mihw/include/mi_module.h",
            "drivers/mihw/mist/mist_dflt.h",
            "drivers/mihw/mist/mist_dflt.c",
            "drivers/mihw/mist/mist_gflt.h",
            "drivers/mihw/mist/mist_gflt.c",
            "drivers/mihw/mist/mist_lc.h",
            "drivers/mihw/mist/mist_lc.c",
            "kernel/sched/walt/walt.h",
            "include/linux/mi_soc.h",
            "include/linux/soc/qcom/msm_adreno_devfreq.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom-dcvs",
            "drivers/clk/qcom/clk-qcom",
            "drivers/soc/qcom/dcvs/qcom-pmu-lib",
        ],
    )
    
    registry.register(
        name = "drivers/mihw/miloadtrace",
        out = "miloadtrace.ko",
        config = "CONFIG_MI_LOAD_TRACE",
        srcs = [
            # do not sort
            "drivers/mihw/include/mi_module.h",
            "drivers/mihw/mi_sched/group.h",
            "drivers/mihw/group_load/frame_weight.c",
            "drivers/mihw/group_load/cpugov_mi.c",
            "drivers/mihw/group_load/trace.c",
            "drivers/mihw/group_load/cpugov_mi.h",
            "drivers/mihw/group_load/trace.h",
            "kernel/sched/walt/preemptirq_long.h",
            "kernel/sched/walt/walt.h",
            "kernel/sched/walt/walt_debug.h",
            "kernel/sched/walt/voter.h",
        ],
        deps = [
            # do not sort
            "drivers/mihw/mi_schedule",
            "drivers/mihw/metis",
            "kernel/sched/walt/sched-walt",
        ],
    )


    registry.register(
        name = "drivers/mihw/perf_actuator",
        out = "perf_actuator.ko",
        config = "CONFIG_MQOS_PERF_ACTUATOR",
        srcs = [
            # do not sort
            "drivers/mihw/include/perf_actuator.h",
            "drivers/mihw/perf_actuator/perf_actuator.c",
        ],
    )

    registry.register(
        name = "drivers/mihw/qis/qi/xr-qi",
        out = "xr-qi.ko",
        config = "CONFIG_MQOS_INHERIT",
        srcs = [
            # do not sort
            "drivers/mihw/include/perf_actuator.h",
            "drivers/mihw/include/mqos_sched.h",
            "drivers/mihw/include/qos_inherit.h",
            "drivers/mihw/include/qos_sched.h",
            "drivers/mihw/qis/qi/qi_internal.h",
            "drivers/mihw/qis/qi/qi.c",
            "drivers/mihw/qis/qi/qi_binder.c",
            "drivers/mihw/qis/qi/qi_utils.c",
            "drivers/mihw/qis/qi/qi_futex.c",
            "drivers/mihw/qis/qi/qi_mutex.c",
            "drivers/mihw/qis/qi/qi_sysctl.c",
            "drivers/mihw/qis/qi/qi_rwsem.c",
            "drivers/mihw/qis/qi/qi_percpu_rwsem.c",
            "drivers/mihw/qis/qi/trace_qi.h",
        ],
        deps = [
            # do not sort
            "drivers/mihw/perf_actuator",
            "drivers/mihw/mi_schedule",
        ],
    )

    registry.register(
        name = "drivers/mihw/qis/qs/xr-qs",
        out = "xr-qs.ko",
        config = "CONFIG_MQOS_SCHED",
        srcs = [
            # do not sort
            "drivers/mihw/include/perf_actuator.h",
            "drivers/mihw/include/qos_inherit.h",
            "drivers/mihw/include/mqos_sched.h",
            "drivers/mihw/include/qos_sched.h",
            "drivers/mihw/qis/qs/qs.c",
            "drivers/mihw/qis/qs/qs_internal.h",
            "drivers/mihw/qis/qs/sl.c",
            "drivers/mihw/qis/qs/trace_qs.h",
        ],
        deps = [
            # do not sort
            "drivers/mihw/perf_actuator",
            "drivers/mihw/qis/qi/xr-qi",
        ],
    )

    registry.register(
        name = "drivers/mihw/dcvs_arbi/sched_entity_freq",
        out = "sched_entity_freq.ko",
        config = "CONFIG_MI_DCVS_ARBI",
        srcs = [
            # do not sort
            "drivers/mihw/dcvs_arbi/sched_entity_freq.h",
            "drivers/mihw/dcvs_arbi/sched_entity_freq_core.c",
            "drivers/mihw/dcvs_arbi/sched_entity_freq_sysfs.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/dcvs/qcom-dcvs",
            "drivers/soc/qcom/dcvs/qcom-pmu-lib",
            "kernel/sched/walt/sched-walt",
        ],
    )
