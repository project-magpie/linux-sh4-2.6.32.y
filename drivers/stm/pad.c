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


struct stm_pad_state {
	struct stm_pad_config *config;
	struct sysconf_field *sysconf_fields[];
};

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
	int ret = 0;

	BUG_ON(!path);
	BUG_ON(!context);

	mutex_lock(&stm_pad_list_mutex);

	pad = stm_pad_list_find(path);
	if (pad) {
		pr_err("stm_pad: pad '%s' claimed by '%s' already used by "
				"'%s'!\n", path, owner, pad->owner);
		ret = -EBUSY;
		goto out;
	}

	pad = stm_pad_alloc(sizeof(*pad) + strlen(path));
	if (!pad) {
		ret = -ENOMEM;
		goto out;
	}

	pad->owner = owner;
	strcpy(pad->path, path);

	pr_debug("stm_pad: pad '%s' claimed by '%s'\n", path, owner);

	list_add(&pad->list, &stm_pad_list_root);

out:
	mutex_unlock(&stm_pad_list_mutex);

	return ret;
}

static int stm_pad_list_delete(char *path, void *context)
{
	struct stm_pad_list *pad;
	const char **owner = context;

	BUG_ON(!path);

	mutex_lock(&stm_pad_list_mutex);

	pad = stm_pad_list_find(path);
	WARN_ON(!pad);

	if (pad) {
		if (owner)
			*owner = pad->owner;
		list_del(&pad->list);
		stm_pad_free(pad);
	}

	mutex_unlock(&stm_pad_list_mutex);

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

static int stm_pad_list_claim(struct stm_pad_state *state, const char *owner)
{
	struct stm_pad_config *config = state->config;
	int pad_num;
	int sysconf_num;
	int gpio_num;

	BUG_ON(!config);

	for (pad_num = 0; pad_num < config->labels_num; pad_num++)
		if (stm_pad_list_for_each_suffix(&config->labels[pad_num],
				stm_pad_list_new, stm_pad_list_delete,
				(void *)owner) != 0)
			goto free_pads;

	for (sysconf_num = 0; sysconf_num < config->sysconf_values_num;
	     sysconf_num++) {
		struct stm_pad_sysconf_value *value =
				&config->sysconf_values[sysconf_num];
		struct sysconf_field *field = sysconf_claim(value->regtype,
				value->regnum, value->lsb, value->msb,
				owner);
		if (!field)
			goto free_sysconf;

		state->sysconf_fields[sysconf_num] = field;
	}

	for (gpio_num = 0; gpio_num < config->gpio_values_num; gpio_num++)
		/* This will result in a recursive call to claim the pad. */
		if (gpio_request(config->gpio_values[gpio_num].gpio, owner))
			goto free_gpio;

	return 0;

free_gpio:
	while (--gpio_num >= 0)
		gpio_free(config->gpio_values[gpio_num].gpio);

free_sysconf:
	while (--sysconf_num >= 0)
		sysconf_release(state->sysconf_fields[sysconf_num]);

free_pads:
	while (--pad_num >= 0)
		stm_pad_list_for_each_suffix(&config->labels[pad_num],
			stm_pad_list_delete, NULL, NULL);

	return -EBUSY;
}

static const char *stm_pad_list_release(struct stm_pad_state *state)
{
	struct stm_pad_config *config = state->config;
	const char *result = NULL;
	int i;

	BUG_ON(!config);

	for (i = 0; i < config->labels_num; i++)
		stm_pad_list_for_each_suffix(&config->labels[i],
				stm_pad_list_delete, NULL, &result);

	for (i = 0; i < config->sysconf_values_num; i++)
		sysconf_release(state->sysconf_fields[i]);

	for (i = 0; i < config->gpio_values_num; i++)
		gpio_free(config->gpio_values[i].gpio);

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

static int stm_pad_exec_config(struct stm_pad_state *state,
		struct stm_pad_config *config)
{
	int i;

	/* sysconf bits... */

	for (i = 0; i < config->sysconf_values_num; i++) {
		struct stm_pad_sysconf_value *value =
				&config->sysconf_values[i];

		sysconf_write(state->sysconf_fields[i], value->value);
	}

	/* gpio bits... */

	for (i = 0; i < config->gpio_values_num; i++) {
		struct stm_pad_gpio_value *value = &config->gpio_values[i];
		int res;

		if (value->direction == -1)
			continue;

		if ((value->direction == STM_GPIO_DIRECTION_OUT) &&
		    (value->value != -1))
			res = gpio_direction_output(value->gpio,
				value->value);
		else {
			res = stm_gpio_direction(value->gpio,
				value->direction);
			if (res == 0)
				/* This will result in a recursive call */
				res = stm_gpio_mux(value->gpio, value->mux);
		}

		if (res != 0)
			return -EINVAL;
	}

	/* Custom callback */

	if (config->custom_claim) {
		int err = config->custom_claim(config, config->custom_priv);

		if (err != 0)
			return err;
	}

	return 0;
}

struct stm_pad_state *stm_pad_claim_exec(struct stm_pad_config *config,
		const char *dev_name, int flag)
{
	int result = 0;
	struct stm_pad_state *state;
	int state_size;

	BUG_ON(!config);

	state_size = sizeof(*state);
	state_size += sizeof(struct sysconf_field *)*config->sysconf_values_num;
	state = stm_pad_alloc(state_size);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->config = config;

	result = stm_pad_list_claim(state, dev_name);
	if (result)
		goto out_free;

	if (flag) {
		result = stm_pad_exec_config(state, config);
		if (result)
			goto out_release;
	}

	return state;

out_release:
	stm_pad_list_release(state);
out_free:
	stm_pad_free(state);

	return ERR_PTR(result);
}
EXPORT_SYMBOL(stm_pad_claim_exec);

struct stm_pad_state *stm_pad_claim(struct stm_pad_config *config,
		const char *dev_name)
{
	return stm_pad_claim_exec(config, dev_name, 1);
}
EXPORT_SYMBOL(stm_pad_claim);

int stm_pad_switch(struct stm_pad_state *state,
		struct stm_pad_config *new_config)
{
	return stm_pad_exec_config(state, new_config);
}
EXPORT_SYMBOL(stm_pad_switch);

void stm_pad_release(struct stm_pad_state *state)
{
	BUG_ON(!state);

	stm_pad_list_release(state);
	stm_pad_free(state);
}
EXPORT_SYMBOL(stm_pad_release);

int stm_pad_gpio(struct stm_pad_config *config, const char* name)
{
	int i;
	int res = -ENODEV;

	mutex_lock(&stm_pad_list_mutex);

	for (i = 0; i < config->gpio_values_num; i++) {
		struct stm_pad_gpio_value *value = &config->gpio_values[i];

		if (value->name && !strcmp(value->name, name)) {
			res = value->gpio;
			break;
		}
	}

	mutex_unlock(&stm_pad_list_mutex);

	return res;
}
EXPORT_SYMBOL(stm_pad_gpio);

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

/*
 * This is a custom version of stm_pad_switch, used by the gpio layer to
 * switch the pad multiplexing and means we only have to store a single
 * int in struct gpio_value rather than an entire struct stm_pad_config.
 */
int stm_pad_mux(struct stm_pad_state *state, struct stm_pad_config *config,
		int mux)
{
	struct stm_pad_config new_config;
	struct stm_pad_sysconf_value new_sysconf_value;

	if (config->sysconf_values_num == 0)
		return 0;

	BUG_ON(config->sysconf_values_num != 1);

	new_sysconf_value = config->sysconf_values[0];
	new_sysconf_value.value = mux;

	memset(&new_config, 0, sizeof(new_config));
	new_config.sysconf_values_num = 1;
	new_config.sysconf_values = &new_sysconf_value;

	return stm_pad_exec_config(state, &new_config);

}

/* Device resource management aware routines */

struct stm_pad_devres {
	struct stm_pad_state *state;
};

static void stm_pad_devres_release(struct device *dev, void *res)
{
	struct stm_pad_devres *this = res;

	stm_pad_release(this->state);
}

static int stm_pad_devres_match(struct device *dev, void *res, void *data)
{
	struct stm_pad_devres *this = res, *match = data;

	return this->state == match->state;
}

struct stm_pad_state *devm_stm_pad_claim(struct device *dev,
	struct stm_pad_config *config, const char *name)
{
	struct stm_pad_devres *dr;
	struct stm_pad_state *state;

	dr = devres_alloc(stm_pad_devres_release, sizeof(struct stm_pad_devres),
			  GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	state = stm_pad_claim(config, name);
	if (IS_ERR(state)) {
		devres_free(dr);
		return state;
	}

	dr->state = state;
	devres_add(dev, dr);

	return state;
}
EXPORT_SYMBOL(devm_stm_pad_claim);

void devm_stm_pad_release(struct device *dev, struct stm_pad_state *state)
{
	struct stm_pad_devres match_data = { state };

	stm_pad_release(state);
	WARN_ON(devres_destroy(dev, stm_pad_devres_release,
			       stm_pad_devres_match, &match_data));
}
EXPORT_SYMBOL(devm_stm_pad_release);



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
