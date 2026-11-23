target_arch_map = {
    "haotian" : "sun",
    "popsicle" : "canoe",
    "pandora" : "canoe",
    "pudding" : "canoe",
    "myron" : "canoe",
    "athens" : "canoe",
    "byron" : "canoe",
    "shuntian" : "canoe",
    "nezha" : "canoe",
    "songyuan" : "canoe",
}

targets = [
    # keep sorted
    "canoe",
    #"gen3auto",
    "autogvm",
    "bengal",
    "hamoa",
    "hamoa_la",
    "lahaina",
    "malabar",
    "monaco",
    "seraph",
    "shikra",
    "sun",
    "vienna",
    "haotian",
    "popsicle",
    "pandora",
    "pudding",
    "myron",
    "athens",
    "byron",
    "shuntian",
    "nezha",
    "songyuan",
]

la_variants = [
    # keep sorted
    "consolidate",
    "perf",
]

le_targets = [
    # keep sorted
    #"sun-allyes",
    "alor-le",
    "autogvm",
    "vienna-le",
]

le_variants = [
    # keep sorted
    #"perf-defconfig",
    "debug-defconfig",
    "defconfig",
]

vm_types = [
    "tuivm",
    "oemvm",
]

vm_target_bases = [
    "sun",
    "canoe",
    "malabar",
    "seraph",
]

vm_targets = [
    "{}-{}".format(t, vt)
    for t in vm_target_bases
    for vt in (["tuivm"] if "seraph" in t else vm_types)
]

vm_variants = [
    # keep sorted
    "debug-defconfig",
    "defconfig",
]

def get_all_la_variants():
    return [(t, v) for t in targets for v in la_variants]

def get_all_le_variants():
    return [(t, v) for t in le_targets for v in le_variants]

def get_all_vm_variants():
    return [(t, v) for t in vm_targets for v in vm_variants]

def get_all_non_la_variants():
    return get_all_le_variants() + get_all_vm_variants()

def get_all_variants():
    return get_all_la_variants() + get_all_le_variants() + get_all_vm_variants()

def get_arch_of_target(target):
    arch = target_arch_map.get(target)
    if not arch:
        arch = target
    return arch
