/*
 * Copyright 2005-2006 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: filelist.c,v 1.31 2006/04/09 12:54:31 mhe Exp $
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "filelist.h"

fl_dir_t *fl_parse(const char *filename)
{
    const char *slash = strrchr(filename, '/');
    if(slash++ == 0)
        slash = filename;
    if(strncmp(slash, "MyList", 6) == 0)
        return fl_parse_dclst(filename);
    else
        return fl_parse_xml(filename);
}

#if 0
void fl_sort_recursive(fl_dir_t *dir)
{
    dir->files = g_list_sort(dir->files, fl_sort_func);
    GList *f_iter = dir->files;
    while(f_iter)
    {
        fl_file_t *f = f_iter->data;
        if(f->dir)
            fl_sort_recursive(f->dir);
        f_iter = g_list_next(f_iter);
    }
}
#endif

static void fl_free_file(fl_file_t *flf)
{
    if(flf)
    {
        free(flf->name);
        free(flf->tth);
        if(flf->dir)
            fl_free_dir(flf->dir);
        free(flf);
    }
}

void fl_free_dir(fl_dir_t *dir)
{
    if(dir)
    {
        fl_file_t *f, *next;
        for(f = LIST_FIRST(&dir->files); f; f = next)
        {
            next = LIST_NEXT(f, link);
            LIST_REMOVE(f, link);
            fl_free_file(f);
        }
        free(dir->path);
        free(dir);
    }
}

fl_dir_t *fl_find_directory(fl_dir_t *root, const char *directory)
{
    assert(root);
    assert(root->path);

    if(strcmp(root->path, directory) == 0)
        return root;

    fl_file_t *file;
    LIST_FOREACH(file, &root->files, link)
    {
        if(file->dir &&
           strncmp(file->dir->path, directory, strlen(file->dir->path)) == 0)
        {
            if(strcmp(file->dir->path, directory) == 0)
                return file->dir;
            fl_dir_t *found_dir = fl_find_directory(file->dir, directory);
            if(found_dir)
                return found_dir;
            /* else keep looking */
        }
    }

    return NULL;
}

