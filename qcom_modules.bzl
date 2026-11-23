load(":kleaf-scripts/modules_register.bzl", "create_module_registry")
load(":modules.bzl", "register_modules")

registry = create_module_registry()
register_modules(registry)