#define DEBUG

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/module.h>



/* Internal memory allocation (may be used very early, when
 * no kmalloc() is possible yet...) */

#define STM_PAD_STATIC_BUFFER_SIZE 1024

static unsigned char stm_pad_static_buffer[STM_PAD_STATIC_BUFFER_SIZE];
static unsigned char *stm_pad_static_buffer_pointer = stm_pad_static_buffer;
static int stm_pad_static_buffer_avail = sizeof(stm_pad_static_buffer);

static void *stm_pad_alloc(int size)
{
	void *result = NULL;

	size = ALIGN(size, 4);

	if (stm_pad_static_buffer_avail >= size) {
		result = stm_pad_static_buffer_pointer;
		stm_pad_static_buffer_avail -= size;
		stm_pad_static_buffer_pointer += size;
	} else {
		static int notified;

		if (!notified) {
			pr_debug("stm_pad: Out of static buffer!\n");
			notified = 1;
		}

		result = kzalloc(size, GFP_KERNEL);
	}

	return result;
}

static void stm_pad_free(void *addr)
{
	if (addr > (void *)stm_pad_static_buffer &&
			addr < (void *)(stm_pad_static_buffer +
			STM_PAD_STATIC_BUFFER_SIZE))
		return;

	kfree(addr);
}



/* Pads list implementation */

/* TODO: tree structure to speed up search (and search results reuse) */

struct stm_pad_list {
	struct list_head list;
	const char *owner;
	char path[1]; /* Keep it last, expanded during allocation */
};

static LIST_HEAD(stm_pad_list_root);
static DEFINE_MUTEX(stm_pad_list_mutex);

static struct stm_pad_list *stm_pad_list_find(const char *path)
{
	struct stm_pad_list *pad;

	list_for_each_entry(pad, &stm_pad_list_root, list)
		if (strcmp(pad->path, path) == 0)
			return pad;

	return NULL;
}

static int stm_pad_list_new(char *path, void *context)
{
	struct stm_pad_list *pad;
	const char *owner = context;

	BUG_ON(!path);
	BUG_ON(!context);

	pad = stm_pad_list_find(path);
	if (pad) {
		pr_err("stm_pad: pad '%s' claimed by '%s' already used by "
				"'%s'!\n", path, owner, pad->owner);
		return -EBUSY;
	}

	pad = stm_pad_alloc(sizeof(*pad) + strlen(path));
	if (!pad)
		return -ENOMEM;
	pad->owner = owner;
	strcpy(pad->path, path);

	pr_debug("stm_pad: pad '%s' claimed by '%s'\n", path, owner);

	list_add(&pad->list, &stm_pad_list_root);

	return 0;
}

static int stm_pad_list_delete(char *path, void *context)
{
	struct stm_pad_list *pad;
	const char **owner = context;

	BUG_ON(!path);

	pad = stm_pad_list_find(path);
	WARN_ON(!pad);

	if (pad) {
		if (owner)
			*owner = pad->owner;
		list_del(&pad->list);
		stm_pad_free(pad);
	}

	return 0;
}

static int stm_pad_list_for_each_suffix_range(struct stm_pad_label *label,
		int (*callback_do)(char *path, void *context),
		int (*callback_undo)(char *path, void *context),
		void *context)
{
	int result = -EINVAL;
	char path[STM_PAD_LABEL_LEN];
	int suffix;

	BUG_ON(!label);
	BUG_ON(label->suffix_type != stm_pad_label_suffix_range);
	BUG_ON(!callback_do);

	for (suffix = label->suffix.range.from;
			suffix <= label->suffix.range.to;
			suffix++) {
		snprintf(path, STM_PAD_LABEL_LEN, "%s.%d",
				label->prefix, suffix);
		result = callback_do(path, context);
		if (result != 0 && callback_undo) {
			while (--suffix >= label->suffix.range.from) {
				snprintf(path, STM_PAD_LABEL_LEN, "%s.%d",
						label->prefix, suffix);
				callback_undo(path, context);
			}
			break;
		}
	}

	return result;
}

