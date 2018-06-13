obj-$(CONFIG_FS_ENCRYPTION)	+= fscrypto.o

fscrypto-y := crypto.o fname.o hooks.o keyinfo.o policy.o
fscrypto-$(CONFIG_BLOCK) += bio.o
ccflags-y += -Ifs/ext4
fscrypto-$(CONFIG_EXT4_FS_ICE_ENCRYPTION) += ext4_ice.o
