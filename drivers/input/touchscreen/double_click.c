#include <linux/double_click.h>

static bool doubleclick;

void tp_enable_doubleclick(bool state)
{
	doubleclick = state;
}
EXPORT_SYMBOL_GPL(tp_enable_doubleclick);

bool is_tp_doubleclick_enable(void)
{
	return doubleclick;
}
EXPORT_SYMBOL_GPL(is_tp_doubleclick_enable);
