load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "//build/kernel/kleaf:kernel.bzl",
    "ddk_config",
    "ddk_module",
    "kernel_compile_commands",
    "kernel_module_group",
    "kernel_modules_install",
)
load(
    ":kleaf-scripts/defconfig_fragment.bzl",
    "define_defconfig_fragment",
    "fragment_menuconfig",
)

def check_name_and_out(name, out):
    # 获取name最后一个/后面的名称
    name_suffix = name.split("/")[-1]

    # 检查是否一致
    if name_suffix + ".ko" != out:
        fail("Error: The name suffix '{}' does not match the out name '{}', You should equal the register name and the register out,For details, refer to the documentation https://xiaomi.f.mioffice.cn/docx/doxk4yhklJTwdfPfKUGrATgWXmg".format(name_suffix, out))


def signing_genrule(name, module, base_kernel, target_variant):
    native.genrule(
        name = name,
        srcs = [
            "{}/certs/signing_key.x509".format(base_kernel),
            "{}/scripts/sign-file".format(base_kernel),
            "signing_key.pem",
            module,
        ],
        outs = ["signed_" + name + ".ko"],
        cmd = """
        for file in $(locations {module}); do
            case $$file in
                *.ko)
                 $(location {kernel_name}/scripts/sign-file) sha1 $(location signing_key.pem) $(location {kernel_name}/certs/signing_key.x509) $$file
                 cp $$file $(OUTS)
                 ;;
            esac
        done
        touch $(OUTS)
        """.format(kernel_name = base_kernel, module = module),
    )

def create_signed_modules_group(name, signed_modules):
    native.filegroup(
        name = name,
        srcs = signed_modules,
    )

def _generate_ddk_target(
        hooks,
        module_map,
        target_variant,
        config_fragment,
        base_kernel,
        ddk_config_deps,
        implicit_config_fragment,
        config_path,
        library_names):
    native.alias(
        name = "{}_base_kernel".format(target_variant),
        actual = base_kernel,
        visibility = ["//visibility:public"],
    )

    if not ddk_config_deps:
        ddk_config_deps = []

    define_defconfig_fragment(
        name = "{}_defconfig".format(target_variant),
        out = "{}.config".format(target_variant),
        config = config_fragment,
    )

    ddk_config(
        name = "{}_config".format(target_variant),
        defconfig = ":{}_defconfig".format(target_variant),
        kconfigs = [":kconfig.msm.generated"],
        kernel_build = ":{}_base_kernel".format(target_variant),
        deps = ddk_config_deps,
    )

    if config_path:
        fragment_menuconfig(
            name = "{}_menuconfig".format(target_variant),
            ddk_config = ":{}_config".format(target_variant),
            dest = config_path,
        )

    if implicit_config_fragment:
        # config_fragment should come last so it takes priority
        # over implicit_config_fragment.
        config_fragment = implicit_config_fragment | config_fragment

    # define a merged defconfig fragment for dtb_build :(
    define_defconfig_fragment(
        name = "{}_merged_defconfig".format(target_variant),
        out = "{}_merged.config".format(target_variant),
        config = config_fragment,
    )

    # Call registered hooks
    for hook in hooks:
        hook.func(target_variant)

    matched_configurations = []
    phony_configurations = []
    module_names = {}

    for config, modules in module_map.items():
        for obj in modules:
            if config_fragment.get(config) in ["y", "m"]:
                matched_configurations.append(obj)
                module_names[obj.name] = "{}/{}".format(target_variant, obj.name)
            else:
                phony_configurations.append(obj)

    for module in matched_configurations:
        module_deps = [":{}".format(module_names.get(dep)) for dep in module.deps if module_names.get(dep)]
        library_deps = []
        for lib_dep in module.lib_deps:
            label_suffix = library_names.get(lib_dep)
            if label_suffix:
                library_deps.append(":{}".format(label_suffix))

        deps = module_deps + library_deps
        deps += [":{}_{}".format(target_variant, dep) for dep in module.hook_deps]
        src_hdrs = [src for src in module.srcs if src.endswith(".h")]
        includes = (module.includes or []) + {paths.dirname(hdr): "" for hdr in src_hdrs}.keys()

        ddk_module(
            name = "{}/{}".format(target_variant, module.name),
            out = module.out,
            srcs = module.srcs,
            conditional_srcs = module.conditional_srcs,
            deps = deps + [
                # do not sort
                ":additional_msm_headers",
                "//common:all_headers",
            ],
            includes = includes,
            kernel_build = ":{}_base_kernel".format(target_variant),
            config = ":{}_config".format(target_variant),
            visibility = ["//visibility:public"],
            **module.extra_args
        )

    for module in phony_configurations:
        native.alias(
            name = "{}/{}".format(target_variant, module.name),
            actual = ":all_headers",
            visibility = ["//visibility:public"],
        )
    kernel_module_group(
        name = "{}_all_modules".format(target_variant),
        srcs = module_names.values(),
        visibility = ["//visibility:public"],
    )

    kernel_modules_install(
        name = "{}_modules_install".format(target_variant),
        kernel_modules = [":{}_all_modules".format(target_variant)],
        outs = ["modules.dep"],
    )

    kernel_compile_commands(
        name = "{}_compile_commands".format(target_variant),
        deps = [":{}_all_modules".format(target_variant)],
    )

    signed_modules = []
    if "kernel_aarch64_qtvm" in base_kernel:
        for module in module_names.values():
            signing_genrule(name = "sign_" + module, module = module, base_kernel = base_kernel, target_variant = target_variant)
            signed_modules.append("sign_" + module)

        create_signed_modules_group(
            name = "{}_signed_modules".format(target_variant),
            signed_modules = signed_modules,
        )

    return [module.name for module in matched_configurations]

