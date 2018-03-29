zram-y	:=	zcomp_lzo.o zcomp.o zram_drv.o

zram-$(CONFIG_ZRAM_LZ4_COMPRESS) += zcomp_lz4.o

zram-$(CONFIG_ZRAM_LZ4K_COMPRESS) += zcomp_lz4k.o

obj-$(CONFIG_ZRAM)	+=	zram.o
