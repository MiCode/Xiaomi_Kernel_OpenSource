load(":drivers/ntag/fm19511/modules.bzl", register_fm19511= "register_modules")

def register_modules(registry):
    register_fm19511(registry)
