/*
 *  Copyright (C) 2017, bo.liu <bo.liu@ingenic.com>
 *  INGENIC Watchdog driver
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <linux/mfd/core.h>
#include <linux/mfd/ingenic-tcu.h>

#define DEFAULT_HEARTBEAT 5
#define MAX_HEARTBEAT     2048

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		 "Watchdog cannot be stopped once started (default="
		 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static unsigned int heartbeat = DEFAULT_HEARTBEAT;
module_param(heartbeat, uint, 0);
MODULE_PARM_DESC(heartbeat,
		"Watchdog heartbeat period in seconds from 1 to "
		__MODULE_STRING(MAX_HEARTBEAT) ", default "
		__MODULE_STRING(DEFAULT_HEARTBEAT));

struct ingenic_wdt_drvdata {
	struct watchdog_device wdt;
	struct platform_device *pdev;
	const struct mfd_cell *cell;

	struct clk *rtc_clk;
	struct clk *ext_clk;
	struct notifier_block restart_handler;
};

static int ingenic_wdt_ping(struct watchdog_device *wdt_dev)
{
	ingenic_watchdog_set_count(0);
	return 0;
}

static inline int get_clk_div(unsigned int *timeout_value)
{
	int clk_div = TCU_PRESCALE_1;

	while (*timeout_value > 0xffff) {
		if (clk_div == TCU_PRESCALE_1024) {
			/* Requested timeout too high;
			 * use highest possible value. */
			*timeout_value = 0xffff;
			clk_div = -1;
			break;
		}
		*timeout_value >>= 2;
		clk_div += 1;
	}

	return clk_div;
}
static void wdt_config(struct ingenic_wdt_drvdata *drvdata,
					   unsigned int new_timeout)
{
	unsigned int rtc_clk_rate;
	unsigned int ext_clk_rate;
	unsigned int timeout_value;
	unsigned int clk_src;
	int clk_div;

	clk_src = TCU_CLKSRC_RTC;
	rtc_clk_rate = clk_get_rate(drvdata->rtc_clk);
	timeout_value = rtc_clk_rate * new_timeout;
	clk_div = get_clk_div(&timeout_value);

	if(clk_div < 0) {
		clk_src = TCU_CLKSRC_EXT;
		ext_clk_rate = clk_get_rate(drvdata->ext_clk);
		timeout_value = ext_clk_rate * new_timeout;
		clk_div = get_clk_div(&timeout_value);
		if(	clk_div < 0){
			printk("Requested timeout too high, use highest possible value\n");
			clk_div = TCU_PRESCALE_1024;
			timeout_value = 0xffff;
		}
	}

	ingenic_watchdog_config((clk_div << 3) | clk_src, timeout_value);
}
static int ingenic_wdt_set_timeout(struct watchdog_device *wdt_dev,
								   unsigned int new_timeout)
{
	struct ingenic_wdt_drvdata *drvdata = watchdog_get_drvdata(wdt_dev);

	wdt_config(drvdata, new_timeout);
	wdt_dev->timeout = new_timeout;
	return 0;
}

static int ingenic_wdt_start(struct watchdog_device *wdt_dev)
{
	struct ingenic_wdt_drvdata *drvdata = watchdog_get_drvdata(wdt_dev);

	drvdata->cell->enable(drvdata->pdev);
	ingenic_wdt_set_timeout(wdt_dev, wdt_dev->timeout);

	return 0;
}

static int ingenic_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct ingenic_wdt_drvdata *drvdata = watchdog_get_drvdata(wdt_dev);
	drvdata->cell->disable(drvdata->pdev);

	return 0;
}

static const struct watchdog_info ingenic_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "ingenic Watchdog",
};

static const struct watchdog_ops ingenic_wdt_ops = {
	.owner = THIS_MODULE,
	.start = ingenic_wdt_start,
	.stop = ingenic_wdt_stop,
	.ping = ingenic_wdt_ping,
	.set_timeout = ingenic_wdt_set_timeout,
};

