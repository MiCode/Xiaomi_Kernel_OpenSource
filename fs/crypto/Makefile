obj-$(CONFIG_FS_ENCRYPTION)	+= fscrypto.o

ccflags-y += -Ifs/ext4
ccflags-y += -Ifs/f2fs

fscrypto-y := crypto.o \
	      fname.o \
	      hkdf.o \
	      hooks.o \
	      keyring.o \
	      keysetup.o \
	      fscrypt_ice.o \
	      keysetup_v1.o \
	      policy.o

fscrypto-$(CONFIG_BLOCK) += bio.o
