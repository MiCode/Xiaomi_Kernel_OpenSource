load(
    "//build/kernel/kleaf:kernel.bzl",
    "kernel_build",
    "kernel_build_config",
    "kernel_images",
    "kernel_modules_install",
)
load("//build/kernel/kleaf:constants.bzl", "aarch64_outs")
load(":modules.bzl", "get_gki_modules_list", "get_kunit_modules_list")

rule_base = "kernel_aarch64_consolidate_16k"

def _gen_config_without_source_lines(build_config, target):
    rule_name = "{}.{}".format(target, build_config)
    out_file_name = rule_name + ".generated"
    native.genrule(
        name = rule_name,
        srcs = [build_config],
        outs = [out_file_name],
        cmd_bash = "sed -e '/^\\. /d' $(location {}) > $@".format(build_config),
    )

    return ":" + rule_name

def define_consolidate_16k():
    kernel_build_config(
        name = rule_base + "_build_config",
        srcs = [
            # do not sort
            "build.config.constants",
            _gen_config_without_source_lines("build.config.common", rule_base),
            "build.config.aarch64",
            "build.config.gki_consolidate.aarch64",
            _gen_config_without_source_lines("build.config.gki.aarch64", rule_base),
        ],
    )

    kernel_build(
        name = rule_base,
        outs = aarch64_outs,
        implicit_outs = [
            "scripts/sign-file",
            "certs/signing_key.pem",
            "certs/signing_key.x509",
        ],
        make_goals = [
            "Image",
            "modules",
            "Image.lz4",
            "Image.gz",
        ],
        page_size = "16k",
        module_implicit_outs = get_gki_modules_list("arm64") + get_kunit_modules_list("arm64"),
        build_config = rule_base + "_build_config",
        trim_nonlisted_kmi = False,
    )

    kernel_modules_install(
        name = "{}_modules_install".format(rule_base),
        kernel_build = ":{}".format(rule_base),
    )

    kernel_images(
        name = "{}_images".format(rule_base),
        kernel_build = ":{}".format(rule_base),
        build_system_dlkm = True,
        build_system_dlkm_flatten = True,
        system_dlkm_fs_types = ["ext4"],
        kernel_modules_install = ":{}_modules_install".format(rule_base),
    )
