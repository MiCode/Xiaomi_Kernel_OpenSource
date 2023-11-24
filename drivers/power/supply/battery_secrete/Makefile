subdir-ccflags-y += -I$(srctree)/drivers/power/supply/battery_secrete/ds28e30lib

obj-$(CONFIG_ONEWIRE_GPIO) += battery_auth_class.o
obj-$(CONFIG_BATT_VERIFY_BY_DS28E30) += ds28e30_verify.o
# N17 code for HQ-292265 by tongjiacheng at 20230526 start
obj-$(CONFIG_BATT_VERIFY_BY_SLG) += slg_verify.o
# N17 code for HQ-292265 by tongjiacheng at 20230526 end
# N17 code for HQ-293392 by tongjiacheng at 2023/04/28 start
obj-$(CONFIG_BATT_VERIFY_BY_ST) += stick_verify.o
# N17 code for HQ-293392 by tongjiacheng at 2023/04/28 end
obj-$(CONFIG_BATT_VERIFY) += auth_battery.o

ds28e30_verify-y += \
	ds28e30.o	\
	ds28e30lib/bignum.o	\
	ds28e30lib/deep_cover_coproc.o	\
	ds28e30lib/ecc_generate_key.o	\
	ds28e30lib/ecdsa_generic_api.o	\
	ds28e30lib/ecdsa_high.o		\
	ds28e30lib/sha256_hmac.o	\
	ds28e30lib/sha256_software.o	\
	ds28e30lib/sha256_stone.o	\
	ds28e30lib/ucl_rng.o	\
	ds28e30lib/ucl_sha256.o	\
	ds28e30lib/ucl_sha3.o	\
	ds28e30lib/ucl_sia256.o	\
	ds28e30lib/ucl_sys.o

# N17 code for HQ-293392 by tongjiacheng at 2023/04/28 start
ccflags-y := \
    -DLTC_NOTHING \
    -DLTC_NO_FILE \
    -DLTC_RIJNDAEL \
    -DLTC_SHA512 \
    -DLTC_CCM_MODE \
    -DLTC_NO_TEST \
    -DLTC_CURVE25519 \
    -DLTC_DER \
    -DLTC_MPI \
    -DLTC_OMAC \
    -DLTC_HASH_HELPERS \
    -DLTC_STICK \
    -DLTC_SOURCE \
    -DLTC_NO_WCHAR \
    -DRELEASE_BUILDING=1 \
    -fvisibility=hidden \
    -DARGTYPE=1
ifeq ($(MY_CUSTID),)
ccflags-y += -DRELEASE_CUSTID=00
else
ccflags-y += -DRELEASE_CUSTID=$(MY_CUSTID)
endif
ifeq ($(MY_RELNAME),)
ccflags-y += -DRELEASE_NAME=dev
else
ccflags-y += -DRELEASE_NAME=$(MY_RELNAME)
endif
ifeq ($(MY_RELPROD),)
ccflags-y += -DRELEASE_TYPE=DEV -DSTICK_DEBUG_LOG=1
else
ccflags-y += -DRELEASE_TYPE=$(MY_RELPROD)
endif
# List of files to build stickapi
stick_verify-objs := \
    stick.o \
    stickapi/stickapitest.o \
    stickapi/stick_api.o \
    stickapi/libsticknvm.o \
    stickapi/st1wire_kernel.o \
    stickapi/stick_core.o \
    stickapi/stick_kernel_platform.o \
    stickapi/stick_NVM_attributes.o \
    stickapi/libstickcrypto/libstickcrypto.o \
    stickapi/libstickcrypto/libstickcrypto_rng.o \
    stickapi/libstickcrypto/aes.o  \
    stickapi/libstickcrypto/aes_tab.o  \
    stickapi/libstickcrypto/ccm_add_aad.o  \
    stickapi/libstickcrypto/ccm_add_nonce.o  \
    stickapi/libstickcrypto/ccm_done.o  \
    stickapi/libstickcrypto/ccm_init.o  \
    stickapi/libstickcrypto/ccm_memory.o  \
    stickapi/libstickcrypto/ccm_process.o  \
    stickapi/libstickcrypto/ccm_reset.o  \
    stickapi/libstickcrypto/hash_memory.o  \
    stickapi/libstickcrypto/sha512.o  \
    stickapi/libstickcrypto/omac_done.o  \
    stickapi/libstickcrypto/omac_init.o  \
    stickapi/libstickcrypto/omac_memory.o  \
    stickapi/libstickcrypto/omac_memory_multi.o  \
    stickapi/libstickcrypto/omac_process.o  \
    stickapi/libstickcrypto/copy_or_zeromem.o  \
    stickapi/libstickcrypto/crypt_argchk.o  \
    stickapi/libstickcrypto/crypt_cipher_descriptor.o  \
    stickapi/libstickcrypto/crypt_cipher_is_valid.o  \
    stickapi/libstickcrypto/crypt_find_cipher.o  \
    stickapi/libstickcrypto/crypt_find_hash.o  \
    stickapi/libstickcrypto/crypt_hash_descriptor.o  \
    stickapi/libstickcrypto/crypt_hash_is_valid.o  \
    stickapi/libstickcrypto/crypt_prng_descriptor.o  \
    stickapi/libstickcrypto/crypt_prng_is_valid.o  \
    stickapi/libstickcrypto/crypt_register_cipher.o  \
    stickapi/libstickcrypto/crypt_register_hash.o  \
    stickapi/libstickcrypto/error_to_string.o  \
    stickapi/libstickcrypto/mem_neq.o  \
    stickapi/libstickcrypto/zeromem.o  \
    stickapi/libstickcrypto/ec25519_export.o  \
    stickapi/libstickcrypto/tweetnacl.o  \
    stickapi/libstickcrypto/ed25519_export.o  \
    stickapi/libstickcrypto/ed25519_import_raw.o  \
    stickapi/libstickcrypto/ed25519_make_key.o  \
    stickapi/libstickcrypto/ed25519_sign.o  \
    stickapi/libstickcrypto/ed25519_verify.o  \
    stickapi/libstickcrypto/x25519_export.o  \
    stickapi/libstickcrypto/x25519_import_raw.o  \
    stickapi/libstickcrypto/x25519_make_key.o  \
    stickapi/libstickcrypto/x25519_shared_secret.o  \
    stickapi/libstickcrypto/stick_kzalloc.o
# N17 code for HQ-293392 by tongjiacheng at 2023/04/28 end
# N17 code for HQ-292265 by tongjiacheng at 20230526 start
slg_verify-objs := \
    slg.o   \
    slglib/uECC.o
# N17 code for HQ-292265 by tongjiacheng at 20230526 end
