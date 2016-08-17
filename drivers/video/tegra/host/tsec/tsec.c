/*
 * drivers/video/tegra/host/tsec/tsec.c
 *
 * Tegra TSEC Module Support
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>         /* for kzalloc */
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <mach/clk.h>
#include <asm/byteorder.h>      /* for parsing ucode image wrt endianness */
#include <linux/delay.h>	/* for udelay */
#include <linux/scatterlist.h>
#include <linux/stop_machine.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include "dev.h"
#include "tsec.h"
#include "hw_tsec.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"
#include "nvhost_intr.h"
#include "t114/t114.h"

#define TSEC_IDLE_TIMEOUT_DEFAULT	10000	/* 10 milliseconds */
#define TSEC_IDLE_CHECK_PERIOD		10	/* 10 usec */
#define TSEC_KEY_LENGTH			16
#define TSEC_RESERVE			256
#define TSEC_KEY_OFFSET			(TSEC_RESERVE - TSEC_KEY_LENGTH)
#define TSEC_HOST1X_STATUS_OFFSET	(TSEC_KEY_OFFSET - 4)

#define TSEC_OS_START_OFFSET    256

#define get_tsec(ndev) ((struct tsec *)(ndev)->dev.platform_data)
#define set_tsec(ndev, f) ((ndev)->dev.platform_data = f)

/* The key value in ascii hex */
static u8 otf_key[TSEC_KEY_LENGTH];

/* Pointer to this device */
struct platform_device *tsec;

static u32 *host1x_status_offset(struct tsec *m)
{
	return (u32 *)
		&(m->mapped[m->os.reserved_offset + TSEC_HOST1X_STATUS_OFFSET]);
}

static u32 tsec_get_host1x_state(void)
{
	struct tsec *m = get_tsec(tsec);
	u32 ret;

	/* We have dedicated memory byte  */
	ret = *host1x_status_offset(m);
	rmb();
	return ret;
}

static void tsec_set_host1x_state(int state)
{
	struct tsec *m = get_tsec(tsec);

	*host1x_status_offset(m) = state;
	wmb();
}

static int stop_machine_fn(void *priv)
{
	int timeout = 10000;

	tsec_set_host1x_state(tsec_host1x_access_granted);

	while (tsec_get_host1x_state() != tsec_host1x_release_access
			&& timeout)
		timeout--;

	if (!timeout)
		pr_err("TSEC didn't release access");

	tsec_set_host1x_state(tsec_host1x_none);

	return 0;
}

static void disable_tsec_irq(struct platform_device *pdev)
{
	nvhost_intr_disable_general_irq(&nvhost_get_host(pdev)->intr, 20);
}

static void enable_tsec_irq(struct platform_device *pdev)
{
	/* Clear interrupt */
	nvhost_device_writel(pdev, tsec_irqsclr_r(), 0xffffff);
	nvhost_device_writel(pdev, tsec_thi_int_status_r(), 0x1);
	nvhost_intr_enable_general_irq(&nvhost_get_host(pdev)->intr, 20,
			nvhost_tsec_isr, nvhost_tsec_isr_thread);
}

void nvhost_tsec_isr(void)
{
	disable_tsec_irq(tsec);
}

void nvhost_tsec_isr_thread(void)
{
	if (tsec_get_host1x_state() == tsec_host1x_request_access)
		stop_machine(stop_machine_fn, NULL, NULL);
	enable_tsec_irq(tsec);
}

/* caller is responsible for freeing */
static char *tsec_get_fw_name(struct platform_device *dev)
{
	char *fw_name;
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* note size here is a little over...*/
	fw_name = kzalloc(32, GFP_KERNEL);
	if (!fw_name)
		return NULL;

	decode_tsec_ver(pdata->version, &maj, &min);
	if (maj == 1) {
		/* there are no minor versions so far for maj==1 */
		sprintf(fw_name, "nvhost_tsec.fw");
	} else {
		kfree(fw_name);
		return NULL;
	}

	dev_info(&dev->dev, "fw name:%s\n", fw_name);

	return fw_name;
}

