# Add unprotected modules corresponding to each platform
_unprotected_modules_map = {
    "sun_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "sun_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "canoe_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "canoe_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "haotian_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "haotian_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "popsicle_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "popsicle_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "pudding_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "pudding_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "pandora_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "pandora_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "myron_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "myron_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "athens_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "athens_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "byron_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "byron_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "shuntian_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "shuntian_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "lahaina_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "lahaina_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "nezha_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "nezha_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "bengal_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "bengal_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "monaco_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "monaco_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "malabar_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "malabar_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "songyuan_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "songyuan_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "seraph_consolidate": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
    "seraph_perf": [
        # keep sorted
        "drivers/block/zram/zram.ko",
        "mm/zsmalloc.ko",
    ],
}

def get_unprotected_vendor_modules_list(msm_target = None):
    """ Provides the list of unprotected vendor modules.

    Args:
      msm_target: name of target_variant platform (e.g. "sun_perf").

    Returns:
      The list of unprotected vendor modules for the given msm_target.
    """
    unprotected_modules_list = []
    for t, m in _unprotected_modules_map.items():
        if msm_target in t:
            unprotected_modules_list = [] + _unprotected_modules_map[msm_target]
    return unprotected_modules_list
