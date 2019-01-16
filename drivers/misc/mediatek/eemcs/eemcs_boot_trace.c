#include <linux/sched.h>
#include <linux/jiffies.h> 
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "eemcs_boot_trace.h"
#include "eemcs_kal.h"
#include "eemcs_debug.h"
#include "eemcs_state.h"
#include "eemcs_boot.h"

static EEMCS_BOOT_TRACE eemcs_trace_inst;
extern struct dentry *g_eemcs_dbg_dentry;

static void _eemcs_trace_out(struct sk_buff *skb, EEMCS_BOOT_TRACE_OUT_TYPE type, struct seq_file *s);

static int boot_trace_print(struct seq_file *s, void *data)
{
    struct sk_buff *first = NULL;
    struct sk_buff *next = NULL;

    if (!skb_queue_empty(&eemcs_trace_inst.log_skb_list)) {
        first = skb_peek(&eemcs_trace_inst.log_skb_list);
        if (first != NULL) {
            _eemcs_trace_out(first, OUT_TYPE_ALL, s);
            while (!skb_queue_is_last(&eemcs_trace_inst.log_skb_list, first)) {
                next = skb_queue_next(&eemcs_trace_inst.log_skb_list, first);
                _eemcs_trace_out(next, OUT_TYPE_ALL, s);
                first = next;
            }
        }
    }

    return 0;
}

static int boot_trace_open(struct inode *inode, struct file *file)
{
    return single_open(file, boot_trace_print, inode->i_private);
}

static const struct file_operations boot_trace_fops = {
    .open = boot_trace_open,
    .read = seq_read,
};

KAL_UINT32 eemcs_boot_trace_init(void)
{
#ifdef _EEMCS_TRACE_SUPPORT
    KAL_INT32 result = KAL_FAIL;
    struct dentry *p_e_dentry, *p_f_dentry;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (g_eemcs_dbg_dentry != NULL) {
        p_e_dentry = g_eemcs_dbg_dentry;
        do {
            p_f_dentry = debugfs_create_file("boot_trace", 0666, p_e_dentry, NULL, &boot_trace_fops);
            result = PTR_ERR(p_f_dentry);
            if (IS_ERR(p_f_dentry) && result != -ENODEV){
                printk(KERN_ERR "Failed to create debugfs \"boot_trace\" (%d)", result);
                goto _boot_trace_init_fail;
            }
        } while(0);
    } else {
        goto _boot_trace_init_fail;
    }

    skb_queue_head_init(&eemcs_trace_inst.log_skb_list);
    eemcs_trace_inst.inited = 1;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;

_boot_trace_init_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return -ENOENT;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT
}

KAL_UINT32 eemcs_boot_trace_deinit(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
#ifdef _EEMCS_TRACE_SUPPORT
    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;
    skb_queue_purge(&eemcs_trace_inst.log_skb_list);
#endif // _EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

unsigned char *eemcs_boot_trace_alloc(void)
{
    struct sk_buff *new_skb = NULL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    return skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
}

KAL_UINT32 eemcs_boot_trace_state(EEMCS_STATE state)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *new_skb = NULL;
    EEMCS_BOOT_TRACE_SET *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;
    if (state <= EEMCS_INVALID || state >= EEMCS_STATE_MAX)
        return KAL_FAIL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    header = (EEMCS_BOOT_TRACE_SET *)skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
    KAL_ASSERT(header != NULL);
    header->type = TRA_TYPE_EEMCS_STATE;
    header->trace.state = state;
    skb_queue_tail(&eemcs_trace_inst.log_skb_list, new_skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT
}

KAL_UINT32 eemcs_boot_trace_boot(EEMCS_BOOT_STATE state)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *new_skb = NULL;
    EEMCS_BOOT_TRACE_SET *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;
    if (state <= MD_INVALID || state >= END_OF_MD_STATE)
        return KAL_FAIL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    header = (EEMCS_BOOT_TRACE_SET *)skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
    KAL_ASSERT(header != NULL);
    header->type = TRA_TYPE_BOOT_STATE;
    header->trace.state = state;
    skb_queue_tail(&eemcs_trace_inst.log_skb_list, new_skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT

}

KAL_UINT32 eemcs_boot_trace_mbx(KAL_UINT32 *val, KAL_UINT32 size, EEMCS_BOOT_TRACE_TYPE type)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *new_skb = NULL;
    EEMCS_BOOT_TRACE_SET *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;
    if (val == NULL || (size != sizeof(KAL_UINT32) * 2))
        return KAL_FAIL;
    if (type != TRA_TYPE_MBX_RX && type != TRA_TYPE_MBX_TX)
        return KAL_FAIL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    header = (EEMCS_BOOT_TRACE_SET *)skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
    KAL_ASSERT(header != NULL);
    header->type = type;
    header->trace.mbx_data[0] = *val;
    header->trace.mbx_data[1] = *(val + 1);
    skb_queue_tail(&eemcs_trace_inst.log_skb_list, new_skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT
}

KAL_UINT32 eemcs_boot_trace_xcmd(XBOOT_CMD *xcmd, EEMCS_BOOT_TRACE_TYPE type)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *new_skb = NULL;
    EEMCS_BOOT_TRACE_SET *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;
    if (xcmd == NULL || (type != TRA_TYPE_XCMD_RX && type != TRA_TYPE_XCMD_TX))
        return KAL_FAIL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    header = (EEMCS_BOOT_TRACE_SET *)skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
    KAL_ASSERT(header != NULL);
    memset(header, 0, sizeof(EEMCS_BOOT_TRACE_SET));
    header->type = type;
    header->trace.xcmd.magic = xcmd->magic;
    header->trace.xcmd.msg_id = xcmd->msg_id;
    header->trace.xcmd.status = xcmd->status;
    skb_queue_tail(&eemcs_trace_inst.log_skb_list, new_skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT
}

KAL_UINT32 eemcs_boot_trace_xcmd_file(KAL_UINT32 offset, KAL_UINT32 len, KAL_UINT8 checksum)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *new_skb = NULL;
    EEMCS_BOOT_TRACE_SET *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (!eemcs_trace_inst.inited)
        return KAL_FAIL;

    new_skb = dev_alloc_skb(sizeof(EEMCS_BOOT_TRACE_SET));
    header = (EEMCS_BOOT_TRACE_SET *)skb_put(new_skb, sizeof(EEMCS_BOOT_TRACE_SET));
    KAL_ASSERT(header != NULL);
    memset(header, 0, sizeof(EEMCS_BOOT_TRACE_SET));
    header->type = TRA_TYPE_XCMD_TX;
    header->trace.xcmd_getbin.offset = offset;
    header->trace.xcmd_getbin.len = len;
    header->trace.xcmd_getbin.reserved[0] = checksum;
    skb_queue_tail(&eemcs_trace_inst.log_skb_list, new_skb);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else // !_EEMCS_TRACE_SUPPORT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_TRACE_SUPPORT
}

