#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <xhci.h>
#include <linux/xhci/xhci-mtk.h>

/* ///////////////////////////////////////////////////////////////////////// */
/* interrupt moderation */
/* ///////////////////////////////////////////////////////////////////////// */

static unsigned int imod;	/* attribute for Interrupt moderation register */

/*
 * show the value of imod.
 */
static ssize_t imod_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	imod = xhci_readl(mtk_xhci, (void __iomem *) &mtk_xhci->ir_set->irq_control);
	imod &= ~0xFFFF0000;
	return sprintf(buf, "0x%x\n", imod);
}

/*
 * write the value to imod and config hardware.
 */
static ssize_t imod_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	u32 temp;

	sscanf(buf, "%xu", &imod);

	temp = xhci_readl(mtk_xhci, (void __iomem *) &mtk_xhci->ir_set->irq_control);
	temp &= ~0xFFFF;
	temp |= imod;
	xhci_writel(mtk_xhci, temp, (void __iomem *) &mtk_xhci->ir_set->irq_control);

	return count;
}

static struct kobj_attribute imod_attribute = __ATTR(imod, 0666, imod_show, imod_store);


/* ///////////////////////////////////////////////////////////////////////// */
/* DMA burst */
/* ///////////////////////////////////////////////////////////////////////// */

static unsigned int dburst;	/* attribute for Interrupt moderation register */

/*
 * show the value of dburst.
 */
static ssize_t dburst_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	dburst = xhci_readl(mtk_xhci, (void __iomem *)_SSUSB_XHCI_HDMA_CFG(mtk_xhci->base_regs));
	return sprintf(buf, "0x%x\n", dburst);
}

/*
 * write the value to dburst and config hardware.
 */
static ssize_t dburst_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	u32 temp;

	sscanf(buf, "%xu", &dburst);

	/* This register configures the maximum burst size per DMA request to DRAM bus. */
	temp = xhci_readl(mtk_xhci, (void __iomem *)_SSUSB_XHCI_HDMA_CFG(mtk_xhci->base_regs));
	/* clear dma r/w fields */
	temp &= ~0x0C0C;
	/* set r/w burst size */
	temp |= (dburst << 2 | dburst << 10);
	xhci_writel(mtk_xhci, temp, (void __iomem *)_SSUSB_XHCI_HDMA_CFG(mtk_xhci->base_regs));

	return count;
}

static struct kobj_attribute dburst_attribute = __ATTR(dburst, 0666, dburst_show, dburst_store);


/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&imod_attribute.attr,
	&dburst_attribute.attr,
	NULL,			/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *xhci_attrs_kobj;

int xhci_attrs_init(void)
{
	int retval;

	/*
	 * Create a simple kobject with the name of "xhci",
	 * located under /sys/kernel/
	 *
	 */
	xhci_attrs_kobj = kobject_create_and_add("xhci", kernel_kobj);
	if (!xhci_attrs_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(xhci_attrs_kobj, &attr_group);
	if (retval)
		kobject_put(xhci_attrs_kobj);

	return retval;
}

void xhci_attrs_exit(void)
{
	kobject_put(xhci_attrs_kobj);
}