static int ingenic_reset_handler(struct notifier_block *this, unsigned long mode,
								 void *cmd)
{
	struct ingenic_wdt_drvdata *drvdata;

	drvdata = container_of(this, struct ingenic_wdt_drvdata, restart_handler);
	drvdata->cell->enable(drvdata->pdev);
	wdt_config(drvdata, 4);
	while (1) {
		mdelay(500);
	}

	return NOTIFY_DONE;
}

#ifdef CONFIG_OF
static const struct of_device_id ingenic_wdt_of_matches[] = {
	{ .compatible = "ingenic,watchdog", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ingenic_wdt_of_matches)
#endif

static int ingenic_wdt_probe(struct platform_device *pdev)
{
	struct ingenic_wdt_drvdata *drvdata;
	struct watchdog_device *ingenic_wdt;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(struct ingenic_wdt_drvdata),
			       GFP_KERNEL);
	if (!drvdata) {
		dev_err(&pdev->dev, "Unable to alloacate watchdog device\n");
		return -ENOMEM;
	}

	drvdata->cell = mfd_get_cell(pdev);
	if (!drvdata->cell) {
		dev_err(&pdev->dev, "Failed to get mfd cell\n");
		return -ENOMEM;
	}
	drvdata->pdev = pdev;
	if (heartbeat < 1 || heartbeat > MAX_HEARTBEAT)
		heartbeat = DEFAULT_HEARTBEAT;

	ingenic_wdt = &drvdata->wdt;
	ingenic_wdt->info = &ingenic_wdt_info;
	ingenic_wdt->ops = &ingenic_wdt_ops;
	ingenic_wdt->timeout = heartbeat;
	ingenic_wdt->min_timeout = 1;
	ingenic_wdt->max_timeout = MAX_HEARTBEAT;
	ingenic_wdt->parent = &pdev->dev;
	watchdog_set_nowayout(ingenic_wdt, nowayout);
	watchdog_set_drvdata(ingenic_wdt, drvdata);

	drvdata->ext_clk = devm_clk_get(&pdev->dev, "ext");
	if (IS_ERR(drvdata->ext_clk)) {
		dev_err(&pdev->dev, "cannot find EXT clock\n");
		ret = PTR_ERR(drvdata->ext_clk);
		goto err_out;
	}

	drvdata->rtc_clk = devm_clk_get(&pdev->dev, "rtc");
	if (IS_ERR(drvdata->rtc_clk)) {
		dev_err(&pdev->dev, "cannot find RTC clock\n");
		ret = PTR_ERR(drvdata->rtc_clk);
		goto err_disable_ext_clk;
	}

	ret = watchdog_register_device(&drvdata->wdt);
	if (ret < 0)
		goto err_disable_clk;

	platform_set_drvdata(pdev, drvdata);

	drvdata->restart_handler.notifier_call = ingenic_reset_handler;
	drvdata->restart_handler.priority = 128;
	ret = register_restart_handler(&drvdata->restart_handler);
	if (ret)
		dev_warn(&pdev->dev,
				 "cannot register restart handler (err=%d)\n", ret);

	return 0;

err_disable_clk:
	clk_put(drvdata->rtc_clk);
err_disable_ext_clk:
	clk_put(drvdata->ext_clk);
err_out:
	return ret;
}

static int ingenic_wdt_remove(struct platform_device *pdev)
{
	struct ingenic_wdt_drvdata *drvdata = platform_get_drvdata(pdev);

	ingenic_wdt_stop(&drvdata->wdt);
	watchdog_unregister_device(&drvdata->wdt);
	clk_put(drvdata->ext_clk);
	clk_put(drvdata->rtc_clk);
	devm_kfree(&pdev->dev, drvdata);
	return 0;
}

static struct platform_driver ingenic_wdt_driver = {
	.probe = ingenic_wdt_probe,
	.remove = ingenic_wdt_remove,
	.driver = {
		.name = "ingenic,watchdog",
		.of_match_table = of_match_ptr(ingenic_wdt_of_matches),
	},
};

module_platform_driver(ingenic_wdt_driver);

MODULE_AUTHOR("bo.liu <bo.liu@ingenic.com>");
MODULE_DESCRIPTION("ingenic Watchdog Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ingenic-wdt");