static int stm_pad_list_for_each_suffix_list(struct stm_pad_label *label,
		int (*callback_do)(char *path, void *context),
		int (*callback_undo)(char *path, void *context),
		void *context)
{
	int result = -EINVAL;
	char path[STM_PAD_LABEL_LEN];
	int *suffix;

	BUG_ON(!label);
	BUG_ON(label->suffix_type != stm_pad_label_suffix_list);
	BUG_ON(!callback_do);

	suffix = label->suffix.list;
	BUG_ON(!suffix);
	while (*suffix != STM_PAD_LABEL_LIST_LAST) {
		snprintf(path, STM_PAD_LABEL_LEN, "%s.%d",
				label->prefix, *suffix);
		result = callback_do(path, context);
		if (result != 0 && callback_undo) {
			while (--suffix >= label->suffix.list) {
				snprintf(path, STM_PAD_LABEL_LEN, "%s.%d",
						label->prefix, *suffix);
				callback_undo(path, context);
			}
			break;
		}
		suffix++;
	}

	return result;
}

static int stm_pad_list_for_each_suffix_strings(struct stm_pad_label *label,
		int (*callback_do)(char *path, void *context),
		int (*callback_undo)(char *path, void *context),
		void *context)
{
	int result = -EINVAL;
	char path[STM_PAD_LABEL_LEN];
	char **suffix;

	BUG_ON(!label);
	BUG_ON(label->suffix_type != stm_pad_label_suffix_strings);
	BUG_ON(!callback_do);

	suffix = label->suffix.strings;
	BUG_ON(!suffix);
	while (*suffix != STM_PAD_LABEL_STRINGS_LAST) {
		snprintf(path, STM_PAD_LABEL_LEN, "%s.%s",
				label->prefix, *suffix);
		result = callback_do(path, context);
		if (result != 0) {
			while (--suffix >= label->suffix.strings) {
				snprintf(path, STM_PAD_LABEL_LEN, "%s.%s",
						label->prefix, *suffix);
				callback_undo(path, context);
			}
			break;
		}
		suffix++;
	}

	return result;
}

static int stm_pad_list_for_each_suffix(struct stm_pad_label *label,
		int (*callback_do)(char *path, void *context),
		int (*callback_undo)(char *path, void *context),
		void *context)
{
	int result = -EINVAL;

	BUG_ON(!label);

	switch (label->suffix_type) {
	case stm_pad_label_suffix_none:
		{
			char path[STM_PAD_LABEL_LEN];

			strlcpy(path, label->prefix, STM_PAD_LABEL_LEN);
			result = callback_do(path, context);
		}
		break;
	case stm_pad_label_suffix_number:
		{
			char path[STM_PAD_LABEL_LEN];

			snprintf(path, STM_PAD_LABEL_LEN, "%s.%d",
					label->prefix, label->suffix.number);
			result = callback_do(path, context);
		}
		break;
	case stm_pad_label_suffix_range:
		result = stm_pad_list_for_each_suffix_range(label,
				callback_do, callback_undo, context);
		break;
	case stm_pad_label_suffix_list:
		result = stm_pad_list_for_each_suffix_list(label,
				callback_do, callback_undo, context);
		break;
	case stm_pad_label_suffix_strings:
		result = stm_pad_list_for_each_suffix_strings(label,
				callback_do, callback_undo, context);
		break;
	default:
		BUG();
		break;
	}

	return result;
}

static int stm_pad_list_claim(struct stm_pad_config *config, const char *owner)
{
	int i;

	BUG_ON(!config);

	for (i = 0; i < config->labels_num; i++) {
		if (stm_pad_list_for_each_suffix(&config->labels[i],
				stm_pad_list_new, stm_pad_list_delete,
				(void *)owner) != 0) {
			while (--i >= 0)
				stm_pad_list_for_each_suffix(&config->labels[i],
						stm_pad_list_delete, NULL,
						NULL);
			return -EBUSY;
		}
	}

	return 0;
}

