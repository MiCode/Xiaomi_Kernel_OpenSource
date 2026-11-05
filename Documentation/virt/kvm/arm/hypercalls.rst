.. SPDX-License-Identifier: GPL-2.0

===============================================
KVM/arm64-specific hypercalls exposed to guests
===============================================

This file documents the KVM/arm64-specific hypercalls which may be
exposed by KVM/arm64 to guest operating systems. These hypercalls are
issued using the HVC instruction according to version 1.1 of the Arm SMC
Calling Convention (DEN0028/C):

https://developer.arm.com/docs/den0028/c

All KVM/arm64-specific hypercalls are allocated within the "Vendor
Specific Hypervisor Service Call" range with a UID of
``28b46fb6-2ec5-11e9-a9ca-4b564d003a74``. This UID should be queried by the
guest using the standard "Call UID" function for the service range in
order to determine that the KVM/arm64-specific hypercalls are available.

``ARM_SMCCC_KVM_FUNC_FEATURES``
---------------------------------------------

Provides a discovery mechanism for other KVM/arm64 hypercalls.

+---------------------+-------------------------------------------------------------+
| Presence:           | Mandatory for the KVM/arm64 UID                             |
+---------------------+-------------------------------------------------------------+
| Calling convention: | HVC32                                                       |
+---------------------+----------+--------------------------------------------------+
| Function ID:        | (uint32) | 0x86000000                                       |
+---------------------+----------+--------------------------------------------------+
| Arguments:          | None                                                        |
+---------------------+----------+----+---------------------------------------------+
| Return Values:      | (uint32) | R0 | Bitmap of available function numbers 0-31   |
|                     +----------+----+---------------------------------------------+
|                     | (uint32) | R1 | Bitmap of available function numbers 32-63  |
|                     +----------+----+---------------------------------------------+
|                     | (uint32) | R2 | Bitmap of available function numbers 64-95  |
|                     +----------+----+---------------------------------------------+
|                     | (uint32) | R3 | Bitmap of available function numbers 96-127 |
+---------------------+----------+----+---------------------------------------------+

``ARM_SMCCC_KVM_FUNC_PTP``
----------------------------------------

See ptp_kvm.rst

``ARM_SMCCC_KVM_FUNC_HYP_MEMINFO``
----------------------------------

Query the memory protection parameters for a protected virtual machine.

+---------------------+-------------------------------------------------------------+
| Presence:           | Optional; protected guests only.                            |
+---------------------+-------------------------------------------------------------+
| Calling convention: | HVC64                                                       |
+---------------------+----------+--------------------------------------------------+
| Function ID:        | (uint32) | 0xC6000002                                       |
+---------------------+----------+----+---------------------------------------------+
| Arguments:          | (uint64) | R1 | Reserved / Must be zero                     |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R2 | Reserved / Must be zero                     |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R3 | Reserved / Must be zero                     |
+---------------------+----------+----+---------------------------------------------+
| Return Values:      | (int64)  | R0 | ``INVALID_PARAMETER (-3)`` on error, else   |
|                     |          |    | memory protection granule in bytes          |
|                     +----------+----+---------------------------------------------+
|                     | (int64)  | R1 | ``KVM_FUNC_HAS_RANGE (1)`` if MEM_SHARE and |
|                     |          |    | MEM_UNSHARE take a range argument.          |
+---------------------+----------+----+---------------------------------------------+

``ARM_SMCCC_KVM_FUNC_MEM_SHARE``
--------------------------------

Share a region of memory with the KVM host, granting it read, write and execute
permissions. The size of the region is equal to the memory protection granule
advertised by ``ARM_SMCCC_KVM_FUNC_HYP_MEMINFO`` times the number of granules
set in R2. See the ``KVM_FUNC_HAS_RANGE`` paragraph for more details about this
argument.

+---------------------+-------------------------------------------------------------+
| Presence:           | Optional; protected guests only.                            |
+---------------------+-------------------------------------------------------------+
| Calling convention: | HVC64                                                       |
+---------------------+----------+--------------------------------------------------+
| Function ID:        | (uint32) | 0xC6000003                                       |
+---------------------+----------+----+---------------------------------------------+
| Arguments:          | (uint64) | R1 | Base IPA of memory region to share          |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R2 | Number of granules to share                 |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R3 | Reserved / Must be zero                     |
+---------------------+----------+----+---------------------------------------------+
| Return Values:      | (int64)  | R0 | ``SUCCESS (0)``                             |
|                     |          |    +---------------------------------------------+
|                     |          |    | ``INVALID_PARAMETER (-3)``                  |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R1 | Number of shared granules                   |
+---------------------+----------+----+---------------------------------------------+

