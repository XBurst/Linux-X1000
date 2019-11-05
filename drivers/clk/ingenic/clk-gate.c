/*
 * Copyright (C) 2016 Ingenic Semiconductor Co., Ltd.
 * Author: cli <chen.li@ingenic.com>
 *
 * The CLock Gate Interface for Ingenic's SOC, such as X1000,
 * and so on. (kernel.4.4)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include "clk-gate.h"

static int ingenic_gate_enable(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_gate_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t gate, gate_tmp;
	unsigned long flags;
	int ret = 0;

	if (unlikely(hwdesc->use_regmap)) {
		ret = regmap_update_bits_check(iclk->pm_regmap,
				hwdesc->regoff,
				BIT(hwdesc->bit),
				hwdesc->invert ? BIT(hwdesc->bit) : ~BIT(hwdesc->bit),
				NULL);
		pr_debug("regmap %s hwdesc->bit = %d gate [%x] %x\n", clk_hw_get_name(hw),
				hwdesc->bit, hwdesc->regoff, clkhw_readl(iclk, hwdesc->regoff));
		if (ret)
			return ret;
		else
			goto out;
	}

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);


	gate_tmp = gate = clkhw_readl(iclk, hwdesc->regoff);

	if (hwdesc->invert)
		gate |= BIT(hwdesc->bit);
	else
		gate &= ~BIT(hwdesc->bit);

	if (gate_tmp != gate)
		clkhw_writel(iclk, hwdesc->regoff, gate);

	pr_debug("%s hwdesc->bit = %d gate [%x] %x\n", clk_hw_get_name(hw),
			hwdesc->bit, hwdesc->regoff, clkhw_readl(iclk, hwdesc->regoff));

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);

out:
	if (!IS_ERR_OR_NULL(hwdesc->assoc_clk)) {
		ret = clk_enable(hwdesc->assoc_clk);
		if (!ret)
			hwdesc->enable_count++;
	}
	return ret;
}

static void ingenic_cpu_disable(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_gate_hwdesc *hwdesc = iclk->hwdesc;
	unsigned gate, gate_tmp;
	unsigned long flags;
	int ret;

	if (unlikely(hwdesc->use_regmap)) {
		ret = regmap_update_bits_check(iclk->pm_regmap,
				hwdesc->regoff,
				BIT(hwdesc->bit),
				!hwdesc->invert ? BIT(hwdesc->bit) : ~BIT(hwdesc->bit),
				NULL);

		pr_debug("regmap %s hwdesc->bit = %d gate [%x] %x\n", clk_hw_get_name(hw),
				hwdesc->bit, hwdesc->regoff, clkhw_readl(iclk, hwdesc->regoff));
		if (ret) {
			pr_err("ERROR: check regmap %s hwdesc->bit = %d gate [%x] disable failed\n",
					clk_hw_get_name(hw), hwdesc->bit, hwdesc->regoff);
		}
		goto out;
	}

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	gate_tmp = gate = clkhw_readl(iclk, hwdesc->regoff);

	if (!hwdesc->invert)
		gate |= BIT(hwdesc->bit);
	else
		gate &= ~BIT(hwdesc->bit);

	if (gate_tmp != gate)
		clkhw_writel(iclk, hwdesc->regoff, gate);

	pr_debug("%s hwdesc->bit = %d gate [%x] %x\n", clk_hw_get_name(hw),
			hwdesc->bit, hwdesc->regoff, clkhw_readl(iclk, hwdesc->regoff));

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
out:
	if (!IS_ERR_OR_NULL(hwdesc->assoc_clk) && hwdesc->enable_count) {
		clk_disable(hwdesc->assoc_clk);
		hwdesc->enable_count--;
	}
	return;
}

static int ingenic_gate_is_enabled(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_gate_hwdesc *hwdesc = iclk->hwdesc;
	unsigned gate;

	/*uncare assoc clk, for sys late disable */
	gate = clkhw_readl(iclk, hwdesc->regoff);
	if (!hwdesc->invert)
		return !(gate & BIT(hwdesc->bit));
	else
		return (gate & BIT(hwdesc->bit));
}

const struct clk_ops ingenic_gate_ops = {
	.enable = ingenic_gate_enable,
	.disable = ingenic_cpu_disable,
	.is_enabled = ingenic_gate_is_enabled,
};

static int ingenic_gate_prepare(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_gate_hwdesc *hwdesc = iclk->hwdesc;
	int ret = 0;

	if (!IS_ERR_OR_NULL(hwdesc->assoc_clk)) {
		ret = clk_prepare(hwdesc->assoc_clk);
		if (!ret)
			hwdesc->prepare_count++;
	}
	return ret;
}

static void ingenic_gate_unprepare(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_gate_hwdesc *hwdesc = iclk->hwdesc;

	if (!IS_ERR_OR_NULL(hwdesc->assoc_clk) && hwdesc->prepare_count) {
		clk_unprepare(hwdesc->assoc_clk);
		hwdesc->prepare_count--;
	}
	return;
}

const struct clk_ops ingenic_gate_prepare_ops = {
	.prepare = ingenic_gate_prepare,
	.unprepare = ingenic_gate_unprepare,
	.enable = ingenic_gate_enable,
	.disable = ingenic_cpu_disable,
	.is_enabled = ingenic_gate_is_enabled,
};
