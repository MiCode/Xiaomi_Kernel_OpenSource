target_arch_map = {
    "haotian" : "sun",
    "dada" : "sun",
    "nirvana" : "sun",
    "miro" : "sun",
    "xuanyuan" : "sun",
    "bixi" : "sun",
    "onyx" : "sun",
    "annibale" : "sun",
    "warsaw" : "sun",
    "luming" : "sun",
    "piano" : "sun",
    "yupei" : "sun",
    "darwin" : "sun",
}

targets = [
    # keep sorted
    "monaco",
    "parrot",
    "pineapple",
    "sun",
    "haotian",
    "dada",
    "nirvana",
    "xuanyuan",
    "miro",
    "bixi",
    "onyx",
    "annibale",
    "warsaw",
    "luming",
    "piano",
    "yupei",
    "darwin",
]

la_variants = [
    # keep sorted
    "consolidate",
    "perf",
]

le_targets = [
    # keep sorted
    "sdxkova",
    "sdxkova.cpe.wkk",
    "sun-allyes",
    "sun-le",
]

le_variants = [
    # keep sorted
    "debug-defconfig",
    "perf-defconfig",
]

vm_types = [
    "tuivm",
    "oemvm",
]

parrot_vm_types = [
    "tuivm",
]

vm_target_bases = {
    "pineapple": vm_types,
    "sun": vm_types,
    "parrot": parrot_vm_types,
}

vm_targets = ["{}-{}".format(t, vt) for t in vm_target_bases.keys() for vt in vm_target_bases[t]]

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
