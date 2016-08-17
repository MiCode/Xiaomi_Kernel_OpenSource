/*
 * This header provides constants for binding nvidia,swgroup ID
 */

#ifndef DT_BINDINGS_IOMMU_TEGRA_SWGID_H
#define DT_BINDINGS_IOMMU_TEGRA_SWGID_H

#define SWGID_AFI 0
#define SWGID_AVPC 1
#define SWGID_DC 2
#define SWGID_DCB 3
#define SWGID_EPP 4
#define SWGID_G2 5
#define SWGID_HC 6
#define SWGID_HDA 7
#define SWGID_ISP 8
#define SWGID_ISP2 SWGID_ISP
/* UNUSED: 9 */
/* UNUSED: 10 */
#define SWGID_MPE 11
#define SWGID_MSENC SWGID_MPE
#define SWGID_NV 12
#define SWGID_NV2 13
#define SWGID_PPCS 14
#define SWGID_SATA2 15
#define SWGID_SATA 16
#define SWGID_VDE 17
#define SWGID_VI 18
#define SWGID_VIC 19
#define SWGID_XUSB_HOST 20
#define SWGID_XUSB_DEV 21
#define SWGID_A9AVP 22
#define SWGID_TSEC 23
#define SWGID_PPCS1 24
/* UNUSED: 25 */
/* UNUSED: 26 */
/* UNUSED: 27 */
/* UNUSED: 28 */
/* UNUSED: 29 */
/* UNUSED: 30 */
/* UNUSED: 31 */

/* UNUSED: 32 */
/* UNUSED: 33 */
/* UNUSED: 34 */
/* UNUSED: 35 */
/* UNUSED: 36 */
#define SWGID_DC14 37 /* 0x0x490 */
/* UNUSED: 38 */
/* UNUSED: 39 */

#define SWGID(x)	(1ULL << SWGID_##x)

#endif /* DT_BINDINGS_IOMMU_TEGRA_SWGID_H */
