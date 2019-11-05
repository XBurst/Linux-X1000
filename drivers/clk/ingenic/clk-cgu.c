/*
 * Copyright (C) 2016 Ingenic Semiconductor Co., Ltd.
 * Author: cli <chen.li@ingenic.com>
 *
 * The Peripherals Part Of Clock Generate Uint interface for Ingenic's SOC,
 * such as X1000, and so on. (kernel.4.4)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include "clk-cgu.h"

int ingenic_cgu_set_parent(struct clk_hw *hw, u8 index)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct clk_hw *clk_hw_parent = clk_hw_get_parent_by_index(hw, index);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	unsigned long parent_rate, rate;
	unsigned long flags;
	uint32_t xcdr, cdr;
	int ret = 0;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);

	if (clk_hw_get_flags(hw) & CLK_SET_PARENT_GATE) {
		if (!(xcdr & BIT(hwdesc->bit_stop))) {
			xcdr |= BIT(hwdesc->bit_stop);
			clkhw_writel(iclk, hwdesc->regoff, (xcdr | BIT(hwdesc->bit_ce)));
		}
	}

	parent_rate = clk_hw_get_rate(clk_hw_parent);
	rate = clk_hw_get_rate(hw);

	/*re caculate rate*/
	cdr = DIV_ROUND_UP_ULL((u64)parent_rate, rate * hwdesc->div_step) - 1;


	xcdr &= ~(hwdesc->cdr_msk << hwdesc->cdr_off);
	xcdr |= (cdr & hwdesc->cdr_msk) << hwdesc->cdr_off;
	xcdr &= ~(hwdesc->cs_msk << hwdesc->cs_off);
	xcdr |= (index & hwdesc->cs_msk) << hwdesc->cs_off;
	clkhw_writel(iclk, hwdesc->regoff, xcdr);

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return ret;
}

u8 ingenic_cgu_get_parent(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t xcdr;
	unsigned long flags;
	u8 cs;
	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);

	cs = (xcdr >> hwdesc->cs_off) & hwdesc->cs_msk;

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return cs;
}

unsigned long ingenic_cgu_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	int div;
	unsigned long rate, flags;
	uint32_t xcdr;
	u8 cs;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);
	cs = (xcdr >> hwdesc->cs_off) & hwdesc->cs_msk;

	if ((u8)hwdesc->cs_exclk != cs) {
		div = (xcdr >> hwdesc->cdr_off) & hwdesc->cdr_msk;
		div += 1;
		div = div * hwdesc->div_step;
		rate = DIV_ROUND_UP_ULL((u64)parent_rate, div);
	} else {
		rate = parent_rate;
	}

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return rate;
}

long ingenic_cgu_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	long div;

	div = DIV_ROUND_UP_ULL((u64)(*parent_rate) , rate * hwdesc->div_step);

	return  DIV_ROUND_UP_ULL((u64)(*parent_rate), div* hwdesc->div_step);
}

int ingenic_cgu_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t xcdr, cdr, cdr_tmp, cs;
	int is_enabled = 0;
	unsigned long flags;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_GATE) {
		if (unlikely(!(xcdr & BIT(hwdesc->bit_stop)))) {
			printk("=====> %s %d\n", __func__, __LINE__);
			xcdr |= BIT(hwdesc->bit_stop);
			clkhw_writel(iclk, hwdesc->regoff, (xcdr | BIT(hwdesc->bit_ce)));
			is_enabled = 1;
		}
	}

	/*use exclk or other fixclk source, and not need to do unstop ops*/
	cs = (xcdr >> hwdesc->cs_off) & hwdesc->cs_msk;
	if (cs == (u8)hwdesc->cs_exclk) {
		if (is_enabled)	/*clear ce*/
			clkhw_writel(iclk, hwdesc->regoff, xcdr);
		goto out;
	}

	cdr_tmp = (xcdr >>  hwdesc->cdr_off) & hwdesc->cdr_msk;
	cdr = DIV_ROUND_UP_ULL((u64)parent_rate , rate * hwdesc->div_step) - 1;

	/*cdr not change, skip reset rate*/
	if (cdr == cdr_tmp && !is_enabled)
		goto out;

	xcdr &= ~(hwdesc->cdr_msk << hwdesc->cdr_off);
	xcdr |= (cdr >> hwdesc->cdr_off) & hwdesc->cdr_msk;
	clkhw_writel(iclk, hwdesc->regoff, xcdr);
