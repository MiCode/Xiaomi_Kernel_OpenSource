# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 The Android Open Source Project

"""
This module contains a full list of kernel modules
 compiled by GKI.
"""

# LINT.IfChange
_COMMON_GKI_MODULES_LIST = [
    # keep sorted
    "drivers/block/virtio_blk.ko",
    "drivers/bluetooth/btbcm.ko",
    "drivers/bluetooth/btqca.ko",
    "drivers/bluetooth/btsdio.ko",
    "drivers/bluetooth/hci_uart.ko",
    "drivers/char/virtio_console.ko",
    "drivers/misc/vcpu_stall_detector.ko",
    "drivers/net/can/dev/can-dev.ko",
    "drivers/net/can/slcan/slcan.ko",
    "drivers/net/can/vcan.ko",
    "drivers/net/macsec.ko",
    "drivers/net/mii.ko",
    "drivers/net/ppp/bsd_comp.ko",
    "drivers/net/ppp/ppp_deflate.ko",
    "drivers/net/ppp/ppp_generic.ko",
    "drivers/net/ppp/ppp_mppe.ko",
    "drivers/net/ppp/pppox.ko",
    "drivers/net/ppp/pptp.ko",
    "drivers/net/slip/slhc.ko",
    "drivers/net/usb/aqc111.ko",
    "drivers/net/usb/asix.ko",
    "drivers/net/usb/ax88179_178a.ko",
    "drivers/net/usb/cdc_eem.ko",
    "drivers/net/usb/cdc_ether.ko",
    "drivers/net/usb/cdc_ncm.ko",
    "drivers/net/usb/r8152.ko",
    "drivers/net/usb/r8153_ecm.ko",
    "drivers/net/usb/rtl8150.ko",
    "drivers/net/usb/usbnet.ko",
    "drivers/net/wwan/wwan.ko",
    "drivers/pps/pps_core.ko",
    "drivers/ptp/ptp.ko",
    "drivers/usb/class/cdc-acm.ko",
    "drivers/usb/mon/usbmon.ko",
    "drivers/usb/serial/ftdi_sio.ko",
    "drivers/usb/serial/usbserial.ko",
    "drivers/virtio/virtio_balloon.ko",
    "drivers/virtio/virtio_pci.ko",
    "drivers/virtio/virtio_pci_legacy_dev.ko",
    "drivers/virtio/virtio_pci_modern_dev.ko",
    "kernel/kheaders.ko",
    "lib/crypto/libarc4.ko",
    "net/6lowpan/6lowpan.ko",
    "net/6lowpan/nhc_dest.ko",
    "net/6lowpan/nhc_fragment.ko",
    "net/6lowpan/nhc_hop.ko",
    "net/6lowpan/nhc_ipv6.ko",
    "net/6lowpan/nhc_mobility.ko",
    "net/6lowpan/nhc_routing.ko",
    "net/6lowpan/nhc_udp.ko",
    "net/8021q/8021q.ko",
    "net/9p/9pnet.ko",
    "net/9p/9pnet_fd.ko",
    "net/bluetooth/bluetooth.ko",
    "net/bluetooth/hidp/hidp.ko",
    "net/bluetooth/rfcomm/rfcomm.ko",
    "net/can/can.ko",
    "net/can/can-bcm.ko",
    "net/can/can-gw.ko",
    "net/can/can-raw.ko",
    "net/ieee802154/6lowpan/ieee802154_6lowpan.ko",
    "net/ieee802154/ieee802154.ko",
    "net/ieee802154/ieee802154_socket.ko",
    "net/l2tp/l2tp_core.ko",
    "net/l2tp/l2tp_ppp.ko",
    "net/mac802154/mac802154.ko",
    "net/nfc/nfc.ko",
    "net/rfkill/rfkill.ko",
    "net/tipc/diag.ko",
    "net/tipc/tipc.ko",
    "net/tls/tls.ko",
    "net/vmw_vsock/vmw_vsock_virtio_transport.ko",
]

# Deprecated - Use `get_gki_modules_list` function instead.
COMMON_GKI_MODULES_LIST = _COMMON_GKI_MODULES_LIST

_ARM_GKI_MODULES_LIST = [
    # keep sorted
    "drivers/ptp/ptp_kvm.ko",
]