static int tsec_dma_wait_idle(struct platform_device *dev, u32 *timeout)
{
	if (!*timeout)
		*timeout = TSEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, TSEC_IDLE_CHECK_PERIOD, *timeout);
		u32 dmatrfcmd = nvhost_device_readl(dev, tsec_dmatrfcmd_r());
		u32 idle_v = tsec_dmatrfcmd_idle_v(dmatrfcmd);

		if (tsec_dmatrfcmd_idle_true_v() == idle_v)
			return 0;

		udelay(TSEC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	dev_err(&dev->dev, "dma idle timeout");

	return -1;
}

static int tsec_dma_pa_to_internal_256b(struct platform_device *dev,
		u32 offset, u32 internal_offset, bool imem)
{
	u32 cmd = tsec_dmatrfcmd_size_256b_f();
	u32 pa_offset =  tsec_dmatrffboffs_offs_f(offset);
	u32 i_offset = tsec_dmatrfmoffs_offs_f(internal_offset);
	u32 timeout = 0; /* default*/

	if (imem)
		cmd |= tsec_dmatrfcmd_imem_true_f();

	nvhost_device_writel(dev, tsec_dmatrfmoffs_r(), i_offset);
	nvhost_device_writel(dev, tsec_dmatrffboffs_r(), pa_offset);
	nvhost_device_writel(dev, tsec_dmatrfcmd_r(), cmd);

	return tsec_dma_wait_idle(dev, &timeout);

}

static int tsec_wait_idle(struct platform_device *dev, u32 *timeout)
{
	if (!*timeout)
		*timeout = TSEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, TSEC_IDLE_CHECK_PERIOD, *timeout);
		u32 w = nvhost_device_readl(dev, tsec_idlestate_r());

		if (!w)
			return 0;
		udelay(TSEC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	return -1;
}

static int tsec_load_kfuse(struct platform_device *pdev)
{
	u32 val;
	u32 timeout;

	val = nvhost_device_readl(pdev, tsec_tegra_ctl_r());
	val &= ~tsec_tegra_ctl_tkfi_kfuse_m();
	nvhost_device_writel(pdev, tsec_tegra_ctl_r(), val);

	val = nvhost_device_readl(pdev, tsec_scp_ctl_pkey_r());
	val |= tsec_scp_ctl_pkey_request_reload_s();
	nvhost_device_writel(pdev, tsec_scp_ctl_pkey_r(), val);

	timeout = TSEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, TSEC_IDLE_CHECK_PERIOD, timeout);
		u32 w = nvhost_device_readl(pdev, tsec_scp_ctl_pkey_r());

		if (w & tsec_scp_ctl_pkey_loaded_m())
			break;
		udelay(TSEC_IDLE_CHECK_PERIOD);
		timeout -= check;
	} while (timeout);

	val = nvhost_device_readl(pdev, tsec_tegra_ctl_r());
	val |= tsec_tegra_ctl_tkfi_kfuse_m();
	nvhost_device_writel(pdev, tsec_tegra_ctl_r(), val);

	if (timeout)
		return 0;
	else
		return -1;
}

