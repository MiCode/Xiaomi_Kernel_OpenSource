load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//build:msm_kernel_extensions.bzl", "define_extras", "export_init_boot_prebuilt", "get_vendor_ramdisk_binaries")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_genrule")
load(
    "//build/kernel/kleaf:kernel.bzl",
    "ddk_headers",
    "kernel_build_config",
    "kernel_images",
    "merged_kernel_uapi_headers",
    "super_image",
    "unsparsed_image",
)
load(":kleaf-scripts/abl.bzl", "define_abl_dist")
load(":kleaf-scripts/avb_boot_img.bzl", "avb_sign_boot_image")
load(":kleaf-scripts/consolidate.bzl", "define_consolidated_kernel")
load(":kleaf-scripts/dtbs.bzl", "define_qcom_dtbs")
load(":kleaf-scripts/modules_unprotected.bzl", "get_unprotected_vendor_modules_list")
load(":kleaf-scripts/msm_dtc.bzl", "define_dtc_dist")
load(":kleaf-scripts/techpack_modules.bzl", "define_techpack_modules")
load(":qcom_libraries.bzl", "library_registry")
load(":qcom_modules.bzl", "registry")
load(":kleaf-scripts/merge_list_files.bzl", "merge_list_files")

def define_common_android_rules():
    write_file(
        name = "qcom_la_image_config",
        out = "la.image.config",
        content = [
            "KERNEL_DIR=common",
            "SOC_DIR=soc-repo",
            "KERNEL_BINARY=Image",
            "DO_NOT_STRIP_MODULES=0",
            "",
        ],
    )

    kernel_build_config(
        name = "build.config.qcom.la.images",
        srcs = [
            # do not sort
            ":qcom_la_image_config",
            "//common:build.config.gki.aarch64",
        ],
    )

    define_consolidated_kernel()

