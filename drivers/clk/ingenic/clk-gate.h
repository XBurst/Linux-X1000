#ifndef __INGENIC_CLK_GATE_H__
#define __INGENIC_CLK_GATE_H__

#include "clk-comm.h"

extern const struct clk_ops ingenic_gate_ops;
extern const struct clk_ops ingenic_gate_prepare_ops;

#define GATE_INIT_DATA(_id, _name, _parent,  _flags, _plock, _phwdesc)	\
	CLK_INIT_DATA(_id, _name, _name, NULL, NULL, NULL, &ingenic_gate_ops, _parents,	\
			1, _flags, _plock, _phwdesc)

#define GATE_DESC_REGMAP(_regoff, _bit, _use_regmap)	\
{	\
	.regoff = _regoff, \
	.bit = _bit,	\
	.invert = 0,	\
	.assoc_id = -1,	\
	.enable_count = 0,	\
	.prepare_count = 0,	\
	.assoc_clk = NULL,	\
	.use_regmap = _use_regmap,	\
}


#define GATE_DESC_INVERT_REGMAP(_regoff, _bit, _use_regmap)	\
{	\
	.regoff = _regoff, \
	.bit = _bit,	\
	.invert = 1,	\
	.assoc_id = -1,	\
	.enable_count = 0,	\
	.prepare_count = 0,	\
	.assoc_clk = NULL,	\
	.use_regmap = _use_regmap,	\
}


#define GATE_DESC_1_REGMAP(_regoff, _bit, _assoc_id, _use_regmap)	\
{	\
	.regoff = _regoff, \
	.bit = _bit,	\
	.invert = 0,	\
	.assoc_id = _assoc_id,	\
	.enable_count = 0,	\
	.prepare_count = 0,	\
	.assoc_clk = NULL,	\
	.use_regmap = _use_regmap,	\
}


#define GATE_DESC_1_INVERT_REGMAP(_regoff, _bit, _assoc_id, _use_regmap)	\
{	\
	.regoff = _regoff, \
	.bit = _bit,	\
	.invert = 1,	\
	.assoc_id = _assoc_id,	\
	.enable_count = 0,	\
	.prepare_count = 0,	\
	.assoc_clk = NULL,	\
	.use_regmap = _use_regmap,	\
}

#define GATE_DESC(_regoff, _bit)		GATE_DESC_REGMAP(_regoff, _bit, 0)
#define GATE_DESC_INVERT(_regoff, _bit)		GATE_DESC_INVERT_REGMAP(_regoff, _bit, 0)
#define GATE_DESC_1(_regoff, _bit, _assoc_id)	GATE_DESC_1_REGMAP(_regoff, _bit, _assoc_id, 0)
#define GATE_DESC_1_INVERT(_regoff, _bit, _assoc_id)	GATE_DESC_1_INVERT_REGMAP(_regoff, _bit, _assoc_id, 0)

struct ingenic_gate_hwdesc {
	u32 regoff;	 /*Clock Control Register*/
	u8 bit:6;	 /*Clock Source select shift in register*/
	u8 invert:1;	 /*Normal 1 is gate, 0 is ungate, invert 1 is ungate, 0 is gate*/
	u8 use_regmap:1; /*usb struct regmap or void * __iomem regbase*/
	int assoc_id;	 /*Associated Clock id, < 0 unused*/
	struct clk *assoc_clk;
	int enable_count;
	int prepare_count;
};
#endif /*__INGENIC_CLK_GATE_H__*/
