def register_modules(registry):
    registry.register(
        name = "drivers/usb/typec/mux/ps883x",
        out = "ps883x.ko",
        config = "CONFIG_TYPEC_MUX_PS883X",
        srcs = [
            # do not sort
            "drivers/usb/typec/mux/ps883x.c",
        ],
    )
