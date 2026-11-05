load(":target_variants.bzl", "le_variants")
load(":msm_kernel_le.bzl", "define_msm_le")
load(":image_opts.bzl", "boot_image_opts")

target_name = "sdxkova.cpe.wkk"

def define_sdxkova_cpe_wkk():
    _sdxkova_cpe_wkk_in_tree_modules = [
        # keep sorted
    ]

    for variant in le_variants:
        mod_list = _sdxkova_cpe_wkk_in_tree_modules

        define_msm_le(
            msm_target = target_name,
            variant = variant,
            defconfig = "build.config.msm.sdxkova.cpe.wkk",
            in_tree_module_list = mod_list,
            boot_image_opts = boot_image_opts(
                boot_image_header_version = 2,
                base_address = 0x80000000,
                page_size = 4096,
            ),
        )
