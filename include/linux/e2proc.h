/*
 * e2proc.h
 *
 * (c) 2009 teamducktales
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

#ifndef E2PROC_H__
#define E2PROC_H__

#include <linux/types.h>
 #include <linux/proc_fs.h>

typedef int (*proc_read_t) (char *page, char **start, off_t off, int count,
		  int *eof, void *data_unused);
typedef int (*proc_write_t) (struct file *file, const char __user *buf,
		   unsigned long count, void *data);


#define cProcDir	1
#define cProcEntry	2

struct ProcStructure_s
{
	int   type;
	char* name;
	struct proc_dir_entry* entry;
	proc_read_t read_proc;
	proc_write_t write_proc;
	void* instance; /* needed for cpp stuff */
	void* identifier; /* needed for cpp stuff */
};

extern int install_e2_procs(char *path, read_proc_t *read_func, write_proc_t *write_func, void *data);
extern int remove_e2_procs(char *path, read_proc_t *read_func, write_proc_t *write_func);
extern int cpp_install_e2_procs(const char *path, read_proc_t *read_func, write_proc_t *write_func, void* instance);
extern int cpp_remove_e2_procs(const char *path, read_proc_t *read_func, write_proc_t *write_func);

#endif /* E2PROC_H__ */