static const char *stm_pad_list_release(struct stm_pad_config *config)
{
	const char *result = NULL;
	int i;

	BUG_ON(!config);
	WARN_ON(config->labels_num == 0);

	for (i = 0; i < config->labels_num; i++)
		stm_pad_list_for_each_suffix(&config->labels[i],
				stm_pad_list_delete, NULL, &result);

	return result;
}



/* /proc/pads view of used pads list implementation */

#ifdef CONFIG_PROC_FS

static void *stm_pad_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos)
		return NULL;

	seq_printf(s, "# pad used_by\n");

	return pos;
}

static void stm_pad_seq_stop(struct seq_file *s, void *v)
{
}

static void *stm_pad_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static int stm_pad_seq_show(struct seq_file *s, void *v)
{
	struct stm_pad_list *pad;

	mutex_lock(&stm_pad_list_mutex);

	list_for_each_entry(pad, &stm_pad_list_root, list)
		seq_printf(s, "%s %s\n", pad->path, pad->owner);

	mutex_unlock(&stm_pad_list_mutex);

	return 0;
}

static struct seq_operations stm_pad_seq_ops = {
	.start = stm_pad_seq_start,
	.next = stm_pad_seq_next,
	.stop = stm_pad_seq_stop,
	.show = stm_pad_seq_show,
};

static int stm_pad_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &stm_pad_seq_ops);
}

static struct file_operations stm_pad_proc_ops = {
	.owner = THIS_MODULE,
	.open = stm_pad_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available. */
static int __init stm_pad_proc_init(void)
{
	struct proc_dir_entry *entry = create_proc_entry("pads", 0, NULL);

	if (entry)
		entry->proc_fops = &stm_pad_proc_ops;

	return 0;
}
__initcall(stm_pad_proc_init);

#endif /* CONFIG_PROC_FS */



/* Pads interface implementation */

static int stm_pad_exec_config(struct stm_pad_config *config,
		const char *owner)
{
	int i;

	/* sysconf bits... */

	for (i = 0; i < config->sysconf_values_num; i++) {
		struct stm_pad_sysconf_value *value =
				&config->sysconf_values[i];
		struct sysconf_field *field = sysconf_claim(value->regtype,
				value->regnum, value->lsb, value->msb,
				owner);

		/* TODO: sysconf_release() */
		if (!field)
			return -EBUSY;

		sysconf_write(field, value->value);
	}

	/* gpio bits... */

	for (i = 0; i < config->gpio_values_num; i++) {
		struct stm_pad_gpio_value *value = &config->gpio_values[i];

		if (stm_gpio_direction(value->gpio, value->direction) != 0)
			return -EINVAL;
	}

	/* Custom callback */

	if (config->custom_claim) {
		int err = config->custom_claim(config->custom_priv);

		if (err != 0)
			return err;
	}

	return 0;
}

int stm_pad_claim(struct stm_pad_config *config, const char *dev_name)
{
	int result = 0;

	BUG_ON(!config);

	mutex_lock(&stm_pad_list_mutex);
	result = stm_pad_list_claim(config, dev_name);
	mutex_unlock(&stm_pad_list_mutex);

	if (result == 0) {
		result = stm_pad_exec_config(config, dev_name);
		if (result != 0) {
			mutex_lock(&stm_pad_list_mutex);
			stm_pad_list_release(config);
			mutex_unlock(&stm_pad_list_mutex);
		}
	}

	return result;
}
EXPORT_SYMBOL(stm_pad_claim);

int stm_pad_switch(struct stm_pad_config *old_config,
		struct stm_pad_config *new_config, const char *new_dev_name)
{
	const char *old_dev_name;

	BUG_ON(!old_config);
	BUG_ON(!new_config);

	mutex_lock(&stm_pad_list_mutex);

	old_dev_name = stm_pad_list_release(old_config);

