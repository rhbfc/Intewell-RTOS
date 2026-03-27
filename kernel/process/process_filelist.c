/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <fs/fs.h>
#include <ttosProcess.h>

static void filelist_destroy (void *obj)
{
    struct filelist *l = (struct filelist *)obj;
    files_releaselist (l);
    free (l);
}

void process_filelist_copy (pcb_t parent, pcb_t child)
{
    struct filelist *l = calloc (1, sizeof (struct filelist));
    child->vfs_list    = process_obj_create (child, l, filelist_destroy);

    /* Initial inode, file, and VFS data structures */
    get_process_vfs_list (child)->fl_rows  = 0;
    get_process_vfs_list (child)->fl_files = NULL;
    files_initlist (get_process_vfs_list (child));
    files_duplist (get_process_vfs_list (parent), get_process_vfs_list (child));
}

void process_filelist_ref (pcb_t parent, pcb_t child)
{
    child->vfs_list = process_obj_ref (child, parent->vfs_list);
}

void process_filelist_create (pcb_t pcb)
{
    struct filelist *l = calloc (1, sizeof (struct filelist));
    pcb->vfs_list      = process_obj_create (pcb, l, filelist_destroy);

    /* Initial inode, file, and VFS data structures */
    get_process_vfs_list (pcb)->fl_rows  = 0;
    get_process_vfs_list (pcb)->fl_files = NULL;
    files_initlist (get_process_vfs_list (pcb));
}

struct file *process_getfile(pcb_t pcb, int fd)
{
    struct filelist *l = pcb_get_files(pcb);
    struct file * filep;
    fs_getfilep_by_list(fd, &filep, l);
    return filep;
}