/*
 * Copyright (C) 2016 Ingenic Semiconductor Co., Ltd.
 * Author: cli <chen.li@ingenic.com>
 *
 * The Clock Commen Register Interface(kernel.4.4)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/spinlock.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clk-comm.h"

DEFINE_SPINLOCK(ingenic_clk_lock);

int __init ingenic_lookup_register(struct ingenic_clk_provide *ctx, struct clk *clk, int idx)
{
	if (ctx->data.clks && idx >= 0) {
		if (idx < ctx->data.clk_num) {
			ctx->data.clks[idx] = clk;
			return 0;
		} else
			pr_warn("clk id %d is too big than clk nums %d\n", idx, ctx->data.clk_num);
	}
	return -ENOMEM;
}

int __init ingenic_fixclk_register(struct ingenic_clk_provide *ctx, const char* fixclk_name, int idx)
{
	struct clk *clk;
	int err = 0;

	clk = of_clk_get_by_name(ctx->np, fixclk_name);
	if (IS_ERR(clk)) {
		pr_err("%s: no external clock '%s' provided\n",
				__func__, fixclk_name);
		err = -ENODEV;
		goto out;
	}
	err = clk_register_clkdev(clk, fixclk_name, NULL);
	if (err) {
		clk_put(clk);
		goto out;
	}

	ingenic_lookup_register(ctx, clk, idx);
out:
	return err;
}


static const char *dummy_name[] = {
	"dummy_stop",
	"dummy_unkown",
};

int __init ingenic_clk_register(struct ingenic_clk_provide *ctx, const struct ingenic_clk_init *jz_init)
{
	struct clk *clk;
	struct ingenic_clk *iclk;
	struct clk_init_data init;
	int i;
	const char *parent_names[10];

	iclk = (struct ingenic_clk *)kzalloc(sizeof(struct ingenic_clk), GFP_ATOMIC);
	iclk->regbase = ctx->regbase;
	iclk->pm_regmap = ctx->pm_regmap;
	iclk->spinlock = jz_init->spinlock;
	iclk->hwdesc = jz_init->hwdesc;
	iclk->hw.init = &init;

	init.name = ctx->clk_name[jz_init->id];
	init.ops = jz_init->ops;
	init.num_parents = jz_init->num_parents;
	if (init.num_parents > 1) {
		for (i = 0; i < init.num_parents; i++) {
			int pid = jz_init->parents_id[i];
			if (pid >= 0)
				parent_names[i] = __clk_get_name(ctx->data.clks[pid]);
			else if (pid == DUMMY_STOP)
				parent_names[i] = dummy_name[0];
			if (!parent_names[i] || pid == DUMMY_UNKOWN)
				parent_names[i] = dummy_name[1];
		}
	} else {
		parent_names[0] =  __clk_get_name(ctx->data.clks[jz_init->parent_id]);
		if (!parent_names[0])
			parent_names[0] = dummy_name[1];
	}
	init.parent_names = parent_names;
	init.flags = jz_init->flags;

	clk = clk_register(NULL, &(iclk->hw));
	if (IS_ERR(clk)) {
		printk("%s clk register failed %ld\n", init.name, PTR_ERR(clk));
		return PTR_ERR(clk);
	}

	if (jz_init->alias)
		clk_register_clkdev(clk, jz_init->alias_id ?: init.name, NULL);

	ingenic_lookup_register(ctx, clk, jz_init->id);
	return 0;
}

int __init ingenic_clks_register(struct ingenic_clk_provide *ctx, const struct ingenic_clk_init init[], int num)
{
	int i;

	if (ctx == NULL || !ctx->regbase)
		return -EINVAL;

	for (i = 0; i < num; i++)
		ingenic_clk_register(ctx, &init[i]);
	return 0;
}
