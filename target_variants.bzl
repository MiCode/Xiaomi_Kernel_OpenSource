target_arch_map = {
    "autogvm" : "autogvm",
    "autoghgvm" : "autoghgvm",
    "blair" : "blair",
    "gen3auto" : "gen3auto",
    "gen4auto" : "gen4auto",
    "pineapple" : "pineapple",
    "pitti" : "pitti",
    "sdmsteppeauto" : "sdmsteppeauto",
    "pineapple-allyes": "pineapple-allyes",
    "pineapple-tuivm": "pineapple-tuivm",
    "pineapple-oemvm": "pineapple-oemvm",

    "shennong" : "pineapple",
    "manet" : "pineapple",
    "houji" : "pineapple",
    "aurora" : "pineapple",
    "niobe" : "niobe",
    "peridot" : "pineapple",
    "chenfeng" : "pineapple",
}

la_targets = [
    # keep sorted
    "autogvm",
    "autoghgvm",
    "shennong",
    "manet",
    "houji",
    "aurora",
    "peridot",
    "chenfeng",
    "blair",
    "gen3auto",
    "niobe",
    "pineapple",
    "pitti",
    "sdmsteppeauto",
]

la_variants = [
    # keep sorted
    "consolidate",
    "gki",
]

lxc_variants = [
    # keep sorted
    "debug-defconfig",
    "perf-defconfig",
]

lxc_targets = [
    # keep sorted
    "autoghgvm",
    "autogvm",
    "gen4auto",
]

le_targets = [
    # keep sorted
    "pineapple-allyes",
]

le_variants = [
    # keep sorted
    "perf-defconfig",
]

vm_types = [
    "tuivm",
    "oemvm",
]

vm_target_bases = [
    "pineapple",
]

vm_targets = ["{}-{}".format(t, vt) for t in vm_target_bases for vt in vm_types]

vm_variants = [
    # keep sorted
    "debug-defconfig",
    "defconfig",
]

def get_all_la_variants():
    return [(t, v) for t in la_targets for v in la_variants]

def get_all_le_variants():
    return [(t, v) for t in le_targets for v in le_variants]

def get_all_lxc_variants():
    return [(t, v) for t in lxc_targets for v in lxc_variants]

def get_all_vm_variants():
    return [(t, v) for t in vm_targets for v in vm_variants]

def get_all_variants():
    return get_all_la_variants() + get_all_le_variants() + get_all_lxc_variants() + get_all_vm_variants()

def get_arch_of_target(target):
    return target_arch_map[target] 
