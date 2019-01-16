//#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <mach/eint.h>
//#include <mach/irqs.h>

#define istate core_internal_state__do_not_mess_with_it

static inline void irqd_clear(struct irq_data *d, unsigned int mask)
{
	d->state_use_accessors &= ~mask;
}

static inline void irqd_set(struct irq_data *d, unsigned int mask)
{
	d->state_use_accessors |= mask;
}

static void irq_state_set_masked(struct irq_desc *desc)
{
    irqd_set(&desc->irq_data, IRQD_IRQ_MASKED);
}

/*
 * Bit masks for desc->state
 *
 * IRQS_AUTODETECT		- autodetection in progress
 * IRQS_SPURIOUS_DISABLED	- was disabled due to spurious interrupt
 *				  detection
 * IRQS_POLL_INPROGRESS		- polling in progress
 * IRQS_ONESHOT			- irq is not unmasked in primary handler
 * IRQS_REPLAY			- irq is replayed
 * IRQS_WAITING			- irq is waiting
 * IRQS_PENDING			- irq is pending and replayed later
 * IRQS_SUSPENDED		- irq is suspended
 */
enum {
	IRQS_AUTODETECT		= 0x00000001,
	IRQS_SPURIOUS_DISABLED	= 0x00000002,
	IRQS_POLL_INPROGRESS	= 0x00000008,
	IRQS_ONESHOT		= 0x00000020,
	IRQS_REPLAY		= 0x00000040,
	IRQS_WAITING		= 0x00000080,
	IRQS_PENDING		= 0x00000200,
	IRQS_SUSPENDED		= 0x00000800,
};

extern bool irq_wait_for_poll(struct irq_desc *desc);
extern irqreturn_t handle_irq_event(struct irq_desc *desc);

static bool irq_check_poll(struct irq_desc *desc)
{
	if (!(desc->istate & IRQS_POLL_INPROGRESS))
		return false;
	return irq_wait_for_poll(desc);
}

static inline void mask_ack_irq(struct irq_desc *desc)
{
    if (desc->irq_data.chip->irq_mask_ack)
        desc->irq_data.chip->irq_mask_ack(&desc->irq_data);
    else {
        desc->irq_data.chip->irq_mask(&desc->irq_data);
        if (desc->irq_data.chip->irq_ack)
            desc->irq_data.chip->irq_ack(&desc->irq_data);
    }
    irq_state_set_masked(desc);
}

/*
 * Called unconditionally from handle_eint_level_irq() and only for oneshot
 * interrupts from handle_fasteoi_irq()
 */
static void cond_unmask_irq(struct irq_desc *desc)
{
    /* check whether we should auto unmask EINT */
    struct EINT_HEADER *p = (struct EINT_HEADER *)desc->action->dev_id;

	/*
	 * We need to unmask in the following cases:
	 * - Standard level irq (IRQF_ONESHOT is not set)
	 * - Oneshot irq which did not wake the thread (caused by a
	 *   spurious interrupt or a primary handler handling it
	 *   completely).
	 */
	if (!irqd_irq_disabled(&desc->irq_data) &&
	    irqd_irq_masked(&desc->irq_data) && !desc->threads_oneshot && (!p || (p && p->is_autounmask))) {
        if (desc->irq_data.chip->irq_unmask) {
            desc->irq_data.chip->irq_unmask(&desc->irq_data);
            irqd_clear(&desc->irq_data, IRQD_IRQ_MASKED);
        }
    }
}

/**
 *  handle_eint_level_irq - EINT level type irq handler
 *  @irq:   the interrupt number
 *  @desc:  the interrupt description structure for this irq
 *
 *  Level type interrupts are active as long as the hardware line has
 *  the active level. This may require to mask the interrupt and unmask
 *  it after the associated handler has acknowledged the device, so the
 *  interrupt line is back to inactive.
 */
void
handle_eint_level_irq(unsigned int irq, struct irq_desc *desc)
{
    raw_spin_lock(&desc->lock);
    mask_ack_irq(desc);

    if (unlikely(irqd_irq_inprogress(&desc->irq_data)))
        if (!irq_check_poll(desc))
            goto out_unlock;

    desc->istate &= ~(IRQS_REPLAY | IRQS_WAITING);
    kstat_incr_irqs_this_cpu(irq, desc);

    /*
     * If its disabled or no action available
     * keep it masked and get out of here
     */
    if (unlikely(!desc->action || irqd_irq_disabled(&desc->irq_data)))
        goto out_unlock;

    handle_irq_event(desc);

    cond_unmask_irq(desc);

out_unlock:
    raw_spin_unlock(&desc->lock);
} 

/* EINT irq chip related functions  */
extern unsigned int mt_eint_ack(unsigned int eint_num);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
static void mt_eint_irq_mask(struct irq_data *data)
{
    mt_eint_mask(data->hwirq);
}

static void mt_eint_irq_unmask(struct irq_data *data)
{
    mt_eint_unmask(data->hwirq);
}

static void mt_eint_irq_ack(struct irq_data *data)
{
    mt_eint_ack(data->hwirq);
}

static int mt_eint_irq_set_type(struct irq_data *data, unsigned int type)
{
    int eint_num = data->hwirq;

    if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
        mt_eint_set_polarity(eint_num, MT_EINT_POL_NEG);
    else
        mt_eint_set_polarity(eint_num, MT_EINT_POL_POS);
    if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
        mt_eint_set_sens(eint_num, MT_EDGE_SENSITIVE);
    else
        mt_eint_set_sens(eint_num, MT_LEVEL_SENSITIVE);

    return IRQ_SET_MASK_OK;
}

struct irq_chip mt_irq_eint = {
   .name       = "mt-eint",
   .irq_mask   = mt_eint_irq_mask,
   .irq_unmask = mt_eint_irq_unmask,
   .irq_ack    = mt_eint_irq_ack,
   .irq_set_type   = mt_eint_irq_set_type,
};
