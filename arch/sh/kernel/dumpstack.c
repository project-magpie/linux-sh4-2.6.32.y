/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 *  Copyright (C) 2009  Matt Fleming
 */
#include <linux/kallsyms.h>
#include <linux/ftrace.h>
#include <linux/debug_locks.h>
#include <asm/unwinder.h>
#include <asm/stacktrace.h>

void printk_address(unsigned long address, int reliable)
{
	printk(" [<%p>] %s%pS\n", (void *) address,
			reliable ? "" : "? ", (void *) address);
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
static void
print_ftrace_graph_addr(unsigned long addr, void *data,
			const struct stacktrace_ops *ops,
			struct thread_info *tinfo, int *graph)
{
	struct task_struct *task = tinfo->task;
	unsigned long ret_addr;
	int index = task->curr_ret_stack;

	if (addr != (unsigned long)return_to_handler)
		return;

	if (!task->ret_stack || index < *graph)
		return;

	index -= *graph;
	ret_addr = task->ret_stack[index].ret;

	ops->address(data, ret_addr, 1);

	(*graph)++;
}
#else
static inline void
print_ftrace_graph_addr(unsigned long addr, void *data,
			const struct stacktrace_ops *ops,
			struct thread_info *tinfo, int *graph)
{ }
#endif

#ifdef CONFIG_FRAME_POINTER
enum { FOUND_NONE, FOUND_PR, FOUND_FP };

static int is_on_stack(struct task_struct *task, unsigned long *ptr)
{
	unsigned long *stack;
	if (!task)
		task = current;
	stack = task_stack_page(task);
	return ((ptr >= stack) && (ptr < (stack + THREAD_SIZE)));
}

static int find_pr(struct task_struct *task,
		unsigned long addr, unsigned long *sp,
		unsigned long *fp, unsigned long **r_val)
{
	unsigned long size, off, arg, i, *ptr, r1;
	int found, have_pr, have_fp, valid_fp;
	uint8_t *opc;
	*r_val = NULL;
	if (!addr || !fp)
		return FOUND_NONE;
	found = kallsyms_lookup_size_offset(addr, &size, &off);
	if (!found)
		return FOUND_NONE;
	opc = (int8_t *)(addr - off);
	arg = 0;
	have_pr = 0;
	have_fp = 0;
	valid_fp = 0;
	r1 = 0;
	for (i = 0; i < size - 1; i += 2) {
		/* off==0 means not from exception, aka dump_stack() */
		if (off && i >= off)
			break;
		/* look for 'mov.l r14,@-r15' */
		if (opc[i] == 0xe6 && opc[i + 1] == 0x2f)
			have_fp = 1;
		/* look for 'add #-X,r15' */
		if (opc[i + 1] == 0x7f && (opc[i] & 0x80) && !(opc[i] & 3))
			arg += (-(int8_t)opc[i]) / sizeof(long);
		/* look for 'sts.l pr,@-r15' */
		if (opc[i] == 0x22 && opc[i + 1] == 0x4f) {
			have_pr = 1;
			arg = 0;
		}
		/* look for 'mov.w XX,r1' */
		if (opc[i + 1] == 0x91) {
			uint16_t r1_off = i + opc[i] * 2 + 4;
			if (r1_off < size - 1)
				r1 = opc[r1_off] | (opc[r1_off + 1] << 8);
		}
		/* look for 'sub r1,r15' */
		if (opc[i] == 0x18 && opc[i + 1] == 0x3f)
			arg += r1 / sizeof(long);
		/* look for 'mov r15,r14' */
		if (opc[i] == 0xf3 && opc[i + 1] == 0x6e) {
			valid_fp = 1;
			break;
		}
	}
	if (!have_pr && !have_fp)
		return FOUND_NONE;
	if (valid_fp)
		ptr = fp + arg;
	else
		ptr = sp + arg;
	if (!is_on_stack(task, ptr))
		return FOUND_NONE;
	if (have_pr) {
		/* pr points to kernel code */
		if (!__kernel_text_address(*ptr))
			return FOUND_NONE;
	} else {
		/* fp points to stack */
		if (!is_on_stack(task, *(unsigned long **)ptr))
			return FOUND_NONE;
	}
	*r_val = ptr;
	if (have_pr)
		return FOUND_PR;
	return FOUND_FP;
}
#endif

void
stack_reader_dump(struct task_struct *task, struct pt_regs *regs,
		  unsigned long *sp, unsigned long *fp,
		  unsigned long faddr,
		  const struct stacktrace_ops *ops, void *data)
{
	struct thread_info *context;
	int graph = 0;
#ifdef CONFIG_FRAME_POINTER
	unsigned long regs_pr = regs ? regs->pr : 0;
	unsigned long *pr;
	int f_pr;
	f_pr = find_pr(task, faddr, sp, fp, &pr);
	if (f_pr != FOUND_PR && regs_pr) {
		/* from exception */
		ops->address(data, regs_pr, 1);
		if (f_pr == FOUND_FP) {
			fp = (unsigned long *)pr[0];
			f_pr = find_pr(task, regs_pr, sp, fp, &pr);
		}
	}
#endif

	context = (struct thread_info *)
		((unsigned long)sp & (~(THREAD_SIZE - 1)));

	while (!kstack_end(sp)) {
		unsigned long addr = sp[0];

		if (__kernel_text_address(addr)) {
#ifdef CONFIG_FRAME_POINTER
			if (!pr) {
				ops->address(data, addr, 0);
			} else if (sp == pr) {
				ops->address(data, addr, 1);
				fp = (unsigned long *)sp[1];
				f_pr = find_pr(task, addr, sp, fp, &pr);
				if (f_pr != FOUND_PR)
					pr = NULL;
			}
#else
			ops->address(data, addr, 1);
#endif

			print_ftrace_graph_addr(addr, data, ops,
						context, &graph);
		}

		sp++;
	}
}

static void
print_trace_warning_symbol(void *data, char *msg, unsigned long symbol)
{
	printk(data);
	print_symbol(msg, symbol);
	printk("\n");
}

static void print_trace_warning(void *data, char *msg)
{
	printk("%s%s\n", (char *)data, msg);
}

static int print_trace_stack(void *data, char *name)
{
	printk("%s <%s> ", (char *)data, name);
	return 0;
}

/*
 * Print one address/symbol entries per line.
 */
static void print_trace_address(void *data, unsigned long addr, int reliable)
{
	printk(data);
	printk_address(addr, reliable);
}

static const struct stacktrace_ops print_trace_ops = {
	.warning = print_trace_warning,
	.warning_symbol = print_trace_warning_symbol,
	.stack = print_trace_stack,
	.address = print_trace_address,
};

void show_trace(struct task_struct *tsk, unsigned long *sp,
		unsigned long *fp, unsigned long faddr, struct pt_regs *regs)
{
	if (regs && user_mode(regs))
		return;

	printk("\nCall trace:\n");

	unwind_stack(tsk, regs, sp, fp, faddr, &print_trace_ops, "");

	printk("\n");

	if (!tsk)
		tsk = current;

	debug_show_held_locks(tsk);
}
