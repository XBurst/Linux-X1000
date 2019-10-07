#ifndef __INGENIC_COMM_CLK_H__
#define __INGENIC_COMM_CLK_H__

#include <linux/clk-provider.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <asm/io.h>

#define __unused __attribute__((unused))
extern spinlock_t ingenic_clk_lock;

struct ingenic_clk_provide {
	struct device_node *np;
	void __iomem *regbase;
	struct regmap *pm_regmap;
	struct clk_onecell_data data;
	const char **clk_name;
};

struct ingenic_clk {
	struct clk_hw hw;
	void __iomem *regbase;
	struct regmap *pm_regmap;
	spinlock_t *spinlock;
	void  *hwdesc;		/*hardware description*/
};

static void inline clkhw_writel(struct ingenic_clk *clk, off_t offset, unsigned val)
{
	writel_relaxed(val, clk->regbase + offset);
}

static u32 inline clkhw_readl(struct ingenic_clk *clk, off_t offset)
{
	return readl_relaxed(clk->regbase + offset);
}

static void inline clkhw_ops_bit(struct ingenic_clk *clk, int offset, int set, int clr)
{
	u32 tmp;
	tmp = readl_relaxed(clk->regbase + offset);
	tmp |= set;
	tmp &= ~clr;
	writel_relaxed(tmp, clk->regbase + offset);
}

static void inline clkhw_set_bit(struct ingenic_clk *clk, int offset, int bit)
{
	clkhw_ops_bit(clk, offset, bit, 0);
}

static void inline clkhw_clr_bit(struct ingenic_clk *clk, int offset, int bit)
{
	clkhw_ops_bit(clk, offset, 0, bit);
}

static int inline clkhw_test_bit(struct ingenic_clk *clk, int offset, int bit)
{
	return !!(clkhw_readl(clk, offset) & bit);
}

#define to_ingenic_clk(clk_hw) container_of(clk_hw, struct ingenic_clk, hw)

#define CLK_INIT_DATA(_id, _ops, _pids, _parentnum, _flags, _plock, _phwdesc)	\
{	\
	.id = _id,	\
	._ops = _ops,	\
	.parents_id = _pids, \
	.num_parents = _parentnum, \
	.flags = _flags,	\
	.spinlock = _plock,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

#define CLK_INIT_DATA_LOCKED(_id, _ops, _pids, _parentnum, _flags, _phwdesc)	\
{	\
	.id = _id,	\
	.ops = _ops,	\
	.parents_id = _pids,  \
	.num_parents = _parentnum, \
	.flags = _flags,	\
	.spinlock = &ingenic_clk_lock,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

#define CLK_INIT_DATA_UNLOCK(_id, _ops, _pids, _parentnum, _flags, _phwdesc)	\
{	\
	.id = _id,	\
	.ops = _ops,	\
	.parents_id = _pids, \
	.num_parents = _parentnum, \
	.flags = _flags,	\
	.spinlock = NULL,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

#define CLK_INIT_DATA1(_id, _ops, _pid, _flags, _plock, _phwdesc)	\
{	\
	.id = _id,	\
	.ops = _ops,	\
	.parent_id = _pid,  \
	.num_parents = 1, \
	.flags = _flags,	\
	.spinlock = _plock,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

#define CLK_INIT_DATA1_LOCKED(_id, _ops, _pid, _flags, _phwdesc)	\
{	\
	.id = _id,	\
	.ops = _ops,	\
	.parent_id = _pid,  \
	.num_parents = 1, \
	.flags = _flags,	\
	.spinlock = &ingenic_clk_lock,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

#define CLK_INIT_DATA1_UNLOCK(_id, _ops, _pid, _flags, _phwdesc)	\
{	\
	.id = _id,	\
	.ops = _ops,	\
	.parent_id = _pid,  \
	.num_parents = 1, \
	.flags = _flags,	\
	.spinlock = NULL,	\
	.hwdesc = _phwdesc,	\
	.alias = 1,	\
}

struct ingenic_clk_init {
	int			id;		/*clk id*/
	const struct clk_ops	*ops;
	unsigned long		flags;		/*flags to clk*/
	spinlock_t		*spinlock;
	void			*hwdesc;	/*hardware description*/
	u8			num_parents;
	bool			alias;
	const char		*alias_id;	/*alias name for lookup*/
	union {
		const int	*parents_id;
		int		parent_id;
	};
};

enum {
	DUMMY_STOP = -2,
	DUMMY_UNKOWN = -1,
};

int __init ingenic_lookup_register(struct ingenic_clk_provide *ctx, struct clk *clk, int idx);
int __init ingenic_fixclk_register(struct ingenic_clk_provide *ctx, const char* fixclk_name, int idx);
int __init ingenic_clk_register(struct ingenic_clk_provide *ctx, const struct ingenic_clk_init *init);
int __init ingenic_clks_register(struct ingenic_clk_provide *ctx, const struct ingenic_clk_init init[], int num);
#endif /*__INGENIC_COMM_CLK_H__*/