	if (stm_pad_list_claim(new_config, new_dev_name) != 0) {
		if (stm_pad_list_claim(old_config, old_dev_name) != 0)
			BUG();
		mutex_unlock(&stm_pad_list_mutex);
	} else {
		mutex_unlock(&stm_pad_list_mutex);
		if (stm_pad_exec_config(new_config, new_dev_name) != 0) {
			mutex_lock(&stm_pad_list_mutex);
			stm_pad_list_release(new_config);
			if (stm_pad_list_claim(old_config, old_dev_name) != 0)
				BUG();
			else if (stm_pad_exec_config(old_config,
						old_dev_name) != 0)
				BUG();
			mutex_unlock(&stm_pad_list_mutex);
		}
	}

	return 0;
}

void stm_pad_release(struct stm_pad_config *config)
{
	BUG_ON(!config);

	mutex_lock(&stm_pad_list_mutex);

	stm_pad_list_release(config);

	mutex_unlock(&stm_pad_list_mutex);
}
EXPORT_SYMBOL(stm_pad_release);

const char *stm_pad_owner(const char *label)
{
	const char *result = NULL;
	struct stm_pad_list *pad;

	BUG_ON(!label);

	mutex_lock(&stm_pad_list_mutex);

	pad = stm_pad_list_find(label);
	if (pad)
		result = pad->owner;

	mutex_unlock(&stm_pad_list_mutex);

	return result;
}



/* Dynamic configuration routines */

#define to_dynamic(config) \
		container_of(config, struct stm_pad_config_dynamic, config)

struct stm_pad_config_dynamic {
	int labels_allocated;
	char *labels_buffer;
	int sysconf_values_allocated;
	int gpio_values_allocated;
#ifdef CONFIG_BUG
	enum { magic_good = 0x600df00d, magic_bad = 0xdeadbeef } magic;
#endif
	struct stm_pad_config config;
};

struct stm_pad_config * __init stm_pad_config_alloc(int min_labels_num,
		int min_sysconf_values_num, int min_gpio_values_num)
{
	struct stm_pad_config *config;
	struct stm_pad_config_dynamic *dynamic;

	dynamic = stm_pad_alloc(sizeof(struct stm_pad_config_dynamic) +
			(sizeof(struct stm_pad_label) + STM_PAD_LABEL_LEN) *
			min_labels_num +
			sizeof(struct stm_pad_sysconf_value) *
			min_sysconf_values_num +
			sizeof(struct stm_pad_gpio_value) *
			min_gpio_values_num);

	if (!dynamic)
		return NULL;

	/* +----------------+
	 * | dynamic        | sizeof(struct stm_pad_config_dynamic)
	 * | { config }     |
	 * +----------------+
	 * | labels_buffer  | STM_PAD_LABEL_LEN * min_labels_num
	 * +----------------+
	 * | labels         | sizeof(struct stm_pad_label) * min_labels_num
	 * +----------------+
	 * | sysconf_values | sizeof(struct stm_pad_sysconf_value) *
	 * +----------------+                        min_sysconf_values_num
	 * | gpio_values    | sizeof(struct stm_pad_gpio_value) *
	 * +----------------+                        min_gpio_values_num
	 */

	dynamic->labels_allocated = min_labels_num;
	dynamic->labels_buffer = (void *)dynamic +
			sizeof(struct stm_pad_config_dynamic);
	dynamic->sysconf_values_allocated = min_sysconf_values_num;
	dynamic->gpio_values_allocated = min_gpio_values_num;
#ifdef CONFIG_BUG
	dynamic->magic = magic_good;
#endif

	config = &dynamic->config;
	memset(config, 0, sizeof(*config));

	config->labels = (void *)dynamic->labels_buffer +
			STM_PAD_LABEL_LEN * min_labels_num;
	config->sysconf_values = (void *)config->labels +
			sizeof(struct stm_pad_label) * min_labels_num;
	config->gpio_values = (void *)config->sysconf_values +
			sizeof(struct stm_pad_sysconf_value) *
			min_sysconf_values_num;

	return config;
}

int __init stm_pad_config_add_label(struct stm_pad_config *config,
		const char *prefix)
{
	struct stm_pad_config_dynamic *dynamic;
	struct stm_pad_label *label;
	int label_no;

	BUG_ON(!config);
	BUG_ON(!prefix);
	dynamic = to_dynamic(config);
	BUG_ON(dynamic->magic != magic_good);

	if (config->labels_num >= dynamic->labels_allocated) {
		/* TODO: dynamic extending */
		BUG();
		return -2;
	}

	label_no = config->labels_num++;
	label = config->labels + label_no;

	label->prefix = dynamic->labels_buffer + (STM_PAD_LABEL_LEN * label_no);
	strlcpy((char *)label->prefix, prefix, STM_PAD_LABEL_LEN);

	label->suffix_type = stm_pad_label_suffix_none;

	return 0;
}

int __init stm_pad_config_add_label_number(struct stm_pad_config *config,
		const char *prefix, int number)
{
	struct stm_pad_config_dynamic *dynamic;
	struct stm_pad_label *label;
	int label_no;

	BUG_ON(!config);
	BUG_ON(!prefix);
	dynamic = to_dynamic(config);
	BUG_ON(dynamic->magic != magic_good);

	if (config->labels_num >= dynamic->labels_allocated) {
		/* TODO: dynamic extending */
		BUG();
		return -2;
	}

	label_no = config->labels_num++;
	label = config->labels + label_no;

	label->prefix = dynamic->labels_buffer + (STM_PAD_LABEL_LEN * label_no);
	strlcpy((char *)label->prefix, prefix, STM_PAD_LABEL_LEN);

	label->suffix_type = stm_pad_label_suffix_number;
	label->suffix.number = number;

	return 0;
}

int __init stm_pad_config_add_label_range(struct stm_pad_config *config,
		const char *prefix, int from, int to)
{
	struct stm_pad_config_dynamic *dynamic;
	struct stm_pad_label *label;
	int label_no;

	BUG_ON(!config);
	BUG_ON(!prefix);
	dynamic = to_dynamic(config);
	BUG_ON(dynamic->magic != magic_good);

	if (config->labels_num >= dynamic->labels_allocated) {
		/* TODO: dynamic extending */
		BUG();
		return -2;
	}

	label_no = config->labels_num++;
	label = config->labels + label_no;

	label->prefix = dynamic->labels_buffer + (STM_PAD_LABEL_LEN * label_no);
	strlcpy((char *)label->prefix, prefix, STM_PAD_LABEL_LEN);

	label->suffix_type = stm_pad_label_suffix_range;
	label->suffix.range.from = from;
	label->suffix.range.to = to;

	return 0;
}

int __init stm_pad_config_add_sysconf(struct stm_pad_config *config,
		int regtype, int regnum, int lsb, int msb, int value)
{
	struct stm_pad_config_dynamic *dynamic;
	struct stm_pad_sysconf_value *sysconf_value;

	BUG_ON(!config);
	dynamic = to_dynamic(config);
	BUG_ON(dynamic->magic != magic_good);

	if (config->sysconf_values_num >= dynamic->sysconf_values_allocated) {
		/* TODO: dynamic extending */
		BUG();
		return -2;
	}

	sysconf_value = config->sysconf_values + config->sysconf_values_num++;

	sysconf_value->regtype = regtype;
	sysconf_value->regnum = regnum;
	sysconf_value->lsb = lsb;
	sysconf_value->msb = msb;
	sysconf_value->value = value;

	return 0;
}

int __init stm_pad_config_add_pio(struct stm_pad_config *config,
		int port, int pin, int direction)
{
	struct stm_pad_config_dynamic *dynamic;
	struct stm_pad_gpio_value *gpio_value;

	BUG_ON(!config);
	dynamic = to_dynamic(config);
	BUG_ON(dynamic->magic != magic_good);

	if (config->gpio_values_num >= dynamic->gpio_values_allocated) {
		/* TODO: dynamic extending */
		BUG();
		return -2;
	}

	gpio_value = config->gpio_values + config->gpio_values_num++;

	gpio_value->gpio = stm_gpio(port, pin);
	gpio_value->direction = direction;

	return 0;
}
