// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "logbuf vendorhook: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <soc/qcom/minidump.h>

#include <trace/hooks/logbuf.h>
#include "../../../kernel/printk/printk_ringbuffer.h"

#define BOOT_LOG_SIZE    SZ_512K
char *boot_log_buf;
unsigned int boot_log_buf_size;
bool copy_early_boot_log = true;

static size_t print_time(u64 ts, char *buf, size_t buf_sz)
{
	unsigned long rem_nsec = do_div(ts, 1000000000);

	return scnprintf(buf, buf_sz, "[%5lu.%06lu]",
				(unsigned long)ts, rem_nsec / 1000);
}

#ifdef CONFIG_PRINTK_CALLER
static size_t print_caller(u32 id, char *buf, size_t buf_sz)
{
	char caller[12];

	snprintf(caller, sizeof(caller), "%c%u",
		id & 0x80000000 ? 'C' : 'T', id & ~0x80000000);
	return scnprintf(buf, buf_sz, "[%6s]", caller);
}
#else
#define print_caller(id, buf) 0
#endif

static size_t info_print_prefix(const struct printk_info *info, char *buf,
				size_t buf_sz)
{
	size_t len = 0;

	len = print_time(info->ts_nsec, buf + len, buf_sz);
	len += print_caller(info->caller_id, buf + len, buf_sz - len);
	buf[len++] = ' ';
	buf[len] = '\0';
	return len;
}

#ifdef CONFIG_PRINTK_CALLER
#define PREFIX_MAX              48
#else
#define PREFIX_MAX              32
#endif

static size_t record_print_text(struct printk_info *pinfo, char *text,
					size_t buf_size)
{
	size_t text_len = pinfo->text_len;
	char prefix[PREFIX_MAX];
	bool truncated = false;
	size_t prefix_len;
	size_t line_len;
	size_t len = 0;
	char *next;

	/*
	 * If the message was truncated because the buffer was not large
	 * enough, treat the available text as if it were the full text.
	 */
	if (text_len > buf_size)
		text_len = buf_size;

	prefix_len = info_print_prefix(pinfo, prefix, buf_size);

	/*
	 * @text_len: bytes of unprocessed text
	 * @line_len: bytes of current line _without_ newline
	 * @text:     pointer to beginning of current line
	 * @len:      number of bytes prepared in r->text_buf
	 */
	for (;;) {
		next = memchr(text, '\n', text_len);
		if (next) {
			line_len = next - text;
		} else {
			/* Drop truncated line(s). */
			if (truncated)
				break;
			line_len = text_len;
		}

		/*
		 * Truncate the text if there is not enough space to add the
		 * prefix and a trailing newline and a terminator.
		 */
		if (len + prefix_len + text_len + 1 + 1 > buf_size) {
			if (len + prefix_len + line_len + 1 + 1 > buf_size)
				break;

			text_len = buf_size - len - prefix_len - 1 - 1;
			truncated = true;
		}

		memmove(text + prefix_len, text, text_len);
		memcpy(text, prefix, prefix_len);

		/*
		 * Increment the prepared length to include the text and
		 * prefix that were just moved+copied. Also increment for the
		 * newline at the end of this line. If this is the last line,
		 * there is no newline, but it will be added immediately below.
		 */
		len += prefix_len + line_len + 1;
		if (text_len == line_len) {
			 /*
			  * This is the last line. Add the trailing newline
			  * removed in vprintk_store().
			  */
			text[prefix_len + line_len] = '\n';
			break;
		}

		/*
		 * Advance beyond the added prefix and the related line with
		 * its newline.
		 */

		text += prefix_len + line_len + 1;

		/*
		 * The remaining text has only decreased by the line with its
		 * newline.
		 *
		 * Note that @text_len can become zero. It happens when @text
		 * ended with a newline (either due to truncation or the
		 * original string ending with "\n\n"). The loop is correctly
		 * repeated and (if not truncated) an empty line with a prefix
		 * will be prepared.
		 */

		text_len -= line_len + 1;
	}

	/*
	 * If a buffer was provided, it will be terminated. Space for the
	 * string terminator is guaranteed to be available. The terminator is
	 * not counted in the return value.
	 */

	if (buf_size > 0)
		text[len] = 0;

	return len;
}

static void copy_boot_log(void *unused, struct printk_ringbuffer *prb,
					struct printk_record *r)
{
	struct prb_desc_ring descring = prb->desc_ring;
	struct prb_data_ring textdata_ring = prb->text_data_ring;
	struct prb_desc *descaddr = descring.descs;
	struct printk_info *p_infos = descring.infos;
	atomic_long_t headid, tailid;
	unsigned long did, ind, sv;
	unsigned int textdata_size = _DATA_SIZE(textdata_ring.size_bits);
	unsigned long begin, end;
	static unsigned int off;
	enum desc_state state;
	size_t rem_buf_sz;

	tailid = descring.tail_id;
	headid = descring.head_id;