unsigned char *g_eemcs_sta_str[] = {
    "EEMCS_INVALID",
    "EEMCS_GATE",
    "EEMCS_INIT",
    "EEMCS_XBOOT",
    "EEMCS_MOLY_HS_P1",
    "EEMCS_MOLY_HS_P2",
    "EEMCS_BOOTING_DONE",
    "EEMCS_EXCEPTION",
    "EEMCS_STATE_MAX",
};

static void __eemcs_trace_out_eemcs_state(EEMCS_BOOT_TRACE_SET *tra_hdr, struct seq_file *s)
{
    if (s != NULL) {
        seq_printf(s, "\033[1;32m%-18s\033[0m (%s)\n", "EEMCS_STATE", g_eemcs_sta_str[tra_hdr->trace.state]);
    } else {
        printk("[EEMCS_BOOT_TRACE] EEMCS_STATE (%s)\n", g_eemcs_sta_str[tra_hdr->trace.state]);
    }
}

extern unsigned char* g_md_sta_str[];
static void __eemcs_trace_out_boot_state(EEMCS_BOOT_TRACE_SET *tra_hdr, struct seq_file *s)
{
    if (s != NULL) {
        seq_printf(s, "\033[1;34m%-18s\033[0m (%s)\n", "EEMCS_BOOT_STATE", g_md_sta_str[tra_hdr->trace.state]);
    } else {
        printk("[EEMCS_BOOT_TRACE] EEMCS_BOOT_STATE (%s)\n", g_md_sta_str[tra_hdr->trace.state]);
    }
}

static void __eemcs_trace_out_mbx(EEMCS_BOOT_TRACE_SET *tra_hdr, struct seq_file *s)
{
    if (s != NULL) {
        seq_printf(s, "\033[1;35m%-18s\033[0m %-15s (0x%X, 0x%X)\n",
            "EEMCS_MBX",
            (tra_hdr->type == TRA_TYPE_MBX_RX) ? "(AP <- MD)" : "(AP -> MD)",
            tra_hdr->trace.mbx_data[0],
            tra_hdr->trace.mbx_data[1]);
    } else {
        printk("[EEMCS_BOOT_TRACE] EEMCS_MBX (%s) (0x%X, 0x%X)\n",
            (tra_hdr->type == TRA_TYPE_MBX_RX) ? "AP <- MD" : "AP -> MD",
            tra_hdr->trace.mbx_data[0],
            tra_hdr->trace.mbx_data[1]);
    }
}

