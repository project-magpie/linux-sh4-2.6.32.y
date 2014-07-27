/*
 * aotom_proc.c
 *
 * (c) 2014 Christian Ege <k4230r6@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "aotom_main.h"
#include <linux/e2proc.h>
#include <asm/uaccess.h>    	/* copy_from_user */
#include <linux/kernel.h>       /* sscanf */

static int rtc_offset = 0;

static int aotom_read_rtc(char *page, char **start, off_t off, int count,
	int *eof, void *data);
static int aotom_write_rtc(struct file *file, const char __user *buffer,
	unsigned long count, void *data);

static int aotom_read_rtc_offset(char *page, char **start, off_t off, int count,
	int *eof, void *data);
static int aotom_write_rtc_offset(struct file *file, const char __user *buffer,
	unsigned long count, void *data);


typedef struct {
  char *name;
  read_proc_t *read_proc;
  write_proc_t *write_proc;
} fp_procs_t;

static fp_procs_t fp_procs[] =
{
  { "stb/fp/rtc", aotom_read_rtc, aotom_write_rtc },
  { "stb/fp/rtc_offset", aotom_read_rtc_offset, aotom_write_rtc_offset },
};

extern void register_proc_fp_entries(void) {
	int idx = 0;
	for(idx = 0; idx < sizeof(fp_procs)/sizeof(fp_procs_t); idx++) {
		install_e2_procs(fp_procs[idx].name, fp_procs[idx].read_proc, fp_procs[idx].write_proc, NULL);
	}
}

extern void remove_proc_fp_entries(void)
{
	int idx = 0;
	for(idx = 0; idx < sizeof(fp_procs)/sizeof(fp_procs_t); idx++) {
    	remove_e2_procs(fp_procs[idx].name, fp_procs[idx].read_proc,fp_procs[idx].write_proc);
    }
}

 static int aotom_read_rtc(char *page, char **start, off_t off, int count,
 	int *eof, void *data) {
 	int len = 0;
	u32 rtc_time = YWPANEL_FP_GetTime();
	if(NULL != page)
	{
		/* AOTOM needs time in local time so deduct rtc_offset for e2 */
		len = sprintf(page,"%u\n", rtc_time-rtc_offset);
	}
	return len;
 }

 static int aotom_write_rtc(struct file *file, const char __user *buffer,
 	unsigned long count, void *data) {
	char *page = NULL;
	ssize_t ret = -ENOMEM;
	u32 argument = 0;
	int test = -1;
	char *myString = kmalloc(count + 1, GFP_KERNEL);
	printk("%s %ld - ", __FUNCTION__, count);
	page = (char *)__get_free_page(GFP_KERNEL);
	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buffer, count))
			goto out;

		strncpy(myString, page, count);
		myString[count] = '\0';
		printk("%s -> %s\n",__FUNCTION__, myString);
		test = sscanf (myString,"%u",&argument);
		if(0 < test)
		{
			/* AOTOM needs time in local time so add rtc_offset for time from e2 */
			YWPANEL_FP_SetTime(argument+rtc_offset);
			YWPANEL_FP_ControlTimer(true);
		}
		/* always return count to avoid endless loop */
		ret = count;
	}

out:
	free_page((unsigned long)page);
	kfree(myString);
	return ret;
 }

static int aotom_read_rtc_offset(char *page, char **start, off_t off, int count,
 	int *eof, void *data) {
 	int len = 0;
	if(NULL != page)
		len = sprintf(page,"%d\n", rtc_offset);
	return len;
 }

 static int aotom_write_rtc_offset(struct file *file, const char __user *buffer,
 	unsigned long count, void *data) {
	char *page = NULL;
	ssize_t ret = -ENOMEM;
	int test = -1;
	char *myString = kmalloc(count + 1, GFP_KERNEL);
	printk("%s %ld - ", __FUNCTION__, count);
	page = (char *)__get_free_page(GFP_KERNEL);
	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buffer, count))
			goto out;

		strncpy(myString, page, count);
		myString[count] = '\0';
		printk("%s -> %s\n",__FUNCTION__, myString);
		test = sscanf (myString,"%d",&rtc_offset);
		printk(" offset: %d\n",rtc_offset);
		/* always return count to avoid endless loop */
		ret = count;
	}

out:
	free_page((unsigned long)page);
	kfree(myString);
	return ret;
 }
