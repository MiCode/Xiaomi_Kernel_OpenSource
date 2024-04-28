/*
 * Broadcom BCM8956X / BCM8957X / BCM8989X 10Gb Ethernet driver
 *
 * Copyright (c) 2022 Broadcom. The term "Broadcom" refers solely to the
 * Broadcom Inc. subsidiary that distributes the Licensed Product, as defined
 * below.
 *
 * The following copyright statements and licenses apply to open source software
 * ("OSS") distributed with the BCM8956X / BCM8957X / BCM8989X product (the "Licensed Product").
 * The Licensed Product does not necessarily use all the OSS referred to below and
 * may also only use portions of a given OSS component.
 *
 * To the extent required under an applicable open source license, Broadcom
 * will make source code available for applicable OSS upon request. Please send
 * an inquiry to opensource@broadcom.com including your name, address, the
 * product name and version, operating system, and the place of purchase.
 *
 * To the extent the Licensed Product includes OSS, the OSS is typically no
 * owned by Broadcom. THE OSS IS PROVIDED AS IS WITHOUT WARRANTY OR CONDITION
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * To the full extent permitted under applicable law, Broadcom disclaims all
 * warranties and liability arising from or related to any use of the OSS.
 *
 * To the extent the Licensed Product includes OSS licensed under the GNU
 * General Public License ("GPL") or the GNU Lesser General Public License
 * ("LGPL"), the use, copying, distribution and modification of the GPL OSS or
 * LGPL OSS is governed, respectively, by the GPL or LGPL.  A copy of the GPL
 * or LGPL license may be found with the applicable OSS.  Additionally, a copy
 * of the GPL License or LGPL License can be found a
 * https://www.gnu.org/licenses or obtained by writing to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2 License
 *
 * Copyright (c) 2021 Broadcom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * License 2: Modified BSD License
 *
 * Copyright (c) 2021 Broadcom
 *
 * Redistribution and use in source and binary forms, with or withou
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyrigh
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyrigh
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "xgbe.h"
#include "xgbe-common.h"

#if BRCM_BCMUTIL

#include "xgbe-bcmutil.h"

#define IOCTL_USE_MISC_DRIVER   1

#define A0_DTCM_IPC_START       (0x2003C000)
#define A0_DTCM_IPC_END         (0x2003FFFF)
#define A0_PCIE_DTCM_OFFSET     (0x80000)
#define B0_DTCM_IPC_START       (0x2007C000)
#define B0_DTCM_IPC_END         (0x2007FFFF)
#define B0_PCIE_DTCM_OFFSET     (0x00100000)

#define IND_PCIE_START          (0x4B280000)
#define IND_PCIE_END            (0x4B28000F)
#define IND_DEV_BASE            (IND_PCIE_START)
#define PCIE_IND_OFFSET         (0x003FFF00)

#define DTCM_TOP_BRIDGE_START   (0x4A800000)
#define DTCM_TOP_BRIDGE_END     (0x4AA00000)

#define PCI_BAR1_ITCM_BASE      (0x20000000)
#define PCI_BAR1_DTCM_DEV_BASE  (0x20000000)
#define PCI_BAR1_BASE           (0x20000000)

#ifdef DEBUG
#define DBG_LOG(x...)    printk(x)
#else
#define DBG_LOG(x...)
#endif

int  xgbe_bcmutil_pci_ioctl(struct xgbe_prv_data* pdata, unsigned int cmd, unsigned long ioctl_arg)
{
    int err = 0;
    struct xgbe_ioctlcmd *iocmd;
    struct xgbe_ioctlcmd io;
    struct pci_dev *pdev;
    uint32_t offset;
    uint32_t base;
    uint32_t val;
    int dev_mem = 0;
    uint32_t reg_read32 = 0;

    if ((XGBE_IOCTL_RDMEM != cmd) &&
        (XGBE_IOCTL_WRMEM != cmd) &&
        (XGBE_IOCTL_RDMEM_SOCK != cmd) &&
        (XGBE_IOCTL_WRMEM_SOCK != cmd)
    ) {
        err = EINVAL;
        goto err_exit;
    }
    pdev = pdata->pcidev;

    if(copy_from_user(&io, (struct xgbe_ioctlcmd *)ioctl_arg, sizeof(io))) {
        err = EINVAL;
        goto err_exit;
    }
    iocmd = &io;
    if(copy_from_user(&reg_read32, iocmd->data, sizeof(uint32_t))) {
        err = EINVAL;
        goto err_exit;
    }

    if(BCM8956X_A0_PF_ID == pdata->dev_id) {
        if ((iocmd->addr >= A0_DTCM_IPC_START) && (iocmd->addr < A0_DTCM_IPC_END)) {
            base = PCI_BAR1_ITCM_BASE;
            offset = (iocmd->addr - PCI_BAR1_DTCM_DEV_BASE) + A0_PCIE_DTCM_OFFSET;
        } else if ((iocmd->addr >= DTCM_TOP_BRIDGE_START) && (iocmd->addr < DTCM_TOP_BRIDGE_END)) {
            base = 0x22000000;
            offset = iocmd->addr - 0x4A000000;
            offset = (offset & 0xFFFF0000) + (offset & 0xFFFF) * 2;
            dev_mem = 1;
        } else {
            err = EINVAL;
            goto err_exit;
        }
        pci_write_config_dword(pdev, 0x84, base);
        err = pci_read_config_dword(pdev, 0x84, &base);
        if (err)
            goto err_exit;
        switch (cmd) {
            case XGBE_IOCTL_RDMEM:
            case XGBE_IOCTL_RDMEM_SOCK:
                switch (iocmd->width) {
                case 32:
                    reg_read32 = ioread32(pdata->xpcs_regs + offset);
                    break;
                case 16:
                    if (dev_mem) {
                        val = ioread32(pdata->xpcs_regs + offset);
                        if (iocmd->addr & 0x3) {
                            reg_read32 = (val >> 16) & 0xFFFF;
                        } else {
                            reg_read32 = val & 0xFFFF;
                        }
                    } else {
                        reg_read32 = ioread16(pdata->xpcs_regs + offset);
                    }
                    break;
                default:
                    err = EINVAL;
                    break;
                }
                break;
            case XGBE_IOCTL_WRMEM:
            case XGBE_IOCTL_WRMEM_SOCK:
                switch (iocmd->width) {
                case 32:
                    iowrite32(reg_read32, pdata->xpcs_regs + offset);
                    break;
                case 16:
                    if (dev_mem) {
                        if (iocmd->addr & 0x3) {
                            val = (reg_read32 & 0xFFFF) << 16;
                        } else {
                            val = reg_read32 & 0xFFFF;
                        }
                        iowrite32(val, pdata->xpcs_regs + offset);
                    } else {
                        iowrite16(reg_read32, pdata->xpcs_regs + offset);
                    }
                    break;
                default:
                    err = EINVAL;
                    break;
                }
                break;
            default:
                err = EINVAL;
                break;
        }
        if(copy_to_user(iocmd->data, &reg_read32, sizeof(uint32_t))) {
            err = EINVAL;
            goto err_exit;
        }
    } else if ((BCM8956X_PF_ID == pdata->dev_id) || (BCM8957X_PF_ID == pdata->dev_id)) {
        if (32 != iocmd->width) {
            err = EINVAL;
            DBG_LOG("Unsupported width : %d addr: 0x%x\n", iocmd->width, iocmd->addr);
            goto err_exit;
        }

        if ((iocmd->addr >= B0_DTCM_IPC_START) && (iocmd->addr < B0_DTCM_IPC_END)) {
            offset = (iocmd->addr - PCI_BAR1_DTCM_DEV_BASE) + B0_PCIE_DTCM_OFFSET;
        } else if ((iocmd->addr >= IND_PCIE_START) && (iocmd->addr < IND_PCIE_END)) {
            offset = (iocmd->addr - IND_DEV_BASE) + PCIE_IND_OFFSET;
        } else {
            err = EINVAL;
            DBG_LOG("Unsupported access\n");
            goto err_exit;
        }

        switch (cmd) {
        case XGBE_IOCTL_RDMEM:
        case XGBE_IOCTL_RDMEM_SOCK:
            reg_read32 = ioread32(pdata->xpcs_regs + offset);
            break;

        case XGBE_IOCTL_WRMEM:
        case XGBE_IOCTL_WRMEM_SOCK:
            iowrite32(reg_read32, pdata->xpcs_regs + offset);
            break;
        default:
            err = EINVAL;
            break;
        }
    }  else  {
        printk("Invalid device ID\n");
    }
    if(copy_to_user(iocmd->data, &reg_read32, sizeof(uint32_t))) {
        err = EINVAL;
        goto err_exit;
    }
    if(copy_to_user((struct xgbe_ioctlcmd *)ioctl_arg, &io, sizeof(io))) {
        err = EINVAL;
        goto err_exit;
    }
err_exit:
    return err;
}

#if IOCTL_USE_MISC_DRIVER
static int xgbe_bcmutil_pci_misc_fops_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = (struct miscdevice *)(file->private_data);
    printk("%s: Device Open Called for device %s \n", __func__, miscdev->nodename);
    return nonseekable_open(inode, file);
}

static int xgbe_bcmutil_pci_misc_fops_release(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = (struct miscdevice *)(file->private_data);
    printk("%s: Device Close Called for device %s \n", __func__, miscdev->nodename);
    return 0;
}


static long xgbe_bcmutil_pci_misc_fops_ioctl(struct file *file, unsigned int cmd, unsigned long ioctl_arg)
{
    struct xgbe_prv_data* pdata = container_of(file->private_data, struct xgbe_prv_data, miscdev);
    return (long)xgbe_bcmutil_pci_ioctl(pdata, cmd, ioctl_arg);
}

static const struct file_operations xgbe_bcmutil_pci_misc_file_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = xgbe_bcmutil_pci_misc_fops_ioctl,
    .open = xgbe_bcmutil_pci_misc_fops_open,
    .release = xgbe_bcmutil_pci_misc_fops_release,
    .llseek = noop_llseek,
};
#endif

void xgbe_bcmutil_fixup(struct xgbe_prv_data *pdata)
{
    u32 temp;
    if(pdata->dev_id == BCM8956X_A0_PF_ID) {
        pci_write_config_dword(pdata->pcidev, 0x74, 0x4000 );
        pci_write_config_dword(pdata->pcidev, 0x78, 0x5000 );
        pci_write_config_dword(pdata->pcidev, 0x94, 0x0400 );

        /* Write to the MII_CTL to forcefully link up */
        XGMAC_IOWRITE(pdata, 0x4, 0x7);
        XGMAC_IOWRITE(pdata, 0x8, 0xe00);

        pdata->xgmac_regs += 0x2000;
        //update SBtoPCIETranslationBigMem
        temp = XGMAC_IOREAD(pdata, 0x108);
        temp &= ~0x80000000;
        XGMAC_IOWRITE(pdata, 0x108, temp);
        temp = XGMAC_IOREAD(pdata, 0x108);
        printk( "SBtoPCIETranslationBigMem = %x\n", temp );
    } else {
        pci_write_config_dword(pdata->pcidev, 0x84, PCI_BAR1_BASE);
    }
}

