#ifndef __ISPV4_BOOT_H
#define __ISPV4_BOOT_H

#include <linux/bits.h>
#include "ispv4_pcie.h"
// #include "ispv4_pcie_iatu.h"

/* PCIE Config */
#define ISPV4_PCIE_CFG_ADDR_OFFSET 0x0CC00000 /* 8MB */
#define PCIE_CTRL_DBI_REG_OFF 0x100000
#define IATU_APB2DBI_OFF 0x60000
#define CDM_SHADOW_OFF 0x40000
#define PCIE_CTRL_BASE (ISPV4_PCIE_CFG_ADDR_OFFSET + PCIE_CTRL_DBI_REG_OFF)
#define PF0_ATU_CAP_OFF (PCIE_CTRL_BASE + IATU_APB2DBI_OFF)
#define PF0_TYPE0_HDR_OFF (PCIE_CTRL_BASE + 0x0)
#define PF0_TYPE0_HDR_DBI2 (PCIE_CTRL_BASE + CDM_SHADOW_OFF)
#define PF0_PORT_LOGIC_OFF (PCIE_CTRL_BASE + 0x700)
#define PF0_PCIE_CAP_OFF (PCIE_CTRL_BASE + 0x70)
#define LINK_CONTROL2_LINK_STATUS2_REG (PF0_PCIE_CAP_OFF + 0x30)
#define ID_DIR_OFF(id, dir) ((id * 0x200) + (dir * 0x100))
#define IATU_ID_IDR_BASE(id, dir) (PF0_ATU_CAP_OFF + ID_DIR_OFF(id, dir))
#define IATU_LWR_TARGET_ADDR(id, dir) (IATU_ID_IDR_BASE(id, dir) + 0x14)
#define IATU_UPPER_TARGET_ADDR(id, dir) (IATU_ID_IDR_BASE(id, dir) + 0x18)
#define IATU_REGION_CTRL_1(id, dir) (IATU_ID_IDR_BASE(id, dir) + 0x0)
#define IATU_REGION_CTRL_2(id, dir) (IATU_ID_IDR_BASE(id, dir) + 0x4)
#define MISC_CONTROL_1 (PF0_PORT_LOGIC_OFF + 0x1bc)
#define IATU_REGION_EN BIT(31)
#define IATU_BAR_MARCH_MODE BIT(30)
#define IATU_BAR_NUM_SHIFT 8
#define IATU_BAR_NUM_MASK (0x7 << IATU_BAR_NUM_SHIFT)
#define BAR_REG(idx) (PF0_TYPE0_HDR_OFF + 0x10 + (idx * 4))
#define BAR_MASK_REG(idx) (PF0_TYPE0_HDR_DBI2 + 0x10 + (idx * 4))

#define TARGET_LINK_SPEED_SHIFT 0
#define TARGET_LINK_SPEED_WIDTH 4
#define TARGET_LINK_SPEED_MASK (0xF << TARGET_LINK_SPEED_SHIFT)

#define PCI_BAR_FLAG_MEM 0x00
#define PCI_BAR_MEM_TYPE_MASK 0x06
#define PCI_BAR_MEM_TYPE_32 0x00 /* 32 bit address */
#define PCI_BAR_MEM_TYPE_1M 0x02 /* Below 1M [obsolete] */
#define PCI_BAR_FLAG_MEM64 0x04 /* 64 bit address */
#define PCI_BAR_FLAG_PREFETCH 0x08 /* prefetchable? */

#define DBI_RO_WR_EN (1 << 0)


#define BOOT_RP_BY_PCI BIT(0)
#define BOOT_MB_BY_PCI BIT(1)
#define BOOT_RP_BY_SPI BIT(2)
#define BOOT_MB_BY_SPI BIT(3)

#define PCIE_CTRL_REG                        0x0CC001A8
#define ITSSM_SW_CTRL_EN_MASK                1
#define PCIE_PHY_REG                         0x0CE00000
#define PHY_MISC_CFG_MASK                    1
#define TOP_CLK_CORE                         0x0D460024
#define SLV_TOP_CLK_CORE_PHY_PCIE_CON_MASK   1
#define PCIE_PHY_REG0                        0x0CE000F0
#define PCIE_PHY_REG1                        0x0CE000F4
#define PHY_RESET_OVERD_EN_MASK              ((1<<3)|(1<<4))
#define PHY_RESET_OVERD_EN_VAL               ((1<<3)|(1<<4))
#define PCIE_PHY_REG2                        0x0CE0001C
#define PHY_MISC_RPT_MASK                    (1<<23)
#define PHY_MISC_RPT_VAL                     (1<<23)
#define PCIE_PHY_SRAM                        (0x0CF00000+0xC000*4)
#define PCIE_PHY_SRAM_SIZE                   (0x10000-0xC000)
#define PCIE_PHY_REG3                        0x0CE00024
#define PHY_MISC_CFG2_MASK                   (1<<26)
#define PCIE_CTRL_REG1                       0x0CC000B0
#define PCIE_PHY_STATUS_MASK                 (1<<0)
#define PCIE_PHY_STATUS_VAL                  (1<<0)
#define PCIE_CTRL_REG2                       0x0CC00018
#define SSI_GENERGEL_CORE_CTRL_MASK          1

#define L1_ENTRY_LAGENCY GENMASK(29, 27)

/* BAR5 SET PCIE IATU REG */
#define PCIE_IATU_ID_IDR_BASE(id, dir)	     ((id * 0x200) + (dir * 0x100))
#define PCIE_IATU_REGION_CTRL_1(id, dir)     (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x0)
#define PCIE_IATU_REGION_CTRL_2(id, dir)     (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x4)
#define PCIE_IATU_LWR_BASE_ADDR(id, dir)     (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x8)
#define PCIE_IATU_UPPER_BASE_ADDR(id, dir)   (PCIE_IATU_ID_IDR_BASE(id, dir) + 0xc)
#define PCIE_IATU_LIMIT_ADDR(id, dir)	     (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x10)
#define PCIE_IATU_LWR_TARGET_ADDR(id, dir)   (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x14)
#define PCIE_IATU_UPPER_TARGET_ADDR(id, dir) (PCIE_IATU_ID_IDR_BASE(id, dir) + 0x18)

int ispv4_boot_init(void);
void ispv4_boot_exit(void);

int ispv4_pci_init(int);
void ispv4_pci_exit(void);
extern int ispv4_boot_flag;

int ispv4_resume_config_pci(void);
int ispv4_boot_config_pci(void);
void _pci_linkdown_event(void);
int _pci_config_iatu_fast(struct ispv4_data *priv);
void _pci_reset(void);
#endif