int tsec_boot(struct platform_device *dev)
{
	u32 timeout;
	u32 offset;
	int err = 0;
	struct tsec *m = get_tsec(dev);

	nvhost_device_writel(dev, tsec_dmactl_r(), 0);
	nvhost_device_writel(dev, tsec_dmatrfbase_r(),
		(sg_dma_address(m->pa->sgl) + m->os.bin_data_offset) >> 8);

	for (offset = 0; offset < m->os.data_size; offset += 256)
		tsec_dma_pa_to_internal_256b(dev,
					   m->os.data_offset + offset,
					   offset, false);

	tsec_dma_pa_to_internal_256b(dev,
				     m->os.code_offset+TSEC_OS_START_OFFSET,
				     TSEC_OS_START_OFFSET, true);


	/* boot tsec */
	nvhost_device_writel(dev, tsec_bootvec_r(),
			     tsec_bootvec_vec_f(TSEC_OS_START_OFFSET));
	nvhost_device_writel(dev, tsec_cpuctl_r(),
			tsec_cpuctl_startcpu_true_f());

	timeout = 0; /* default */

	err = tsec_wait_idle(dev, &timeout);
	if (err != 0) {
		dev_err(&dev->dev, "boot failed due to timeout");
		return err;
	}

	/* setup tsec interrupts and enable interface */
	nvhost_device_writel(dev, tsec_irqmset_r(),
			(tsec_irqmset_ext_f(0xff) |
				tsec_irqmset_swgen1_set_f() |
				tsec_irqmset_swgen0_set_f() |
				tsec_irqmset_exterr_set_f() |
				tsec_irqmset_halt_set_f()   |
				tsec_irqmset_wdtmr_set_f()));

	nvhost_device_writel(dev, tsec_itfen_r(),
			(tsec_itfen_mthden_enable_f() |
				tsec_itfen_ctxen_enable_f()));

	return tsec_load_kfuse(dev);
}

static int tsec_setup_ucode_image(struct platform_device *dev,
		u32 *ucode_ptr,
		const struct firmware *ucode_fw)
{
	struct tsec *m = get_tsec(dev);
	/* image data is little endian. */
	struct tsec_ucode_v1 ucode;
	int w;
	u32 reserved_offset;
	u32 tsec_key_offset;

	/* copy the whole thing taking into account endianness */
	for (w = 0; w < ucode_fw->size / sizeof(u32); w++)
		ucode_ptr[w] = le32_to_cpu(((u32 *)ucode_fw->data)[w]);

	ucode.bin_header = (struct tsec_ucode_bin_header_v1 *)ucode_ptr;
	/* endian problems would show up right here */
	if (ucode.bin_header->bin_magic != 0x10de) {
		dev_err(&dev->dev,
			   "failed to get firmware magic");
		return -EINVAL;
	}
	if (ucode.bin_header->bin_ver != 1) {
		dev_err(&dev->dev,
			   "unsupported firmware version");
		return -ENOENT;
	}
	/* shouldn't be bigger than what firmware thinks */
	if (ucode.bin_header->bin_size > ucode_fw->size) {
		dev_err(&dev->dev,
			   "ucode image size inconsistency");
		return -EINVAL;
	}

	dev_dbg(&dev->dev,
		"ucode bin header: magic:0x%x ver:%d size:%d\n",
		ucode.bin_header->bin_magic,
		ucode.bin_header->bin_ver,
		ucode.bin_header->bin_size);
	dev_dbg(&dev->dev,
		"ucode bin header: os bin (header,data) offset size: 0x%x, 0x%x %d\n",
		ucode.bin_header->os_bin_header_offset,
		ucode.bin_header->os_bin_data_offset,
		ucode.bin_header->os_bin_size);
	ucode.os_header = (struct tsec_ucode_os_header_v1 *)
		(((void *)ucode_ptr) + ucode.bin_header->os_bin_header_offset);

	dev_dbg(&dev->dev,
		"os ucode header: os code (offset,size): 0x%x, 0x%x\n",
		ucode.os_header->os_code_offset,
		ucode.os_header->os_code_size);
	dev_dbg(&dev->dev,
		"os ucode header: os data (offset,size): 0x%x, 0x%x\n",
		ucode.os_header->os_data_offset,
		ucode.os_header->os_data_size);
	dev_dbg(&dev->dev,
		"os ucode header: num apps: %d\n",
		ucode.os_header->num_apps);

	/* make space for reserved area - we need 20 bytes, but we move 256
	 * bytes because firmware needs to be 256 byte aligned */
	reserved_offset = ucode.bin_header->os_bin_data_offset;
	memmove(((void *)ucode_ptr) + reserved_offset + TSEC_RESERVE,
			((void *)ucode_ptr) + reserved_offset,
			ucode.bin_header->os_bin_size);
	ucode.bin_header->os_bin_data_offset += TSEC_RESERVE;

	/*  clear 256 bytes before ucode os code */
	memset(((void *)ucode_ptr) + reserved_offset, 0, TSEC_RESERVE);

	/* Copy key to be the 16 bytes before the firmware */
	tsec_key_offset = reserved_offset + TSEC_KEY_OFFSET;
	memcpy(((void *)ucode_ptr) + tsec_key_offset, otf_key, TSEC_KEY_LENGTH);

	m->os.size = ucode.bin_header->os_bin_size;
	m->os.reserved_offset = reserved_offset;
	m->os.bin_data_offset = ucode.bin_header->os_bin_data_offset;
	m->os.code_offset = ucode.os_header->os_code_offset;
	m->os.data_offset = ucode.os_header->os_data_offset;
	m->os.data_size   = ucode.os_header->os_data_size;

	return 0;
}