int xgbe_bcmutil_misc_driver_register(struct xgbe_prv_data *pdata)
{

    static int dev_instance = 0;
    int ret = 0;
#if IOCTL_USE_MISC_DRIVER
    struct miscdevice *miscdev;
    char* node_name;
    char* dev_name;

    pdata->xpcs_regs = pdata->phy_regs - (pdata->bar2_size - XGMAC_PHY_REGS_OFFSET_FROM_END);
    miscdev = &pdata->miscdev;
    //Eiger No Need to Register MISC Device. Misc Device is only for Switch
    if(pdata->dev_id == BCM8989X_PF_ID) { return ret; }

    dev_name = kzalloc(32,  GFP_KERNEL);
    node_name = kzalloc(32,  GFP_KERNEL);
    // Enable below to test multiple SC-1/SC-2 cards using a soft link 
    // sudo ln -s /dev/net/bcm1 /dev/net/bcm will point /dev/net/bcm to bcm1
#if 0
    snprintf(dev_name, 32, "%s%c", XGBE_DRV_NAME, ('0' + dev_instance));
    snprintf(node_name, 32, "net/bcm%c", ('0' + dev_instance));
#else
    snprintf(dev_name, 32, "%s%c", XGBE_DRV_NAME, (dev_instance == 0) ? '\0' : ('0' + dev_instance));
    snprintf(node_name, 32, "net/bcm%c", (dev_instance == 0) ? '\0' : ('0' + dev_instance));
#endif
    miscdev->name = dev_name;
    miscdev->nodename = node_name;
    miscdev->minor = MISC_DYNAMIC_MINOR;
    miscdev->fops = &xgbe_bcmutil_pci_misc_file_ops;
    miscdev->mode = 0664;

    ret = misc_register(miscdev);
    if (ret) {
        printk("%s: Can't register misc device %s -- ret=%d\n", __func__, miscdev->nodename, ret);
        kfree(miscdev->name);
        kfree(miscdev->nodename);
        miscdev->nodename = NULL;
        miscdev->name = NULL;
    } else {
        dev_instance++;
    }
#endif
    return ret;
}

void xgbe_bcmutil_misc_driver_deregister(struct xgbe_prv_data* pdata)
{
#if IOCTL_USE_MISC_DRIVER
    struct miscdevice *miscdev;
    if(pdata->dev_id == BCM8989X_PF_ID) { return; }
    miscdev = &pdata->miscdev;
    kfree(miscdev->name);
    kfree(miscdev->nodename);
    misc_deregister(&pdata->miscdev);
#endif
}
#endif
