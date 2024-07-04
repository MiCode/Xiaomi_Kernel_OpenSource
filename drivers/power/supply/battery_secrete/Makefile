subdir-ccflags-y += -I$(srctree)/drivers/power/supply/battery_secrete/ds28e30lib

obj-$(CONFIG_ONEWIRE_GPIO) += battery_auth_class.o
obj-$(CONFIG_BATT_VERIFY_BY_DS28E30) += ds28e30_verify.o
obj-$(CONFIG_BATT_VERIFY_BY_SLG) += slg_verify.o
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
# N17 code for HQ-293392 by tongjiacheng at 2023/04/28 end
# N17 code for HQ-292265 by tongjiacheng at 20230526 start
slg_verify-objs := \
    slg.o   \
    slglib/uECC.o
# N17 code for HQ-292265 by tongjiacheng at 20230526 end