	if (!copy_early_boot_log) {
		if (!r->info->text_len)
			return;

		if ((off + r->info->text_len) > boot_log_buf_size)
			return;

		rem_buf_sz = boot_log_buf_size - off;
		if (!rem_buf_sz || rem_buf_sz < sizeof(struct printk_record))
			return;

		memcpy(&boot_log_buf[off], &r->text_buf[0], r->info->text_len);
		off += record_print_text(r->info, &boot_log_buf[off],
			rem_buf_sz);
		return;
	}

	copy_early_boot_log = false;
	did = atomic_long_read(&tailid);
	while (true) {
		ind = did % _DESCS_COUNT(descring.count_bits);
		sv = atomic_long_read(&descaddr[ind].state_var);
		state = DESC_STATE(sv);
		/* skip non-committed record */
		if ((state != desc_committed) && (state != desc_finalized)) {
			if (did == atomic_long_read(&headid))
				break;

			did = DESC_ID(did + 1);
			continue;
		}

		begin = (descaddr[ind].text_blk_lpos.begin) % textdata_size;
		end = (descaddr[ind].text_blk_lpos.next) % textdata_size;
		if ((begin & 1) != 1) {
			unsigned long text_start;
			u16 textlen;

			if (begin > end)
				begin = 0;

			text_start = begin + sizeof(unsigned long);
			textlen = p_infos[ind].text_len;
			if (end - text_start < textlen)
				textlen = end - text_start;

			if ((off + textlen) > boot_log_buf_size)
				break;

			rem_buf_sz = boot_log_buf_size - off;

			memcpy(&boot_log_buf[off],
				&textdata_ring.data[text_start],
				textlen);
			off += record_print_text(&p_infos[ind],
						&boot_log_buf[off], rem_buf_sz);
			if (off > boot_log_buf_size)
				break;
		}

		if (did == atomic_long_read(&headid))
			break;

		did = DESC_ID(did + 1);
	}
}

static int boot_log_init(void)
{
	void *start;
	int ret = 0;
	unsigned int size;
	struct md_region md_entry;
	uint32_t log_buf_len;

	log_buf_len = log_buf_len_get();
	if (!log_buf_len) {
		pr_err("log_buf_len is zero\n");
		ret = -EINVAL;
		goto out;
	}

	if (log_buf_len >= BOOT_LOG_SIZE)
		size = log_buf_len;
	else
		size = BOOT_LOG_SIZE;

	start = kzalloc(size, GFP_KERNEL);
	if (!start) {
		ret = -ENOMEM;
		goto out;
	}

	strlcpy(md_entry.name, "KBOOT_LOG", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t)start;
	md_entry.phys_addr = virt_to_phys(start);
	md_entry.size = size;
	ret = msm_minidump_add_region(&md_entry);
	if (ret < 0) {
		pr_err("Failed to add boot_log entry in minidump table\n");
		kfree(start);
		goto out;
	}

	boot_log_buf_size = size;
	boot_log_buf = (char *)start;

	/* Ensure boot_log_buf and boot_log_buf initialization
	 * is visible to other CPU's
	 */
	smp_mb();

out:
	return ret;
}

static void release_boot_log_buf(void)
{
	if (!boot_log_buf)
		return;

	kfree(boot_log_buf);
}

static int logbuf_vendor_hooks_driver_probe(struct platform_device *pdev)
{
	int ret;

	ret = boot_log_init();
	if (ret < 0)
		return ret;

	ret = register_trace_android_vh_logbuf(copy_boot_log, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register android_vh_logbuf hook\n");
		kfree(boot_log_buf);
	}

	return ret;
}

static int logbuf_vendor_hooks_driver_remove(struct platform_device *pdev)
{
	unregister_trace_android_vh_logbuf(copy_boot_log, NULL);
	release_boot_log_buf();
	return 0;
}

static const struct of_device_id logbuf_vendor_hooks_of_match[] = {
	{ .compatible = "qcom,logbuf-vendor-hooks" },
	{ }
};
MODULE_DEVICE_TABLE(of, logbuf_vendor_hooks_of_match);

static struct platform_driver logbuf_vendor_hooks_driver = {
	.driver = {
		.name = "qcom-logbuf-vendor-hooks",
		.of_match_table = logbuf_vendor_hooks_of_match,
	},
	.probe = logbuf_vendor_hooks_driver_probe,
	.remove = logbuf_vendor_hooks_driver_remove,
};

static int __init qcom_logbuf_vendor_hook_driver_init(void)
{
	return platform_driver_register(&logbuf_vendor_hooks_driver);
}
#if IS_MODULE(CONFIG_QCOM_LOGBUF_VENDOR_HOOKS)
module_init(qcom_logbuf_vendor_hook_driver_init);
#else
pure_initcall(qcom_logbuf_vendor_hook_driver_init);
#endif

static void __exit qcom_logbuf_vendor_hook_driver_exit(void)
{
	return platform_driver_unregister(&logbuf_vendor_hooks_driver);
}
module_exit(qcom_logbuf_vendor_hook_driver_exit);

MODULE_DESCRIPTION("QCOM Logbuf Vendor Hooks Driver");
MODULE_LICENSE("GPL v2");