int tsec_read_ucode(struct platform_device *dev, const char *fw_name)
{
	struct tsec *m = get_tsec(dev);
	const struct firmware *ucode_fw;
	int err;

	ucode_fw = nvhost_client_request_firmware(dev, fw_name);
	if (IS_ERR_OR_NULL(ucode_fw)) {
		dev_err(&dev->dev, "failed to get tsec firmware\n");
		err = -ENOENT;
		return err;
	}

	/* allocate pages for ucode */
	m->mem_r = mem_op().alloc(nvhost_get_host(dev)->memmgr,
			     roundup(ucode_fw->size+256, PAGE_SIZE),
			     PAGE_SIZE, mem_mgr_flag_uncacheable);
	if (IS_ERR_OR_NULL(m->mem_r)) {
		dev_err(&dev->dev, "nvmap alloc failed");
		err = -ENOMEM;
		goto clean_up;
	}

	m->pa = mem_op().pin(nvhost_get_host(dev)->memmgr, m->mem_r);
	if (IS_ERR_OR_NULL(m->pa)) {
		dev_err(&dev->dev, "nvmap pin failed for ucode");
		err = PTR_ERR(m->pa);
		m->pa = 0;
		goto clean_up;
	}

	m->mapped = mem_op().mmap(m->mem_r);
	if (IS_ERR_OR_NULL(m->mapped)) {
		dev_err(&dev->dev, "nvmap mmap failed");
		err = -ENOMEM;
		goto clean_up;
	}

	err = tsec_setup_ucode_image(dev, (u32 *)m->mapped, ucode_fw);
	if (err) {
		dev_err(&dev->dev, "failed to parse firmware image\n");
		return err;
	}

	m->valid = true;

	release_firmware(ucode_fw);

	return 0;

clean_up:
	if (m->mapped) {
		mem_op().munmap(m->mem_r, m->mapped);
		m->mapped = NULL;
	}
	if (m->pa) {
		mem_op().unpin(nvhost_get_host(dev)->memmgr, m->mem_r, m->pa);
		m->pa = NULL;
	}
	if (m->mem_r) {
		mem_op().put(nvhost_get_host(dev)->memmgr, m->mem_r);
		m->mem_r = NULL;
	}
	release_firmware(ucode_fw);
	return err;
}

void nvhost_tsec_init(struct platform_device *dev)
{
	int err = 0;
	struct tsec *m;
	char *fw_name;

	fw_name = tsec_get_fw_name(dev);
	if (!fw_name) {
		dev_err(&dev->dev, "couldn't determine firmware name");
		return;
	}

	m = kzalloc(sizeof(struct tsec), GFP_KERNEL);
	if (!m) {
		dev_err(&dev->dev, "couldn't alloc ucode");
		kfree(fw_name);
		return;
	}
	set_tsec(dev, m);

	err = tsec_read_ucode(dev, fw_name);
	kfree(fw_name);
	fw_name = 0;

	if (err || !m->valid) {
		dev_err(&dev->dev, "ucode not valid");
		goto clean_up;
	}

	nvhost_module_busy(dev);

	tsec_boot(dev);
	enable_tsec_irq(dev);
	nvhost_module_idle(dev);
	return;

clean_up:
	dev_err(&dev->dev, "failed");
}

