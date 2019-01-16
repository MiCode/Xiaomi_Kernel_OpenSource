#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtk_gpu_utility.h>

unsigned int (*mtk_get_gpu_memory_usage_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_memory_usage_fp);

bool mtk_get_gpu_memory_usage(unsigned int* pMemUsage)
{
    if (NULL != mtk_get_gpu_memory_usage_fp)
    {
        if (pMemUsage)
        {
            *pMemUsage = mtk_get_gpu_memory_usage_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_memory_usage);

unsigned int (*mtk_get_gpu_page_cache_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_page_cache_fp);

bool mtk_get_gpu_page_cache(unsigned int* pPageCache)
{
    if (NULL != mtk_get_gpu_page_cache_fp)
    {
        if (pPageCache)
        {
            *pPageCache = mtk_get_gpu_page_cache_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_page_cache);

unsigned int (*mtk_get_gpu_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_loading_fp);

bool mtk_get_gpu_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_loading);

unsigned int (*mtk_get_gpu_block_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_block_fp);

bool mtk_get_gpu_block(unsigned int* pBlock)
{
    if (NULL != mtk_get_gpu_block_fp)
    {
        if (pBlock)
        {
            *pBlock = mtk_get_gpu_block_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_block);

unsigned int (*mtk_get_gpu_idle_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_idle_fp);

bool mtk_get_gpu_idle(unsigned int* pIdle)
{
    if (NULL != mtk_get_gpu_idle_fp)
    {
        if (pIdle)
        {
            *pIdle = mtk_get_gpu_idle_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_idle);


unsigned int (*mtk_get_gpu_GP_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_GP_loading_fp);

bool mtk_get_gpu_GP_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_GP_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_GP_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_GP_loading);

unsigned int (*mtk_get_gpu_PP_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_PP_loading_fp);

bool mtk_get_gpu_PP_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_PP_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_PP_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_PP_loading);

unsigned int (*mtk_get_gpu_power_loading_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_gpu_power_loading_fp);

bool mtk_get_gpu_power_loading(unsigned int* pLoading)
{
    if (NULL != mtk_get_gpu_power_loading_fp)
    {
        if (pLoading)
        {
            *pLoading = mtk_get_gpu_power_loading_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_gpu_power_loading);

void (*mtk_enable_gpu_dvfs_timer_fp)(bool bEnable) = NULL;
EXPORT_SYMBOL(mtk_enable_gpu_dvfs_timer_fp);

bool mtk_enable_gpu_dvfs_timer(bool bEnable)
{
    if (NULL != mtk_enable_gpu_dvfs_timer_fp)
    {
        mtk_enable_gpu_dvfs_timer_fp(bEnable);
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_enable_gpu_dvfs_timer);


void (*mtk_boost_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_boost_gpu_freq_fp);

bool mtk_boost_gpu_freq(void)
{
    if (NULL != mtk_boost_gpu_freq_fp)
    {
        mtk_boost_gpu_freq_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_boost_gpu_freq);

void (*mtk_set_bottom_gpu_freq_fp)(unsigned int) = NULL;
EXPORT_SYMBOL(mtk_set_bottom_gpu_freq_fp);

bool mtk_set_bottom_gpu_freq(unsigned int ui32FreqLevel)
{
    if (NULL != mtk_set_bottom_gpu_freq_fp)
    {
        mtk_set_bottom_gpu_freq_fp(ui32FreqLevel);
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_set_bottom_gpu_freq);

//-----------------------------------------------------------------------------
unsigned int (*mtk_custom_get_gpu_freq_level_count_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_custom_get_gpu_freq_level_count_fp);

bool mtk_custom_get_gpu_freq_level_count(unsigned int* pui32FreqLevelCount)
{
    if (NULL != mtk_custom_get_gpu_freq_level_count_fp)
    {
        if (pui32FreqLevelCount)
        {
            *pui32FreqLevelCount = mtk_custom_get_gpu_freq_level_count_fp();
            return true;
        }
    }
    return false;
}
EXPORT_SYMBOL(mtk_custom_get_gpu_freq_level_count);

//-----------------------------------------------------------------------------

void (*mtk_custom_boost_gpu_freq_fp)(unsigned int ui32FreqLevel) = NULL;
EXPORT_SYMBOL(mtk_custom_boost_gpu_freq_fp);

bool mtk_custom_boost_gpu_freq(unsigned int ui32FreqLevel)
{
    if (NULL != mtk_custom_boost_gpu_freq_fp)
    {
        mtk_custom_boost_gpu_freq_fp(ui32FreqLevel);
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_custom_boost_gpu_freq);

//-----------------------------------------------------------------------------

void (*mtk_custom_upbound_gpu_freq_fp)(unsigned int ui32FreqLevel) = NULL;
EXPORT_SYMBOL(mtk_custom_upbound_gpu_freq_fp);

bool mtk_custom_upbound_gpu_freq(unsigned int ui32FreqLevel)
{
    if (NULL != mtk_custom_upbound_gpu_freq_fp)
    {
        mtk_custom_upbound_gpu_freq_fp(ui32FreqLevel);
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_custom_upbound_gpu_freq);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_custom_boost_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_custom_boost_gpu_freq_fp);

bool mtk_get_custom_boost_gpu_freq(unsigned int *pui32FreqLevel)
{
    if ((NULL != mtk_get_custom_boost_gpu_freq_fp) && (NULL != pui32FreqLevel))
    {
        *pui32FreqLevel = mtk_get_custom_boost_gpu_freq_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_custom_boost_gpu_freq);

//-----------------------------------------------------------------------------

unsigned int (*mtk_get_custom_upbound_gpu_freq_fp)(void) = NULL;
EXPORT_SYMBOL(mtk_get_custom_upbound_gpu_freq_fp);

bool mtk_get_custom_upbound_gpu_freq(unsigned int *pui32FreqLevel)
{
    if ((NULL != mtk_get_custom_upbound_gpu_freq_fp) && (NULL != pui32FreqLevel))
    {
        *pui32FreqLevel = mtk_get_custom_upbound_gpu_freq_fp();
        return true;
    }
    return false;
}
EXPORT_SYMBOL(mtk_get_custom_upbound_gpu_freq);


