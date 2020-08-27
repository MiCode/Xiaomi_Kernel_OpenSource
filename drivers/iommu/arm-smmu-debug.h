/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#define ARM_SMMU_TESTBUS_SEL			0x25E4
#define ARM_SMMU_TESTBUS			0x25E8
#define ARM_SMMU_TESTBUS_SEL_HLOS1_NS		0x8
#define DEBUG_TESTBUS_SEL_TBU			0x50
#define DEBUG_TESTBUS_TBU			0x58
#define CLIENT_DEBUG_SR_HALT_ACK		0x24

#define TCU_PTW_TESTBUS				(0x1 << 8)
#define TCU_CACHE_TESTBUS			~TCU_PTW_TESTBUS
#define TCU_PTW_TESTBUS_SEL			(0x1 << 1)
#define TCU_PTW_INTERNAL_STATES			3
#define TCU_PTW_TESTBUS_SEL2			3
#define TCU_PTW_QUEUE_START			32
#define TCU_PTW_QUEUE_SIZE			32
#define TCU_CACHE_TESTBUS_SEL			0x1
#define TCU_CACHE_LOOKUP_QUEUE_SIZE		32
#define TCU_CLK_TESTBUS_SEL			0x200

#define TBU_CLK_GATE_CONTROLLER_TESTBUS_SEL	0x1
#define TBU_QNS4_A2Q_TESTBUS_SEL		(0x1 << 1)
#define TBU_QNS4_Q2A_TESTBUS_SEL		(0x1 << 2)
#define TBU_MULTIMASTER_QCHANNEL_TESTBUS_SEL	(0x1 << 3)
#define TBU_CLK_GATE_CONTROLLER_TESTBUS		(0x1 << 6)
#define TBU_QNS4_A2Q_TESTBUS			(0x2 << 6)
#define TBU_QNS4_Q2A_TESTBUS			(0x5 << 5)
#define TBU_MULTIMASTER_QCHANNEL_TESTBUS	(0x3 << 6)
#define TBU_QNS4_BRIDGE_SIZE			32

enum tcu_testbus {
	PTW_AND_CACHE_TESTBUS,
	CLK_TESTBUS,
};

enum testbus_sel {
	SEL_TCU,
	SEL_TBU,
};

enum testbus_ops {
	TESTBUS_SELECT,
	TESTBUS_OUTPUT,
};

#define ARM_SMMU_TNX_TCR_CNTL		0x130
#define ARM_SMMU_CAPTURE1_MASK(i)	(0x100 + (0x8)*(i-1))
#define ARM_SMMU_CAPTURE1_MATCH(i)	(0x118 + (0x8)*(i-1))
#define ARM_SMMU_CAPTURE_SNAPSHOT(i, j)	((0x138 + (0x10)*i) + j*0x8)
#define APPS_SMMU_TNX_TCR_CNTL_2	0x178

#define NO_OF_MASK_AND_MATCH		0x3
#define NO_OF_CAPTURE_POINTS		0x4
#define REGS_PER_CAPTURE_POINT		0x2
#define INTR_CLR			(1 << 0)
#define RESET_VALID			(1 << 7)

#ifdef CONFIG_ARM_SMMU

u32 arm_smmu_debug_tbu_testbus_select(void __iomem *tbu_base,
		void __iomem *tcu_base, u32 testbus_version,
		bool write, u32 reg);
u32 arm_smmu_debug_tbu_testbus_output(void __iomem *tbu_base,
					u32 testbus_version);
u32 arm_smmu_debug_tcu_testbus_select(phys_addr_t phys_addr,
		void __iomem *tcu_base, enum tcu_testbus testbus,
		bool write, u32 val);
u32 arm_smmu_debug_tcu_testbus_output(phys_addr_t phys_addr);
void arm_smmu_debug_dump_tbu_testbus(struct device *dev, void __iomem *tbu_base,
		void __iomem *tcu_base, int tbu_testbus_sel,
		u32 testbus_version);
void arm_smmu_debug_dump_tcu_testbus(struct device *dev, phys_addr_t phys_addr,
			void __iomem *tcu_base, int tcu_testbus_sel);
void arm_smmu_debug_set_tnx_tcr_cntl(void __iomem *tbu_base, u64 val);
unsigned long arm_smmu_debug_get_tnx_tcr_cntl(void __iomem *tbu_base);
unsigned long arm_smmu_debug_get_tnx_tcr_cntl_2(void __iomem *tbu_base);
void arm_smmu_debug_set_mask_and_match(void __iomem *tbu_base, u64 sel,
					u64 mask, u64 match);
void arm_smmu_debug_get_mask_and_match(void __iomem *tbu_base,
					u64 *mask, u64 *match);
void arm_smmu_debug_get_capture_snapshot(void __iomem *tbu_base,
		u64 snapshot[NO_OF_CAPTURE_POINTS][REGS_PER_CAPTURE_POINT]);
void arm_smmu_debug_clear_intr_and_validbits(void __iomem *tbu_base);
#else
static inline u32 arm_smmu_debug_tbu_testbus_select(void __iomem *tbu_base,
		void __iomem *tcu_base,	u32 testbus_version, bool write,
		u32 val)
{
}
static inline u32 arm_smmu_debug_tbu_testbus_output(void __iomem *tbu_base,
						u32 testbus_version)
{
}
u32 arm_smmu_debug_tcu_testbus_select(phys_addr_t phys_addr,
		void __iomem *tcu_base,	enum tcu_testbus testbus,
		bool write, u32 val)
{
}
static inline u32 arm_smmu_debug_tcu_testbus_output(phys_addr_t phys_addr)
{
}
static inline void arm_smmu_debug_dump_tbu_testbus(struct device *dev,
			void __iomem *tbu_base, void __iomem *tcu_base,
			int tbu_testbus_sel, u32 testbus_version)
{
}
static inline void arm_smmu_debug_dump_tcu_testbus(struct device *dev,
			phys_addr_t phys_addr, void __iomem *tcu_base,
			int tcu_testbus_sel)
{
}
void arm_smmu_debug_set_tnx_tcr_cntl(void __iomem *tbu_base, u64 val)
{
}
unsigned long arm_smmu_debug_get_tnx_tcr_cntl(void __iomem *tbu_base)
{
}
unsigned long arm_smmu_debug_get_tnx_tcr_cntl_2(void __iomem *tbu_base)
{
}
void arm_smmu_debug_set_mask_and_match(void __iomem *tbu_base, u64 sel,
					u64 mask, u64 match)
{
}
void arm_smmu_debug_get_mask_and_match(void __iomem *tbu_base,
					u64 *mask, u64 *match)
{
}
void arm_smmu_debug_get_capture_snapshot(void __iomem *tbu_base,
		u64 snapshot[NO_OF_CAPTURE_POINTS][REGS_PER_CAPTURE_POINT])
{
}
void arm_smmu_debug_clear_intr_and_validbits(void __iomem *tbu_base)
{
}
#endif