def define_single_android_build(
        name,
        arch,
        variant,
        config_fragment,
        base_kernel,
        build_img_opts = None,
        dtb_target = None,
        ddk_config_deps = None,
        implicit_config_fragment = None,
        config_path = None):
    stem = "{}_{}".format(name, variant)

    library_targets = library_registry.define_libraries(
        target_variant = stem,
        config_fragment = config_fragment,
        base_kernel = base_kernel,
        ddk_config_deps = ddk_config_deps,
        implicit_config_fragment = implicit_config_fragment,
        config_path = config_path,
    )

    library_names = {}
    for t in library_targets:
        if t.startswith(stem + "_"):
            logical = t[len(stem) + 1:]
            library_names[logical] = t
        else:
            parts = t.split("/", 1)
            if len(parts) > 1:
                library_names[parts[1]] = t
            else:
                fail("Library target {} does not match stem {}_".format(t, stem))

    modules = registry.define_modules(
        stem,
        config_fragment,
        base_kernel,
        ddk_config_deps,
        implicit_config_fragment,
        config_path = config_path,
        library_names = library_names,
    )

    hermetic_genrule(
        name = "{}_vendor_dlkm_modules_list_generated".format(stem),
        srcs = [],
        outs = ["modules.list.vendor_dlkm.{}".format(stem)],
        cmd = """
          echo "" > "$@"
          for module in {mod_list}; do
            basename "$$module".ko >> "$@"
          done
        """.format(mod_list = " ".join(modules)),
    )

    if dtb_target:
        dtb_list, dtbo_list = define_qcom_dtbs(
            stem = stem,
            target = dtb_target,
            defconfig = "//common:arch/arm64/configs/gki_defconfig",
            cmdline = build_img_opts.kernel_vendor_cmdline_extras if build_img_opts else [""],
        )
    else:
        dtb_list = None
        dtbo_list = None

    native.alias(
        name = "{}_abl".format(stem),
        actual = "//bootable/bootloader/edk2:{}_abl".format(stem),
    )

    kernel_build_config(
        name = "{}_build_config".format(stem),
        srcs = [
            # do not sort
            ":qcom_la_image_config",
            "//common:build.config.gki.aarch64",
            "build.config.msm.common",
            "build.config.msm.perf",
        ],
    )

    if build_img_opts != None:
        board_cmdline_extras = " ".join(build_img_opts.board_kernel_cmdline_extras)

        if board_cmdline_extras:
            hermetic_genrule(
                name = "{}_extra_cmdline".format(stem),
                outs = ["board_extra_cmdline_{}".format(stem)],
                cmd = """
                    echo {} > "$@"
                """.format(board_cmdline_extras),
            )

        board_bc_extras = " ".join(build_img_opts.board_bootconfig_extras)

        if board_bc_extras:
            hermetic_genrule(
                name = "{}_extra_bootconfig".format(stem),
                outs = ["board_extra_bootconfig_{}".format(stem)],
                cmd = """
                    echo {} > "$@"
                """.format(board_bc_extras),
            )

    # Regenerate the follow  list
    #   modules.list.msm.{}
    #   modules.systemdlkm_blocklist.msm.{}
    #   modules.vendor_blocklist.msm.{}
    prefixes = [
        "modules-lists/modules.list.msm",
        "modules-lists/modules.systemdlkm_blocklist.msm",
        "modules-lists/modules.vendor_blocklist.msm",
    ]
    modules_list_name = {}
    for prefix in prefixes:
        modules_list_name[prefix] = "{prefix}.{target}_generated".format(prefix = prefix, target = stem)
        files = [ prefix + ".{}".format(name) ] if name == arch else [
            prefix + ".{}".format(arch),
            prefix + ".{}".format(name),
        ]
        merge_list_files( name = modules_list_name[prefix], files = files )

    copy_file(
        name = "{}_system_dlkm_blocklist".format(stem),
        src = ":{}".format(modules_list_name["modules-lists/modules.systemdlkm_blocklist.msm"]),
        out = "{}/system_dlkm.modules.blocklist".format(stem),
    )

    kernel_images(
        name = "{}_images".format(stem),
        kernel_modules_install = ":{}_modules_install".format(stem),
        kernel_build = "{}_dtb_build".format(stem),
        base_kernel_images = "{}_images".format(base_kernel),
        dtbo_srcs = [":{}_dtb_build/{}".format(stem, dtbo) for dtbo in dtbo_list] if dtbo_list else None,
        build_vendor_boot = True,
        build_vendor_kernel_boot = False,
        build_initramfs = True,
        build_dtbo = True,
        build_vendor_dlkm = True,
        dedup_dlkm_modules = True,  # removes system_dlkm modules from vendor_dlkm
        modules_list = ":{}".format(modules_list_name["modules-lists/modules.list.msm"]),
        vendor_dlkm_modules_list = ":{}_vendor_dlkm_modules_list_generated".format(stem),
        system_dlkm_modules_blocklist = ":{}".format(modules_list_name["modules-lists/modules.systemdlkm_blocklist.msm"]),
        vendor_dlkm_modules_blocklist = ":{}".format(modules_list_name["modules-lists/modules.vendor_blocklist.msm"]),
        vendor_ramdisk_binaries = get_vendor_ramdisk_binaries(stem),
        deps = [
            ":{}".format(modules_list_name["modules-lists/modules.list.msm"]),
            ":{}".format(modules_list_name["modules-lists/modules.systemdlkm_blocklist.msm"]),
            ":{}".format(modules_list_name["modules-lists/modules.vendor_blocklist.msm"]),
        ],
    )

    avb_sign_boot_image(
        name = "{}_avb_sign_boot_image".format(stem),
        artifacts = "{}_gki_artifacts".format(base_kernel),
        avbtool = "//prebuilts/kernel-build-tools:avbtool",
        key = "//tools/mkbootimg:gki/testdata/testkey_rsa4096.pem",
        props = [
            "com.android.build.boot.os_version:13",
            "com.android.build.boot.security_patch:2023-05-05",
        ],
        boot_partition_size = build_img_opts.boot_partition_size,
    )

    native.filegroup(
        name = "{}_system_dlkm_image_file".format(stem),
        srcs = ["{}_images".format(base_kernel)],
        output_group = "system_dlkm.flatten.ext4.img",
    )

    native.filegroup(
        name = "{}_vendor_dlkm_image_file".format(stem),
        srcs = [":{}_images".format(stem)],
        output_group = "vendor_dlkm.img",
    )

    super_image(
        name = "{}_super_image".format(stem),
        system_dlkm_image = ":{}_system_dlkm_image_file".format(stem),
        vendor_dlkm_image = ":{}_vendor_dlkm_image_file".format(stem),
        super_img_size = 0x22000000,
    )

    unsparsed_image(
        name = "{}_unsparsed_image".format(stem),
        src = "{}_super_image".format(stem),
        out = "super_unsparsed.img",
    )

    hermetic_genrule(
        name = "{}_merge_msm_uapi_headers".format(stem),
        srcs = [
            # do not sort
            ":{}_merged_kernel_uapi_headers".format(stem),
            "msm_uapi_headers",
        ],
        outs = ["{}_kernel-uapi-headers.tar.gz".format(stem)],
        cmd = """
            mkdir -p intermediate_dir
            for file in $(SRCS)
            do
            tar xf $$file -C intermediate_dir
            done
            tar czf $(OUTS) -C intermediate_dir usr/
            rm -rf intermediate_dir
        """,
    )

    merged_kernel_uapi_headers(
        name = "{}_merged_kernel_uapi_headers".format(stem),
        kernel_build = ":{}_dtb_build".format(stem),
    )

    hermetic_genrule(
        name = "{}_tar_kernel_headers".format(stem),
        srcs = [
            # do not sort
            ":additional_msm_headers",
            "//common:all_headers",
        ],
        outs = ["{}_kernel-headers.tar.gz".format(stem)],
        cmd = """
            mkdir -p kernel-headers
            for file in $(SRCS)
            do
            cp -D $$file kernel-headers
            done
            tar czf $(OUTS) kernel-headers
            rm -rf kernel-headers
        """,
    )

    dist_data = [
        "{}_gki_artifacts".format(base_kernel),
        ":{}_modules_install".format(stem),
        "{}_dtb_build".format(stem),
        ":{}_images".format(stem),
        "{}_images".format(base_kernel),
        "{}_super_image".format(stem),
        "{}_unsparsed_image".format(stem),
        "{}_avb_sign_boot_image".format(stem),
        "{}_merge_msm_uapi_headers".format(stem),
        "{}_dtb_build_config".format(stem),
        "{}_tar_kernel_headers".format(stem),
        "{}_system_dlkm_blocklist".format(stem),
    ] + [
        ":{}/{}".format(stem, module)
        for module in modules
    ]

    vendor_dlkm_module_unprotected_list = get_unprotected_vendor_modules_list(stem)
    vendor_unprotected_dlkm = " ".join(vendor_dlkm_module_unprotected_list)
    if vendor_unprotected_dlkm:
        write_file(
            name = "{}_vendor_dlkm_module_unprotectedlist".format(stem),
            out = "{}/vendor_dlkm.modules.unprotectedlist".format(stem),
            content = [vendor_unprotected_dlkm, ""],
        )
        dist_data.extend(["{}_vendor_dlkm_module_unprotectedlist".format(stem)])

    if build_img_opts != None:
        board_cmdline_extras = " ".join(build_img_opts.board_kernel_cmdline_extras)

        if board_cmdline_extras:
            dist_data.append("{}_extra_cmdline".format(stem))

        board_bc_extras = " ".join(build_img_opts.board_bootconfig_extras)

        if board_bc_extras:
            dist_data.append("{}_extra_bootconfig".format(stem))

    copy_to_dist_dir(
        name = "{}_dist".format(stem),
        data = dist_data,
        dist_dir = "out/msm-kernel-{}-{}/dist".format(name, variant),
        flat = True,
        log = "info",
        allow_duplicate_filenames = True,
        mode_overrides = {
            # do not sort
            "**/*.elf": "755",
            "**/vmlinux": "755",
            "**/Image": "755",
            "**/*.dtb*": "755",
            "**/LinuxLoader*": "755",
            "**/*": "644",
        },
    )

    define_abl_dist(stem, name, variant)

    define_dtc_dist(stem, name, variant)

    define_techpack_modules(name, arch, variant)

    define_extras(stem, kbuild_config = base_kernel)

    export_init_boot_prebuilt(name, variant)

