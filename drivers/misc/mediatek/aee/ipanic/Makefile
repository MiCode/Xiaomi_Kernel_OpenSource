obj-$(CONFIG_MTK_AEE_IPANIC)	+= ipanic.o 
ipanic-y	:= ipanic_log.o ipanic_rom.o

ifdef CONFIG_MTK_EMMC_SUPPORT
ipanic-y	+= ipanic_emmc.o
else
ipanic-y	+= ipanic_mtd.o
endif