out:
	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return 0;
}

int ingenic_cgu_enable(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t xcdr;
	u8 cs;
	unsigned long flags;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);
	cs = (xcdr >> hwdesc->cs_off) & hwdesc->cs_msk;

	/*use exclk, stop cgu to save power*/
	if (cs == (u8)hwdesc->cs_exclk) {
		if (unlikely(!(xcdr & BIT(hwdesc->bit_stop)))) {
			xcdr |= BIT(hwdesc->bit_stop);
			clkhw_writel(iclk, hwdesc->regoff, (xcdr | BIT(hwdesc->bit_ce)));
			clkhw_writel(iclk, hwdesc->regoff, xcdr);
		}
		goto out;
	}

	if (!(xcdr & BIT(hwdesc->bit_stop)) && !(xcdr & BIT(hwdesc->bit_ce))) /*bootloader don not take care of ce*/
		goto out;

	xcdr &= ~BIT(hwdesc->bit_stop);
	xcdr |= BIT(hwdesc->bit_ce);
	clkhw_writel(iclk, hwdesc->regoff, xcdr);

	while (clkhw_test_bit(iclk, hwdesc->regoff, BIT(hwdesc->bit_busy)))
		printk("wait stable.[%d][%s]\n",__LINE__, clk_hw_get_name(hw));

	xcdr &= ~BIT(hwdesc->bit_ce);
	clkhw_writel(iclk, hwdesc->regoff, xcdr);
out:
	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return 0;
}

void ingenic_cpu_disable(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t xcdr;
	unsigned long flags;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);
	if (xcdr & BIT(hwdesc->bit_stop))
		goto out;

	xcdr |= BIT(hwdesc->bit_ce) | BIT(hwdesc->bit_stop);
	clkhw_writel(iclk, hwdesc->regoff, xcdr);

	xcdr &= ~BIT(hwdesc->bit_ce);
	clkhw_writel(iclk, hwdesc->regoff, xcdr);

out:
	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);

	return;
}

int ingenic_cgu_is_enabled(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_cgu_hwdesc *hwdesc = iclk->hwdesc;
	uint32_t xcdr;
	unsigned long flags;
	u8 cs;
	int ret = 0;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, hwdesc->regoff);
	cs = (xcdr >> hwdesc->cs_off) & hwdesc->cs_msk;
	if (cs == (u8)hwdesc->cs_exclk)
		ret = 1;
	else
		ret = (int)(!(xcdr & BIT(hwdesc->bit_stop)));

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);

	return ret;
}

const struct clk_ops __unused ingenic_cgu_mux_ops = {
	.set_parent = ingenic_cgu_set_parent,
	.get_parent = ingenic_cgu_get_parent,
};

const struct clk_ops __unused ingenic_cgu_ops = {
	.set_parent = ingenic_cgu_set_parent,
	.get_parent = ingenic_cgu_get_parent,
	.enable = ingenic_cgu_enable,
	.disable = ingenic_cpu_disable,
	.is_enabled = ingenic_cgu_is_enabled,
	.recalc_rate = ingenic_cgu_recalc_rate,
	.round_rate = ingenic_cgu_round_rate,
	.set_rate = ingenic_cgu_set_rate,
};

const struct clk_ops __unused ingenic_cgu_nomux_ops = {
	.enable = ingenic_cgu_enable,
	.disable = ingenic_cpu_disable,
	.is_enabled = ingenic_cgu_is_enabled,
	.recalc_rate = ingenic_cgu_recalc_rate,
	.round_rate = ingenic_cgu_round_rate,
	.set_rate = ingenic_cgu_set_rate,
};

const struct clk_ops __unused ingenic_cgu_ro_ops = {
	.enable = ingenic_cgu_enable,
	.disable = ingenic_cpu_disable,
	.is_enabled = ingenic_cgu_is_enabled,
	.recalc_rate = ingenic_cgu_recalc_rate,
	.get_parent = ingenic_cgu_get_parent,
};