def define_android_build(
        name,
        arch,
        configs,
        **kwargs):
    for (variant, options) in configs.items():
        define_single_android_build(
            name = name,
            arch = arch,
            variant = variant,
            **(options | kwargs)
        )

def define_typical_android_build(
        name,
        perf_config,
        consolidate_config,
        arch = None,
        perf_kwargs = None,
        consolidate_kwargs = None,
        consolidate_build_img_opts = None,
        perf_build_img_opts = None,
        **kwargs):
    """Define a typical Android build target.

    Defines a typical Android build with two variants: one optimized for
    performance and another that has uses the debug(consolidate) kernel.

    Args:
      name: The name of this target.
      perf_config: The configuration to use when building the performance (GKI)
        build.
      consolidate_config: The configuration to use when building the
        consolidate (debug) build.
      perf_kwargs: Additional keyword arguments to pass to
        `define_single_android_build` when building the performance variant.
      consolidate_kwargs: Additional keyword arguments to pass to
        `define_single_android_build` when building the consolidate variant.
      **kwargs: Additional keyword arguments to pass to
        `define_single_android_build` for all variants.
    """

    if perf_kwargs == None:
        perf_kwargs = dict()
    if consolidate_kwargs == None:
        consolidate_kwargs = dict()

    # See b/370450569#comment34
    common_info = "{}_common_info".format(name)
    ddk_headers(
        name = common_info,
        kconfigs = [":kconfig.msm.generated"],
        defconfigs = [":{}_perf_defconfig".format(name)],
    )

    if arch == None:
        arch = name

    define_android_build(
        name,
        arch,
        configs = {
            "perf": {
                "config_fragment": perf_config,
                "base_kernel": ":msm_kernel_build",
                "build_img_opts": perf_build_img_opts,
                "ddk_config_deps": [common_info],
            } | perf_kwargs,
            "consolidate": {
                "config_fragment": consolidate_config,
                "base_kernel": "//soc-repo:kernel_aarch64_consolidate",
                "build_img_opts": consolidate_build_img_opts,
                "ddk_config_deps": [common_info],
                "implicit_config_fragment": perf_config,
            } | consolidate_kwargs,
        },
        dtb_target = name,
        **kwargs
    )
