/*
 * Copyright (C) 2016 Ingenic Semiconductor Co., Ltd.
 * Author: cli <chen.li@ingenic.com>
 *
 * The PLL Part Of Clock Generate Uint interface for Ingenic's SOC,
 * such as X1000, and so on. (kernel.4.4)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/debugfs.h>
#include <linux/slab.h>
#include "clk-pll.h"


static unsigned long ingenic_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{

	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct ingenic_pll_hwdesc *hwdesc = iclk->hwdesc;
	unsigned od, m, n;
	uint32_t tmp;
	tmp = clkhw_readl(iclk, hwdesc->regoff);

	if (!(tmp & BIT(hwdesc->on_bit)))
		return 0;

	od = (tmp >> hwdesc->od_sft) & GENMASK(hwdesc->od_width - 1, 0);
	n = (tmp >> hwdesc->n_sft) & GENMASK(hwdesc->n_width - 1, 0);
	m = (tmp >> hwdesc->m_sft) & GENMASK(hwdesc->m_width - 1, 0);

	od = hwdesc->od_encode[od];
	m += 1;
	n += 1;

	return div_u64((u64)parent_rate * m * hwdesc->div_step, n * od);
}


#ifdef CONFIG_DEBUG_FS
static ssize_t pll_show_hwdesc(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct ingenic_clk *iclk = file->private_data;
	struct ingenic_pll_hwdesc *hwdesc = iclk->hwdesc;
	char *buf;
	int len = 0, i;
	ssize_t ret;

#define REGS_BUFSIZE	4096
	buf = kzalloc(REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;
	len += snprintf(buf + len, REGS_BUFSIZE - len, "%s hardware description: \n", clk_hw_get_name(&(iclk->hw)));
	len += snprintf(buf + len, REGS_BUFSIZE - len, "regbase  [0x%08x]\n", (u32)iclk->regbase);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "spinlock [%s]%p\n", iclk->spinlock?"YES":"NO", iclk->spinlock);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "regoff  [0x%03x]\n", hwdesc->regoff);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "m_off   [%d]\n", hwdesc->m_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "n_off   [%d]\n", hwdesc->n_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "od_off  [%d]\n", hwdesc->od_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "bit_on  [%d]\n", hwdesc->on_bit);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "bit_en  [%d]\n", hwdesc->en_bit);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "bit_bs  [%d]\n", hwdesc->bs_bit);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "m_msk   [0x%08lx]\n", GENMASK(hwdesc->m_width - 1, 0) << hwdesc->m_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "n_msk   [0x%08lx]\n", GENMASK(hwdesc->n_width - 1, 0) << hwdesc->n_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "od_msk  [0x%08lx]\n", GENMASK(hwdesc->od_width - 1, 0) << hwdesc->od_sft);
	len += snprintf(buf + len, REGS_BUFSIZE - len, "regval  [0x%08x]\n", clkhw_readl(iclk, hwdesc->regoff));
	len += snprintf(buf + len, REGS_BUFSIZE - len, "=========od encode array==========\n");
	for (i = 0; i <= GENMASK(hwdesc->od_width - 1, 0); i++) {
		if (hwdesc->od_encode[i] > 0)
			len += snprintf(buf + len, REGS_BUFSIZE - len, "%d:[0x%02x]\n", i, hwdesc->od_encode[i]);
		else
			len += snprintf(buf + len, REGS_BUFSIZE - len, "%d:[@@@-1@@@@]\n", i);
	}

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
#undef REGS_BUFSIZE
	return ret;
}

static const struct file_operations pll_debug_fops  =  {
	.open = simple_open,
	.read = pll_show_hwdesc,
	.llseek = default_llseek,
};

static int  ingenic_pll_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	struct dentry *ret;
	ret = debugfs_create_file("hwdesc", 0444, dentry, (void *)iclk, &pll_debug_fops);
	if (IS_ERR(ret) || !ret)
		return -ENODEV;
	return 0;
}
#endif

const struct clk_ops __unused ingenic_pll_ro_ops = {
	.recalc_rate	= ingenic_pll_recalc_rate,
#ifdef CONFIG_DEBUG_FS
	.debug_init	= ingenic_pll_debug_init,
#endif
};