def create_module_registry():
    module_map = {}
    hooks = []

    def register_hook(hook):
        hooks.append(hook)

    def register(
            name,
            srcs,
            out,
            config = None,
            conditional_srcs = None,
            deps = None,
            lib_deps = None,
            hook_deps = None,
            includes = None,
            **kwargs):
        """Register a module with the registry.

        Args:
          name: A unique name for the module.
            For example: drivers/firmware/qcom/qcom_scm
            Conventionally, we do not add ".ko" suffix
          srcs: A list of source and header files to compile the module.
            These sources are "module-private" and are not exported to dependent
            modules.
          out: Desired name of the ko
            For example: qcom_scm.ko
          config: A Kconfig symbol which needs to be enabled for this module to
            be compiled.
            Optional. If unspecified, the module will be built for every target.
            For example: CONFIG_QCOM_SCM
          conditional_srcs: A dictionary mapping Kconfig symbols to additional
            sources which will be compiled.
            Note that the entire module is already dependent on the `config`
            symbol, and do need to again specify the config symbol.
            Example:
                conditional_srcs = {
                    "CONFIG_QTEE_SHM_BRIDGE": {
                        True: [
                            # do not sort
                            "drivers/firmware/qcom/qtee_shmbridge.c",
                        ],
                    },
                },
          deps: List of dependent modules, including optional dependencies.
            Note that transitive dependencies do not need to be listed: If you
            only depend on module_foo, and module_foo depends on module_bar,
            you need only list module_foo. The initial scripts to create the
            modules.bzl did *not* simplify the deps list and (unnecessarily)
            included transitive dependencies.
            Example:
              deps = [
                # do not sort
                "arch/arm64/gunyah/gunyah_hypercall",
              ]
          lib_deps: List of dependent DDK libraries.
            These are libraries defined using libraries_register.bzl.
            The names should match the "logical" library name (e.g. "pkvm-geni-hyp-lib")
            not the full target name. These are resolved using the `library_names` map
            passed to `define_modules`.
          hook_deps: List of dependent targets defined in hooks.
            A "target_variant_" prefix is added for each dependency before
            passing to ddk_module() (See _generate_ddk_target()).
          includes: See ddk_module() documentation.
          **kwargs: Additional ddk_module() arguments. See ddk_module() documentation.
        """
        check_name_and_out(name, out)

        if not module_map.get(config):
            module_map[config] = []
        module_map[config].append(struct(
            name = name,
            srcs = srcs,
            out = out,
            config = config,
            conditional_srcs = conditional_srcs,
            deps = deps or [],
            lib_deps = lib_deps or [],
            hook_deps = hook_deps or [],
            includes = includes,
            extra_args = kwargs,
        ))

    def define_modules(
            target_variant,
            config_fragment,
            base_kernel,
            ddk_config_deps = None,
            implicit_config_fragment = None,
            config_path = None,
            library_names = None):
        """Define register modules for a target/variant.

        Creates the following rules:
          {target_variant}_all_modules - kernel_module_group of all enabled modules
          {target_variant}_base_kernel - alias to `base_kernel`
          {target_variant}_config - ddk_config from the config_fragment and base_kernel
          {target_variant}/{module_name} - ddk_module for the target/variant

        ddk_config_deps and implicit_config_fragment allow other (base) ddk
        configurations to be applied. The example use case here is for the
        "perf" and "debug" variants. The debug variant can add:
          ddk_config_deps = ["target_perf_config"],
          implicit_config_fragment = target_perf_config,
        The ddk_config_deps ensures that the real .config is updated with the
        perf configuration. implicit_config_fragment ensures the additional
        modules are enabled.

        Args:
            target_variant: Base name of the target
            config_fragment: A dictionary containg Kconfig symbols and their values.
              Analogous to a defconfig fragment, but as a starlark dictionary.
              See the files under configs/
            base_kernel: A kernel_build() to base the module build.
            ddk_config_deps: Additional dependencies to pass to ddk_config().
            implicit_config_fragment: Additional Kconfig symbols to select
              from the module dictionary. See note above.
            config_path: Path to the file which declares config_fragment.
              Optional. If set, an executable {target_variant}_menuconfig is created
              which can run menuconfig and update the config_path file.
            library_names: A dictionary mapping logical library names to their Bazel targets.
              This is used to resolve `lib_deps` in registered modules.
              Typically generated by `android_build.bzl` from `library_registry.define_libraries()`.

        Returns:
            The list of all enabled modules *without* the target_variant/ prefix.
            e.g. ["drivers/firmware/qcom/qcom_scm"]
        """
        return _generate_ddk_target(
            hooks,
            module_map,
            target_variant,
            config_fragment,
            base_kernel,
            ddk_config_deps,
            implicit_config_fragment,
            config_path,
            library_names or {},
        )

    return struct(
        module_map = module_map,
        register = register,
        register_hook = register_hook,
        get = module_map.get,
        define_modules = define_modules,
    )
