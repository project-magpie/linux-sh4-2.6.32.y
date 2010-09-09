/*
 * STMicroelectronics clock framework
 *
 *  Copyright (C) 2009, STMicroelectronics
 *  Copyright (C) 2010, STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License V2 _ONLY_.  See the file "COPYING" in the main directory of
 * this archive for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/stm/clk.h>

static LIST_HEAD(clks_list);
static DEFINE_MUTEX(clks_list_sem);

static int __clk_init(struct clk *clk)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->init)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->init(clk);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_enable(struct clk *clk)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->enable)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->enable(clk);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_disable(struct clk *clk)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->disable)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->disable(clk);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->set_rate)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->set_rate(clk, rate);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->set_parent)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->set_parent(clk, parent);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_recalc_rate(struct clk *clk)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->recalc)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->recalc(clk);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_observe(struct clk *clk, unsigned long *div)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->observe)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->observe(clk, div);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static int __clk_get_measure(struct clk *clk)
{
	int ret = 0;
	if (likely(clk->ops && clk->ops->get_measure)) {
		unsigned long flags;
		spin_lock_irqsave(&clk->lock, flags);
		ret = clk->ops->get_measure(clk);
		spin_unlock_irqrestore(&clk->lock, flags);
	}
	return ret;
}

static inline int clk_is_always_enabled(struct clk *clk)
{
	return clk->flags & CLK_ALWAYS_ENABLED;
}

static inline int clk_wants_propagate(struct clk *clk)
{
	return clk->flags & CLK_RATE_PROPAGATES;
}

static int __clk_for_each_child(struct clk *clk,
		int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn || !clk)
		return -EINVAL;

	list_for_each_entry(clkp, &clk->children, children_node)
		result |= fn(clkp, data);

	return result;
}

static void clk_propagate_rate(struct clk *clk);

static int __clk_propagate_rate(struct clk *clk, void *data)
{
	__clk_recalc_rate(clk);

	if (likely(clk_wants_propagate(clk)))
		clk_propagate_rate(clk);

	return 0;
}

static void clk_propagate_rate(struct clk *clk)
{
	__clk_for_each_child(clk, __clk_propagate_rate, NULL);
}

int clk_enable(struct clk *clk)
{
	int ret = 0;

	if (!clk)
		return -EINVAL;

	if (clk_is_always_enabled(clk))
		/* No enable required! */
		return 0;

	if (clk->usage_counter++ == 0) {
		if (clk->parent)
			clk_enable(clk->parent);

		ret = __clk_enable(clk);
		if (ret) { /* on error */
			if (clk->parent)
				clk_disable(clk->parent);
			--clk->usage_counter;
		}
	}
	return ret;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	int ret;
	if (!clk)
		return;

	if (clk_is_always_enabled(clk))
		/* this clock can not be disabled */
		return;

	if (--clk->usage_counter == 0) {
		ret = __clk_disable(clk);
		if (ret) {/* on error */
			clk->usage_counter++;
			return;
		}
		if (clk->parent)
			clk_disable(clk->parent);

	}
	return;
}
EXPORT_SYMBOL(clk_disable);

int clk_register(struct clk *clk)
{
	if (!clk || !clk->name)
		return -EINVAL;

	mutex_lock(&clks_list_sem);

	list_add_tail(&clk->node, &clks_list);
	INIT_LIST_HEAD(&clk->children);
	spin_lock_init(&clk->lock);

	clk->usage_counter = 0;

	__clk_init(clk);

	if (clk->parent)
		list_add_tail(&clk->children_node, &clk->parent->children);

	kref_init(&clk->kref);

	mutex_unlock(&clks_list_sem);

	if (clk_is_always_enabled(clk))
		__clk_enable(clk);

	return 0;
}
EXPORT_SYMBOL(clk_register);

void clk_unregister(struct clk *clk)
{
	if (!clk)
		return;
	mutex_lock(&clks_list_sem);
	list_del(&clk->node);
	if (clk->parent)
		list_del(&clk->children_node);
	mutex_unlock(&clks_list_sem);
}
EXPORT_SYMBOL(clk_unregister);

unsigned long clk_get_rate(struct clk *clk)
{
	if (!clk)
		return -EINVAL;
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EINVAL;

	if (!clk)
		return ret;

	if (rate == clk_get_rate(clk))
		return 0;

	ret = __clk_set_rate(clk, rate);

	if (clk_wants_propagate(clk) && !ret)
		clk_propagate_rate(clk);
	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	unsigned long ret = clk_get_rate(clk);

	if (likely(clk->ops && clk->ops->round_rate))
		ret = clk->ops->round_rate(clk, rate);
	return ret;
}
EXPORT_SYMBOL(clk_round_rate);

struct clk *clk_get_parent(struct clk *clk)
{
	if (!clk)
		return NULL;
	return clk->parent;
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = -EINVAL;
	struct clk *old_parent;
	unsigned long old_rate;

	if (!parent || !clk)
		return ret;

	if (parent == clk_get_parent(clk))
		return 0;

	old_parent = clk_get_parent(clk);
	old_rate = clk_get_rate(clk);

	if (old_rate)
		/* enable the new parent if required */
		clk_enable(parent);

	ret = __clk_set_parent(clk, parent);

	/* update the parent field */
	clk->parent = (ret ? old_parent : parent);

	if (old_rate)
		/* notify to the parent the 'disable' clock */
		clk_disable(ret ? parent : old_parent);

	/* propagate if required */
	if (!ret && likely(clk_wants_propagate(clk)))
		clk_propagate_rate(clk);

