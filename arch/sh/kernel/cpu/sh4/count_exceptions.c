/*
 * Support functions when CONFIG_COUNT_EXCEPTIONS is enabled.
 *
 * Copyright (C) 2013 STMicroelectronics Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/irq.h>

#define NR_EXCEPTIONS ((0x820/0x20)+1)

unsigned long exception_count_table[NR_EXCEPTIONS];
unsigned long exception_count_table2[NR_EXCEPTIONS];

static char *exception_names[16] = {
	"POR", "Man reset", "TLB miss (r)", "TLB miss (w)",
	"Init page wr", "TLB prot (r)", "TLB prot (w)", "Addr err (r)",
	"Addr err (w)", "FPU", "TLB multi-hit", "TRAPA",
	"Illegal instr", "Slot illegal", "NMI", "User break"
};

static int exceptions_seq_show(struct seq_file *file, void *iter)
{
	int i;
	char buf[10];
	char *name;

	seq_printf(file, "EXPEVT %10s %10s %10s\n", "Total", "asm", "C");
	for (i = 0; i < NR_EXCEPTIONS; i++) {
		if (exception_count_table[i] == 0)
			continue;

		if (i < 16) {
			name = exception_names[i];
		} else if (i == (0x800/32)) {
			name = "FPU disabled";
		} else if (i == (0x820/32)) {
			name = "FPU disabled (slot)";
		} else {
			sprintf(buf, "IRQ %3d", i-16);
			name = buf;
		}

		if (exception_count_table2[i])
			seq_printf(file, "0x%03x: %10lu %10lu %10lu %s\n",
				   i*32,
				   exception_count_table2[i],
				   exception_count_table2[i] -
					exception_count_table[i],
				   exception_count_table[i], name);
		else
			seq_printf(file, "0x%03x: %10lu %10s %10s %s\n",
				   i*32,
				   exception_count_table[i], "", "", name);
	}

	return 0;
}

static int exceptions_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, exceptions_seq_show, inode->i_private);
}

static const struct file_operations exceptions_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= exceptions_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init exceptions_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("exceptions", S_IRUSR, sh_debugfs_root,
				     NULL, &exceptions_debugfs_fops);
	if (!dentry)
		return -ENOMEM;

	return IS_ERR(dentry) ? PTR_ERR(dentry) : 0;
}
module_init(exceptions_debugfs_init);
