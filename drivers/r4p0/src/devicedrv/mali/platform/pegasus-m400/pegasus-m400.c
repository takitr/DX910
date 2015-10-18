/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include <linux/version.h>
#include "mali_kernel_common.h"
#include "mali_kernel_linux.h"
#include "mali_osk.h"
#include "mali_osk_mali.h"
#include "mali_platform.h"

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/mali/mali_utgard.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>

#include <asm/io.h>


/* Please take special care when lowering the voltage value, since it can *
 * cause system stability problems (random oops, etc.)                    */
unsigned int mali_gpu_vol = 1125000; /* 1.1125 V */

static struct clk *sclk_g3d_clock = NULL;
static struct clk *g3d_clock = NULL;

static struct regulator *g3d_regulator = NULL;
static _mali_osk_mutex_t *mali_dvfs_lock = NULL;




#ifdef CONFIG_REGULATOR
static int mali_regulator_set_voltage(int min_uV, int max_uV)
{
	int voltage;

	_mali_osk_mutex_wait(mali_dvfs_lock);

	if (IS_ERR_OR_NULL(g3d_regulator)) {
		MALI_PRINT_ERROR(("Mali platform: invalid g3d regulator\n"));
		return 1;
	}

	MALI_DEBUG_PRINT(3, ("Mali platform: setting g3d regulator to: %d / %d uV\n", min_uV, max_uV));
	regulator_set_voltage(g3d_regulator, min_uV, max_uV);

	voltage = regulator_get_voltage(g3d_regulator);
	mali_gpu_vol = voltage;
	MALI_DEBUG_PRINT(3, ("Mali platform: g3d regulator set to: %d uV\n", mali_gpu_vol));

	_mali_osk_mutex_signal(mali_dvfs_lock);

	return 0;
}
#endif

static int mali_platform_clk_enable(void)
{
	struct device *dev = &mali_platform_device->dev;
	unsigned long rate;

	sclk_g3d_clock = clk_get(dev, "sclk_g3d");
	if (IS_ERR(sclk_g3d_clock)) {
		MALI_PRINT_ERROR(("Mali platform: failed to get source g3d clock\n"));
		return 1;
	}

	g3d_clock = clk_get(dev, "g3d");
	if (IS_ERR(g3d_clock)) {
		MALI_PRINT_ERROR(("Mali platform: failed to get g3d clock\n"));
		return 1;
	}

	_mali_osk_mutex_wait(mali_dvfs_lock);

	if (clk_prepare_enable(sclk_g3d_clock) < 0) {
		MALI_PRINT_ERROR(("Mali platform: failed to enable source g3d clock\n"));
		return 1;
	}

	if (clk_prepare_enable(g3d_clock) < 0) {
		MALI_PRINT_ERROR(("Mali platform: failed to enable g3d clock\n"));
		return 1;
	}

	rate = clk_get_rate(sclk_g3d_clock);

	MALI_PRINT(("Mali platform: source g3d clock rate = %u MHz\n", rate / 1000000));

	rate = clk_get_rate(g3d_clock);

	MALI_PRINT(("Mali platform: g3d clock rate = %u MHz\n", rate / 1000000));

	_mali_osk_mutex_signal(mali_dvfs_lock);

	return 0;
}

static int mali_platform_init_clk(void)
{
	static int initialized = 0;

	if (initialized) return 1;

	mali_dvfs_lock = _mali_osk_mutex_init(0, 0);
	if (mali_dvfs_lock == NULL) return 1;

	if (mali_platform_clk_enable()) return 1;

#ifdef CONFIG_REGULATOR
	g3d_regulator = regulator_get(&(mali_platform_device->dev), "gpu");

	if (IS_ERR(g3d_regulator)) {
		MALI_PRINT_ERROR(("Mali platform: failed to get g3d regulator\n"));
		goto err_regulator;
	}

	if (regulator_enable(g3d_regulator)) {
		MALI_PRINT_ERROR(("Mali platform: failed to enable g3d regulator\n"));
		goto err_regulator;
	}
	MALI_DEBUG_PRINT(3, ("Mali platform: g3d regulator enabled\n"));

	if (mali_regulator_set_voltage(mali_gpu_vol, mali_gpu_vol)) {
		goto err_regulator;
	}
#endif

	initialized = 1;
	return 0;

#ifdef CONFIG_REGULATOR
err_regulator:
	regulator_put(g3d_regulator);
#endif
	return 1;
}

_mali_osk_errcode_t mali_platform_init(void)
{
	struct cma *default_area = dma_contiguous_default_area;

	MALI_CHECK(mali_platform_init_clk() == 0, _MALI_OSK_ERR_FAULT);

	if (default_area && mali_platform_data) {
		mali_platform_data->fb_start = cma_get_base(default_area);
		mali_platform_data->fb_size = cma_get_size(default_area);
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	if (g3d_clock) {
		clk_disable_unprepare(g3d_clock);
		clk_put(g3d_clock);
		g3d_clock = NULL;
	}

	if (sclk_g3d_clock) {
		clk_disable_unprepare(sclk_g3d_clock);
		clk_put(sclk_g3d_clock);
		sclk_g3d_clock = NULL;
	}

#ifdef CONFIG_REGULATOR
	if (g3d_regulator) {
		regulator_put(g3d_regulator);
		g3d_regulator = NULL;
	}
#endif

	MALI_SUCCESS;
}