	return ret;
}
EXPORT_SYMBOL(clk_set_parent);

int clk_observe(struct clk *clk, unsigned long *div)
{
	int ret = -EINVAL;
	if (!clk)
		return ret;
	return __clk_observe(clk, div);
}
EXPORT_SYMBOL(clk_observe);

unsigned long clk_get_measure(struct clk *clk)
{
	if (!clk)
		return 0;

	return __clk_get_measure(clk);
}
EXPORT_SYMBOL(clk_get_measure);

/*
 * Returns a clock.
 */
struct clk *clk_get(struct device *dev, const char *name)
{
	struct clk *clkp, *clk = NULL;

	mutex_lock(&clks_list_sem);

	list_for_each_entry(clkp, &clks_list, node) {
		if (strcmp(name, clkp->name) == 0 &&
		    try_module_get(clkp->owner)) {
			clk = clkp;
			break;
		}
	}

	mutex_unlock(&clks_list_sem);

	return clk;
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	if (clk && !IS_ERR(clk))
		module_put(clk->owner);
}
EXPORT_SYMBOL(clk_put);

int clk_for_each(int (*fn)(struct clk *clk, void *data), void *data)
{
	struct clk *clkp;
	int result = 0;

	if (!fn)
		return -EINVAL;

	mutex_lock(&clks_list_sem);
	list_for_each_entry(clkp, &clks_list, node)
		result |= fn(clkp, data);
	mutex_unlock(&clks_list_sem);
	return result;
}
EXPORT_SYMBOL(clk_for_each);

int clk_for_each_child(struct clk *clk, int (*fn)(struct clk *clk, void *data),
		void *data)
{
	int ret = 0;
	mutex_lock(&clks_list_sem);
	ret = __clk_for_each_child(clk, fn, data);
	mutex_unlock(&clks_list_sem);
	return ret;
}
EXPORT_SYMBOL(clk_for_each_child);


int __weak plat_clk_init(void)
{
	return 0;
}


int __weak arch_clk_init(void)
{
	return 0;
}


int __init clk_init(void)
{
	int ret;
/*
 * the plat_clk_init registers the clocks the chip has
 */
	ret = plat_clk_init();
	if (ret) {
		pr_err("[STM][CLK]: Error on plat_clk_init()\n");
		return ret;
	}
/* the arch_clk_init registers the virtual clocks
 * the architecture needs
 */
	ret = arch_clk_init();
	if (ret) {
		pr_err("[STM][CLK]: Error on arch_clk_init()\n");
		return ret;
	}
	return ret;

}

#ifdef CONFIG_PROC_FS
static void *clk_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return seq_list_next(v, &clks_list, pos);
}

static void *clk_seq_start(struct seq_file *s, loff_t *pos)
{
	return seq_list_start(&clks_list, *pos);
}

static int clk_seq_show(struct seq_file *s, void *v)
{
	struct clk *clk = list_entry(v, struct clk, node);
	unsigned long rate = clk_get_rate(clk);

	if (unlikely(!rate && !clk->parent))
		return 0;
	seq_printf(s, "%-12s\t: %ld.%02ldMHz - ", clk->name,
		rate / 1000000, (rate % 1000000) / 10000);
	seq_printf(s, "[%ld.%02ldMHz] - ", clk->nominal_rate / 1000000,
		(clk->nominal_rate % 1000000) / 10000);
	seq_printf(s, "[0x%p]", clk);
	if (clk_get_rate(clk))
		seq_printf(s, " - enabled");

	if (clk->parent)
		seq_printf(s, " - [%s]", clk->parent->name);
	seq_printf(s, "\n");
	return 0;
}

static void clk_seq_stop(struct seq_file *s, void *v)
{
}

static const struct seq_operations clk_seq_ops = {
	.start = clk_seq_start,
	.next = clk_seq_next,
	.stop = clk_seq_stop,
	.show = clk_seq_show,
};

static int clk_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &clk_seq_ops);
}

static struct file_operations clk_proc_ops = {
	.owner = THIS_MODULE,
	.open = clk_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int __init clk_proc_init(void)
{
	struct proc_dir_entry *p;

	p = create_proc_entry("clocks", S_IRUGO, NULL);

	if (unlikely(!p))
		return -EINVAL;

	p->proc_fops = &clk_proc_ops;

	return 0;
}
module_init(clk_proc_init);
#endif

#ifdef CONFIG_HIBERNATION
static int clk_resume_from_hibernation(struct clk *clk, void *data)
{
	unsigned long rate = clk->rate;

	__clk_set_parent(clk, clk->parent);
	__clk_set_rate(clk, rate);
	__clk_recalc_rate(clk);
	return 0;
}

static int clks_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;

	if (state.event == PM_EVENT_ON && prev_state.event == PM_EVENT_FREEZE)
		/* Resumeing from hibernation */
		clk_for_each(clk_resume_from_hibernation, NULL);

	prev_state = state;
	return 0;
}

static int clks_sysdev_resume(struct sys_device *dev)
{
	return clks_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_class clk_sysdev_class = {
	.name = "clks",
	.suspend = clks_sysdev_suspend,
	.resume = clks_sysdev_resume,
};

static struct sys_device clks_sysdev_dev = {
	.id = 0,
	.cls = &clk_sysdev_class,
};

static int __init clk_sysdev_init(void)
{
	int ret;
	ret = sysdev_class_register(&clk_sysdev_class);
	if (ret)
		return ret;

	ret = sysdev_register(&clks_sysdev_dev);
	if (ret)
		return ret;

	return 0;
};

module_init(clk_sysdev_init);

#endif