_ARM64_GKI_MODULES_LIST = [
    # keep sorted
    "arch/arm64/geniezone/gzvm.ko",
    "drivers/char/hw_random/cctrng.ko",
    "drivers/misc/open-dice.ko",
    "drivers/ptp/ptp_kvm.ko",
]
# LINT.ThenChange(android/abi_gki_protected_exports_aarch64)

_X86_GKI_MODULES_LIST = [
    # keep sorted
    "drivers/ptp/ptp_kvm.ko",
]

_X86_64_GKI_MODULES_LIST = [
    # keep sorted
    "drivers/ptp/ptp_kvm.ko",
]

# buildifier: disable=unnamed-macro
def get_gki_modules_list(arch = None):
    """ Provides the list of GKI modules.

    Args:
      arch: One of [arm, arm64, i386, x86_64].

    Returns:
      The list of GKI modules for the given |arch|.
    """
    gki_modules_list = [] + _COMMON_GKI_MODULES_LIST
    if arch == "arm":
        gki_modules_list += _ARM_GKI_MODULES_LIST
    elif arch == "arm64":
        gki_modules_list += _ARM64_GKI_MODULES_LIST
    elif arch == "i386":
        gki_modules_list += _X86_GKI_MODULES_LIST
    elif arch == "x86_64":
        gki_modules_list += _X86_64_GKI_MODULES_LIST
    else:
        fail("{}: arch {} not supported. Use one of [arm, arm64, i386, x86_64]".format(
            str(native.package_relative_label(":x")).removesuffix(":x"),
            arch,
        ))

    return gki_modules_list

_KUNIT_FRAMEWORK_MODULES = [
    "lib/kunit/kunit.ko",
]

# Common Kunit test modules
_KUNIT_COMMON_MODULES_LIST = [
    # keep sorted
    "drivers/base/regmap/regmap-kunit.ko",
    "drivers/base/regmap/regmap-ram.ko",
    "drivers/base/regmap/regmap-raw-ram.ko",
    "drivers/hid/hid-uclogic-test.ko",
    "drivers/iio/test/iio-test-format.ko",
    "drivers/input/tests/input_test.ko",
    "drivers/rtc/lib_test.ko",
    "fs/ext4/ext4-inode-test.ko",
    "fs/fat/fat_test.ko",
    "kernel/time/time_test.ko",
    "lib/kunit/kunit-example-test.ko",
    "lib/kunit/kunit-test.ko",
    # "mm/kfence/kfence_test.ko",
    "net/core/dev_addr_lists_test.ko",
    "sound/soc/soc-topology-test.ko",
    "sound/soc/soc-utils-test.ko",
]

# KUnit test module for arm64 only
_KUNIT_CLK_MODULES_LIST = [
    "drivers/clk/clk-gate_test.ko",
    "drivers/clk/clk_test.ko",
]

# buildifier: disable=unnamed-macro
def get_kunit_modules_list(arch = None):
    """ Provides the list of GKI modules.

    Args:
      arch: One of [arm, arm64, i386, x86_64].

    Returns:
      The list of KUnit modules for the given |arch|.
    """
    kunit_modules_list = _KUNIT_FRAMEWORK_MODULES + _KUNIT_COMMON_MODULES_LIST
    if arch == "arm":
        kunit_modules_list += _KUNIT_CLK_MODULES_LIST
    elif arch == "arm64":
        kunit_modules_list += _KUNIT_CLK_MODULES_LIST
    elif arch == "i386":
        kunit_modules_list += []
    elif arch == "x86_64":
        kunit_modules_list += []
    else:
        fail("{}: arch {} not supported. Use one of [arm, arm64, i386, x86_64]".format(
            str(native.package_relative_label(":x")).removesuffix(":x"),
            arch,
        ))

    return kunit_modules_list

# LINT.IfChange
_COMMON_UNPROTECTED_MODULES_LIST = [
    "drivers/block/zram/zram.ko",
    "kernel/kheaders.ko",
    "mm/zsmalloc.ko",
]
# LINT.ThenChange(android/abi_gki_protected_exports_aarch64)

# buildifier: disable=unnamed-macro
def get_gki_protected_modules_list(arch = None):
    all_gki_modules = get_gki_modules_list(arch) + get_kunit_modules_list(arch)
    unprotected_modules = _COMMON_UNPROTECTED_MODULES_LIST
    protected_modules = [mod for mod in all_gki_modules if mod not in unprotected_modules]
    return protected_modules