void nvhost_tsec_deinit(struct platform_device *dev)
{
	struct tsec *m = get_tsec(dev);

	disable_tsec_irq(dev);

	/* unpin, free ucode memory */
	if (m->mem_r) {
		if (m->mapped)
			mem_op().munmap(m->mem_r, m->mapped);
		if (m->pa)
			mem_op().unpin(nvhost_get_host(dev)->memmgr, m->mem_r,
				m->pa);
		if (m->mem_r)
			mem_op().put(nvhost_get_host(dev)->memmgr, m->mem_r);
		m->mem_r = 0;
	}
}

void nvhost_tsec_finalize_poweron(struct platform_device *dev)
{
	tsec_boot(dev);
}

static struct of_device_id tegra_tsec_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra114-tsec",
		.data = (struct nvhost_device_data *)&t11_tsec_info },
	{ },
};
static int __devinit tsec_probe(struct platform_device *dev)
{
	int err;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_tsec_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	pdata->pdev = dev;
	pdata->init = nvhost_tsec_init;
	pdata->deinit = nvhost_tsec_deinit;

	platform_set_drvdata(dev, pdata);

	err = nvhost_client_device_get_resources(dev);
	if (err)
		return err;

	tsec = dev;

	err = nvhost_client_device_init(dev);
	if (err)
		return err;

	nvhost_module_busy(to_platform_device(dev->dev.parent));

	/* Reset TSEC at boot-up. Otherwise it starts sending interrupts. */
	clk_enable(pdata->clk[0]);
	tegra_periph_reset_assert(pdata->clk[0]);
	udelay(10);
	tegra_periph_reset_deassert(pdata->clk[0]);
	clk_disable(pdata->clk[0]);

	pm_runtime_use_autosuspend(&dev->dev);
	pm_runtime_set_autosuspend_delay(&dev->dev, 100);
	pm_runtime_enable(&dev->dev);

	nvhost_module_idle(to_platform_device(dev->dev.parent));
	return err;
}

static int __exit tsec_remove(struct platform_device *dev)
{
	struct nvhost_master *host = nvhost_get_host(dev);

	/* Add clean-up */
	host->intr.generic_isr[20] = NULL;
	host->intr.generic_isr_thread[20] = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int tsec_suspend(struct platform_device *dev, pm_message_t state)
{
	return nvhost_client_device_suspend(dev);
}

static int tsec_resume(struct platform_device *dev)
{
	dev_info(&dev->dev, "resuming\n");
	return 0;
}
#endif

static struct platform_driver tsec_driver = {
	.probe = tsec_probe,
	.remove = __exit_p(tsec_remove),
#ifdef CONFIG_PM
	.suspend = tsec_suspend,
	.resume = tsec_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "tsec",
#ifdef CONFIG_OF
		.of_match_table = tegra_tsec_of_match,
#endif
	}
};

static int __init tsec_key_setup(char *line)
{
	int i;
	u8 tmp[] = {0,0,0};
	pr_debug("tsec otf key: %s\n", line);

	if (strlen(line) != TSEC_KEY_LENGTH*2) {
		pr_warn("invalid tsec key: %s\n", line);
		return 0;
	}

	for (i = 0; i < TSEC_KEY_LENGTH; i++) {
		int err;
		memcpy(tmp, &line[i*2], 2);
		err = kstrtou8(tmp, 16, &otf_key[i]);
		if (err) {
			pr_warn("cannot read tsec otf key: %d", err);
			break;
		}
	}
	return 0;
}
__setup("otf_key=", tsec_key_setup);

static int __init tsec_init(void)
{
	return platform_driver_register(&tsec_driver);
}

static void __exit tsec_exit(void)
{
	platform_driver_unregister(&tsec_driver);
}

module_init(tsec_init);
module_exit(tsec_exit);
