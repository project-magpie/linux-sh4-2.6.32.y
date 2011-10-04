/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "mmc_ops.h"

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_claim_host(card->host);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_release_host(card->host);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

static int mmc_ext_csd_read(struct seq_file *s, void *data)
{
	struct mmc_card *card = s->private;
	u8 *ext_csd;
	int err;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free;
	}

	/* Get extended CSD */
	mmc_claim_host(card->host);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_release_host(card->host);
	if (err)
		goto out_free;

	/* aligned to the 4.41 */
	seq_printf(s, "s_cmd_set: 0x%02x\n", ext_csd[504]);
	seq_printf(s, "ini_timeout_ap: 0x%02x\n", ext_csd[241]);
	seq_printf(s, "pwr_cl_ddr_52_360 0x%02x\n", ext_csd[239]);
	seq_printf(s, "pwr_cl_ddr_52_195 0x%02x\n", ext_csd[238]);
	seq_printf(s, "min_perf_ddr_w_8_52 0x%02x\n", ext_csd[235]);
	seq_printf(s, "min_perf_ddr_r_8_52 0x%02x\n", ext_csd[234]);
	seq_printf(s, "trim_mult  0x%02x\n", ext_csd[232]);
	seq_printf(s, "sec_feature_support 0x%02x\n", ext_csd[231]);
	seq_printf(s, "sec_erase_mult 0x%02x\n", ext_csd[230]);
	seq_printf(s, "sec_trim_mult  0x%02x\n", ext_csd[229]);
	seq_printf(s, "boot_info 0x%02x\n", ext_csd[228]);
	seq_printf(s, "boot_size_multi 0x%02x\n", ext_csd[226]);
	seq_printf(s, "acc_size 0x%02x\n", ext_csd[225]);
	seq_printf(s, "hc_erase_grp_size 0x%02x\n", ext_csd[224]);
	seq_printf(s, "erase_timeout_mult 0x%02x\n", ext_csd[223]);
	seq_printf(s, "rel_wr_sec_c 0x%02x\n", ext_csd[222]);
	seq_printf(s, "hc_wp_grp_size 0x%02x\n", ext_csd[221]);
	seq_printf(s, "s_c_vcc 0x%02x\n", ext_csd[220]);
	seq_printf(s, "s_c_vccq 0x%02x\n", ext_csd[219]);
	seq_printf(s, "s_a_timeout 0x%02x\n", ext_csd[217]);
	seq_printf(s, "sec_count 0x%02x\n", (ext_csd[215] << 24) |
		      (ext_csd[214] << 16) | (ext_csd[213] << 8)  |
		      ext_csd[212]);
	seq_printf(s, "min_perf_w_8_52 0x%02x\n", ext_csd[210]);
	seq_printf(s, "min_perf_r_8_52 0x%02x\n", ext_csd[209]);
	seq_printf(s, "min_perf_w_8_26_4_52 0x%02x\n", ext_csd[208]);
	seq_printf(s, "min_perf_r_8_26_4_52 0x%02x\n", ext_csd[207]);
	seq_printf(s, "min_perf_w_4_26 0x%02x\n", ext_csd[206]);
	seq_printf(s, "min_perf_r_4_26 0x%02x\n", ext_csd[205]);
	seq_printf(s, "pwr_cl_26_360 0x%02x\n", ext_csd[203]);
	seq_printf(s, "pwr_cl_52_360 0x%02x\n", ext_csd[202]);
	seq_printf(s, "pwr_cl_26_195 0x%02x\n", ext_csd[201]);
	seq_printf(s, "pwr_cl_52_195 0x%02x\n", ext_csd[200]);
	seq_printf(s, "card_type  0x%02x\n", ext_csd[196]);
	seq_printf(s, "csd_structure 0x%02x\n", ext_csd[194]);
	seq_printf(s, "ext_csd_rev 0x%02x\n", ext_csd[192]);
	seq_printf(s, "cmd_set 0x%02x\n", ext_csd[191]);
	seq_printf(s, "cmd_set_rev 0x%02x\n", ext_csd[189]);
	seq_printf(s, "power_class 0x%02x\n", ext_csd[187]);
	seq_printf(s, "hs_timing 0x%02x\n", ext_csd[185]);
	seq_printf(s, "bus_width 0x%02x\n", ext_csd[183]);
	seq_printf(s, "erased_mem_cont 0x%02x\n", ext_csd[181]);
	seq_printf(s, "partition_config 0x%02x\n", ext_csd[179]);
	seq_printf(s, "boot_config_prot 0x%02x\n", ext_csd[178]);
	seq_printf(s, "boot_bus_width 0x%02x\n", ext_csd[177]);
	seq_printf(s, "erase_group_def 0x%02x\n", ext_csd[175]);
	seq_printf(s, "boot_wp 0x%02x\n", ext_csd[173]);
	seq_printf(s, "user_wp 0x%02x\n", ext_csd[171]);
	seq_printf(s, "fw_config 0x%02x\n", ext_csd[169]);
	seq_printf(s, "rpmb_size_mult 0x%02x\n", ext_csd[168]);
	seq_printf(s, "rst_n_function 0x%02x\n", ext_csd[162]);
	seq_printf(s, "partitioning_support 0x%02x\n", ext_csd[160]);
	seq_printf(s, "max_enh_size_mult[2] 0x%02x\n", ext_csd[159]);
	seq_printf(s, "max_enh_size_mult[1] 0x%02x\n", ext_csd[158]);
	seq_printf(s, "max_enh_size_mult[0] 0x%02x\n", ext_csd[157]);
	seq_printf(s, "partitions_attribute 0x%02x\n", ext_csd[156]);
	seq_printf(s, "partition_setting_completed 0x%02x\n",
		      ext_csd[155]);
	seq_printf(s, "gp_size_mult_4[2] 0x%02x\n", ext_csd[154]);
	seq_printf(s, "gp_size_mult_4[1] 0x%02x\n", ext_csd[153]);
	seq_printf(s, "gp_size_mult_4[0] 0x%02x\n", ext_csd[152]);
	seq_printf(s, "gp_size_mult_3[2] 0x%02x\n", ext_csd[151]);
	seq_printf(s, "gp_size_mult_3[1] 0x%02x\n", ext_csd[150]);
	seq_printf(s, "gp_size_mult_3[0] 0x%02x\n", ext_csd[149]);
	seq_printf(s, "gp_size_mult_2[2] 0x%02x\n", ext_csd[148]);
	seq_printf(s, "gp_size_mult_2[1] 0x%02x\n", ext_csd[147]);
	seq_printf(s, "gp_size_mult_2[0] 0x%02x\n", ext_csd[146]);
	seq_printf(s, "gp_size_mult_1[2] 0x%02x\n", ext_csd[145]);
	seq_printf(s, "gp_size_mult_1[1] 0x%02x\n", ext_csd[144]);
	seq_printf(s, "gp_size_mult_1[0] 0x%02x\n", ext_csd[143]);
	seq_printf(s, "enh_size_mult[2] 0x%02x\n", ext_csd[142]);
	seq_printf(s, "enh_size_mult[1] 0x%02x\n", ext_csd[141]);
	seq_printf(s, "enh_size_mult[0] 0x%02x\n", ext_csd[140]);
	seq_printf(s, "enh_start_addr[3] 0x%02x\n", ext_csd[139]);
	seq_printf(s, "enh_start_addr[2] 0x%02x\n", ext_csd[138]);
	seq_printf(s, "enh_start_addr[1] 0x%02x\n", ext_csd[137]);
	seq_printf(s, "enh_start_addr[0] 0x%02x\n", ext_csd[136]);
	seq_printf(s, "sec_bad_blk_mgmnt 0x%02x\n", ext_csd[134]);

out_free:
	kfree(ext_csd);
	return err;
}

static int mmc_ext_csd_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ext_csd_read, inode->i_private);
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
