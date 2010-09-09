/*
 * STMicroelectronics clock framework
 *
 *  Copyright (C) 2009, STMicroelectronics
 *  Copyright (C) 2010, STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_STM_CLK_H__
#define __ASM_STM_CLK_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/spinlock.h>

struct clk;

struct clk_ops {
	int (*init)(struct clk *clk);
	int (*enable)(struct clk *clk);
	int (*disable)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	int (*recalc)(struct clk *clk);
	long (*round_rate)(struct clk *clk, unsigned long rate);
	int (*observe)(struct clk *clk, unsigned long *div);
	unsigned long (*get_measure)(struct clk *clk);
	void *private_data;
};

struct clk {
	spinlock_t		lock;

	struct list_head	node;

	const char		*name;
	int			id;
	struct module		*owner;

	struct clk		*parent;
	struct clk_ops		*ops;

	void			*private_data;

	struct kref		kref;

	unsigned long		usage_counter;

	unsigned long		rate;
	unsigned long		nominal_rate;
	unsigned long		flags;

	struct list_head	children;
	struct list_head	children_node;
};

#define CLK_ALWAYS_ENABLED	(1 << 0)
#define CLK_RATE_PROPAGATES	(1 << 1)


int clk_init(void);

/*
 * The clk_init calls the:
 * - plat_clk_init() to register the platform clocks
 * - arch_clk_init() to register the arch clock (virtual clocks)
 */
int plat_clk_init(void);
int arch_clk_init(void);


int clk_register(struct clk *);
void clk_unregister(struct clk *);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data);
int clk_for_each_child(struct clk *clk,
		int (*fn)(struct clk *clk, void *data), void *data);
/**
 * Routes the clock on an external pin (if possible)
 */
int clk_observe(struct clk *clk, unsigned long *div);

/**
 * Evaluate the clock rate in hardware (if possible)
 */
unsigned long clk_get_measure(struct clk *clk);

#endif /* __ASM_STM_CLOCK_H */