static void __eemcs_trace_out_xcmd(EEMCS_BOOT_TRACE_SET *tra_hdr, struct seq_file *s)
{
    XBOOT_CMD *xcmd = NULL;
    XBOOT_CMD_GETBIN *xcmd_getbin = NULL;

    xcmd = (XBOOT_CMD *)&(tra_hdr->trace.xcmd);
    if (xcmd->magic != (KAL_UINT32)MAGIC_MD_CMD &&
        xcmd->magic != (KAL_UINT32)MAGIC_MD_CMD_ACK) {
        xcmd_getbin = (XBOOT_CMD_GETBIN *)xcmd;
        if (s != NULL) {
            seq_printf(s, "%-18s %-15s (0x%05X, 0x%06X) CS-->0x%X\n",
                "EEMCS_XCMD_FILE",
                (tra_hdr->type == TRA_TYPE_XCMD_RX) ? "(AP <- MD)" : "(AP -> MD)",
                xcmd_getbin->offset,
                xcmd_getbin->len,
                xcmd_getbin->reserved[0]);

        } else {
            printk("[EEMCS_BOOT_TRACE] EEMCS_XCMD_FILE (%s) (0x%X)(0x%X) CS-->0x%X\n",
                (tra_hdr->type == TRA_TYPE_XCMD_RX) ? "AP <- MD" : "AP -> MD",
                xcmd_getbin->offset,
                xcmd_getbin->len,
                xcmd_getbin->reserved[0]);
        }
    } else {
        if (s != NULL) {
            seq_printf(s, "%-18s %-15s (0x%X)(0x%X)(0x%X)(0x%X)\n",
                "EEMCS_XCMD",
                (tra_hdr->type == TRA_TYPE_XCMD_RX) ? "(AP <- MD)" : "(AP -> MD)",
                xcmd->magic, xcmd->msg_id, xcmd->status, xcmd->reserved[0]);

        } else {                
            printk("[EEMCS_BOOT_TRACE] EEMCS_XCMD (%s) (0x%X)(0x%X)(0x%X)(0x%X)\n",
                (tra_hdr->type == TRA_TYPE_XCMD_RX) ? "AP <- MD" : "AP -> MD",
                xcmd->magic, xcmd->msg_id, xcmd->status, xcmd->reserved[0]);
        }
    }
}

static void __eemcs_trace_out(struct sk_buff *skb, struct seq_file *s)
{
    EEMCS_BOOT_TRACE_SET *tra_hdr = NULL;

    KAL_ASSERT(skb != NULL);
    tra_hdr = (EEMCS_BOOT_TRACE_SET *)skb->data;
    switch (tra_hdr->type)
    {
        case TRA_TYPE_EEMCS_STATE:
            __eemcs_trace_out_eemcs_state(tra_hdr, s);
            break;
        case TRA_TYPE_BOOT_STATE:
            __eemcs_trace_out_boot_state(tra_hdr, s);
            break;
        case TRA_TYPE_MBX_RX:
        case TRA_TYPE_MBX_TX:
            __eemcs_trace_out_mbx(tra_hdr, s);
            break;
        case TRA_TYPE_XCMD_RX:
        case TRA_TYPE_XCMD_TX:
            __eemcs_trace_out_xcmd(tra_hdr, s);
            break;
        default:
            break;
    }
}

static void _eemcs_trace_out(struct sk_buff *skb, EEMCS_BOOT_TRACE_OUT_TYPE type, struct seq_file *s)
{
    EEMCS_BOOT_TRACE_SET *tra_hdr = NULL;

    if (skb != NULL) {
        tra_hdr = (EEMCS_BOOT_TRACE_SET *)skb->data;
        switch (type)
        {
            case OUT_TYPE_EEMCS_STATE:
                if (tra_hdr->type != TRA_TYPE_EEMCS_STATE)
                    break;
            case OUT_TYPE_BOOT_STATE:
                if (tra_hdr->type != TRA_TYPE_BOOT_STATE)
                    break;
            case OUT_TYPE_MBX:
                if (tra_hdr->type != TRA_TYPE_MBX_RX && tra_hdr->type != TRA_TYPE_MBX_TX)
                    break;
            case OUT_TYPE_XCMD:
                if (tra_hdr->type != TRA_TYPE_XCMD_RX && tra_hdr->type != TRA_TYPE_XCMD_TX)
                    break;
            case OUT_TYPE_ALL:
            default:
                __eemcs_trace_out(skb, s);
        }
    }
}

void eemcs_boot_trace_out(EEMCS_BOOT_TRACE_OUT_TYPE type)
{
#ifdef _EEMCS_TRACE_SUPPORT
    struct sk_buff *first = NULL;
    struct sk_buff *next = NULL;

    if (type < START_OF_TRA_TYPE || type >= END_OF_TRA_TYPE)
        return ;

    if (!eemcs_trace_inst.inited)
        return ;

    if (!skb_queue_empty(&eemcs_trace_inst.log_skb_list)) {
        first = skb_peek(&eemcs_trace_inst.log_skb_list);
        if (first != NULL) {
            _eemcs_trace_out(first, type, NULL);
            while (!skb_queue_is_last(&eemcs_trace_inst.log_skb_list, first)) {
                next = skb_queue_next(&eemcs_trace_inst.log_skb_list, first);
                _eemcs_trace_out(next, type, NULL);
                first = next;
            }
        }
    }
#endif // _EEMCS_TRACE_SUPPORT
}