``ARM_SMCCC_KVM_FUNC_MEM_UNSHARE``
----------------------------------

Revoke access permission from the KVM host to a memory region previously shared
with ``ARM_SMCCC_KVM_FUNC_MEM_SHARE``. The size of the region is equal to the
memory protection granule advertised by ``ARM_SMCCC_KVM_FUNC_HYP_MEMINFO`` times
the number of granules set in R2. See the ``KVM_FUNC_HAS_RANGE`` paragraph for
more details about this argument.

+---------------------+-------------------------------------------------------------+
| Presence:           | Optional; protected guests only.                            |
+---------------------+-------------------------------------------------------------+
| Calling convention: | HVC64                                                       |
+---------------------+----------+--------------------------------------------------+
| Function ID:        | (uint32) | 0xC6000004                                       |
+---------------------+----------+----+---------------------------------------------+
| Arguments:          | (uint64) | R1 | Base IPA of memory region to unshare        |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R2 | Number of granules to unshare               |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R3 | Reserved / Must be zero                     |
+---------------------+----------+----+---------------------------------------------+
| Return Values:      | (int64)  | R0 | ``SUCCESS (0)``                             |
|                     |          |    +---------------------------------------------+
|                     |          |    | ``INVALID_PARAMETER (-3)``                  |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R1 | Number of unshared granules                 |
+---------------------+----------+----+---------------------------------------------+

``ARM_SMCCC_KVM_FUNC_MEM_RELINQUISH``
--------------------------------------

Cooperatively relinquish ownership of a memory region. The size of the
region is equal to the memory protection granule advertised by
``ARM_SMCCC_KVM_FUNC_HYP_MEMINFO``. If this hypercall is advertised
then it is mandatory to call it before freeing memory via, for
example, virtio balloon. If the caller is a protected VM, it is
guaranteed that the memory region will be completely cleared before
becoming visible to another VM.

+---------------------+-------------------------------------------------------------+
| Presence:           | Optional.                                                   |
+---------------------+-------------------------------------------------------------+
| Calling convention: | HVC64                                                       |
+---------------------+----------+--------------------------------------------------+
| Function ID:        | (uint32) | 0xC6000009                                       |
+---------------------+----------+----+---------------------------------------------+
| Arguments:          | (uint64) | R1 | Base IPA of memory region to relinquish     |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R2 | Reserved / Must be zero                     |
|                     +----------+----+---------------------------------------------+
|                     | (uint64) | R3 | Reserved / Must be zero                     |
+---------------------+----------+----+---------------------------------------------+
| Return Values:      | (int64)  | R0 | ``SUCCESS (0)``                             |
|                     |          |    +---------------------------------------------+
|                     |          |    | ``INVALID_PARAMETER (-3)``                  |
+---------------------+----------+----+---------------------------------------------+

``ARM_SMCCC_KVM_FUNC_MMIO_GUARD_*``
-----------------------------------

See mmio-guard.rst

``KVM_FUNC_HAS_RANGE``
----------------------

This flag, when set in ARM_SMCCC_KVM_FUNC_HYP_MEMINFO, indicates the guest can
pass a number of granules as an argument to:

  * ARM_SMCCC_KVM_FUNC_MEM_SHARE
  * ARM_SMCCC_KVM_FUNC_MEM_UNSHARE

In order to support legacy guests, the kernel still accepts ``0`` as a value. In
that case a single granule is shared/unshared.

When set in ARM_SMCCC_KVM_FUNC_MMIO_GUARD_INFO, indicates the guest can call the
HVCs:

  * ARM_SMCCC_KVM_FUNC_MMIO_RGUARD_MAP
  * ARM_SMCCC_KVM_FUNC_MMIO_RGUARD_UNMAP

For all those HVCs, the hypervisor is free to stop the process at any time
either because the range isn't physically contiguous or to limit the time spent
at EL2. In a such case, the number of actually shared granules is returned (R1)
and the caller can start again where it stopped, that is, the base IPA + (Number
of processed granules * protection granule size).

If the number of processed granules returned is zero (R1), an error (R0) will be
set.
