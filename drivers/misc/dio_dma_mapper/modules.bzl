def register_modules(registry):
    registry.register(
        name = "drivers/misc/dio_dma_mapper",
        out = "dio_dma_mapper.ko",
        config = "CONFIG_DEVICE_MODULES_DIO_DMABUF_MAPPER",
        srcs = [
            # do not sort
            "drivers/misc/dio_dma_mapper/dio_dma_mapper.c",
        ],

    )
