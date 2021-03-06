/*
 * Runtime PM support code for OMAP
 *
 * Author: Kevin Hilman, Deep Root Systems, LLC
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#ifdef CONFIG_PM_RUNTIME
static int omap_pm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int r, ret = 0;

	ret = pm_generic_runtime_suspend(dev);

	if (!ret && dev->parent == &omap_device_parent) {
		r = omap_device_idle(pdev);
		WARN_ON(r);
	}

	return ret;
};

static int omap_pm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int r;

	if (dev->parent == &omap_device_parent) {
		r = omap_device_enable(pdev);
		WARN_ON(r);
	}

	return pm_generic_runtime_resume(dev);
};
#else
#define omap_pm_runtime_suspend NULL
#define omap_pm_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_SUSPEND
int omap_pm_suspend_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->suspend_noirq)
			ret = drv->pm->suspend_noirq(dev);
	}

	/*
	 * The DPM core has done a 'get' to prevent runtime PM
	 * transitions during system PM.  This put is to balance
	 * out that get so that this device can now be runtime
	 * suspended.
	 */
	pm_runtime_put_sync(dev);

	return ret;
}

int omap_pm_resume_noirq(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	int ret = 0;

	/*
	 * This 'get' is to balance the 'put' in the above suspend_noirq
	 * method so that the runtime PM usage counting is in the same
	 * state it was when suspend was called.
	 */
	pm_runtime_get_noresume(dev);

	if (!drv)
		return 0;

	if (drv->pm) {
		if (drv->pm->resume_noirq)
			ret = drv->pm->resume_noirq(dev);
	}

	return ret;
}
#else
#define omap_pm_suspend_noirq NULL
#define omap_pm_resume_noirq NULL
#endif /* CONFIG_SUSPEND */

static int __init omap_pm_runtime_init(void)
{
	const struct dev_pm_ops *pm;
	struct dev_pm_ops *omap_pm;

	pm = platform_bus_get_pm_ops();
	if (!pm) {
		pr_err("%s: unable to get dev_pm_ops from platform_bus\n",
			__func__);
		return -ENODEV;
	}

	omap_pm = kmemdup(pm, sizeof(struct dev_pm_ops), GFP_KERNEL);
	if (!omap_pm) {
		pr_err("%s: unable to alloc memory for new dev_pm_ops\n",
			__func__);
		return -ENOMEM;
	}

	omap_pm->runtime_suspend = omap_pm_runtime_suspend;
	omap_pm->runtime_resume = omap_pm_runtime_resume;
	omap_pm->suspend_noirq = omap_pm_suspend_noirq;
	omap_pm->resume_noirq = omap_pm_resume_noirq;

	platform_bus_set_pm_ops(omap_pm);

	return 0;
}
core_initcall(omap_pm_runtime_init);
