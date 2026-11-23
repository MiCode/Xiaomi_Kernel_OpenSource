def register_modules(registry):
    registry.register(
        name = "drivers/ir-spi/ir-spi",
        out = "ir-spi.ko",
        config = "CONFIG_IR_SPI",
        srcs = [
            # do not sort
            "drivers/ir-spi/ir-spi.c",
        ],
    )