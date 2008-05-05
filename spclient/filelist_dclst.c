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
 * $Id: filelist_dclst.c,v 1.2 2006/04/21 09:30:40 mhe Exp $
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "filelist.h"
#include "iconv_string.h"
#include "xstr.h"
#include "log.h"

static int fl_indentation(const char *line)
{
    int i = 0;
    while(line[i] == '\t')
        ++i;
    return i;
}

static char *fl_dclst_read_line(FILE *fp)
{
    char buf[1024];
    char *e = fgets(buf, sizeof(buf), fp);
    if(e == NULL)
        return NULL;
    char *line = iconv_string_lossy(buf, -1, "WINDOWS-1252", "UTF-8");
    return line;
}

static fl_dir_t *fl_parse_dclst_recursive(FILE *fp, char **saved,
        int level, char *path)
{
    fl_dir_t *dir = calloc(1, sizeof(fl_dir_t));
    dir->path = strdup(path ? path : "");

    while(1)
    {
        char *line = 0;
        if(saved && *saved)
        {
            line = *saved;
            if(saved)
                *saved = NULL;
        }
        else
        {
            line = fl_dclst_read_line(fp);
            if(line == NULL)
            {
                break;
            }
        }
        str_trim_end_inplace(line, NULL);

        int tabs = fl_indentation(line);
        if(tabs < level)
        {
            if(saved)
                *saved = line;
            break;
        }

        char *pipe = strchr(line + tabs, '|');
        if(pipe)
            *pipe = 0;

        char *filename = line + tabs;

        fl_file_t *file = calloc(1, sizeof(fl_file_t));
        file->name = xstrdup(filename);

        if(pipe)
        {
            /* regular file */
            file->type = share_filetype(filename);
            file->size = strtoull(pipe + 1, NULL, 10);
            dir->size += file->size;
        }
        else
        {
            /* directory */
            file->type = SHARE_TYPE_DIRECTORY;

            char *newpath;
            if(path)
                asprintf(&newpath, "%s\\%s", path, filename);
            else
                newpath = xstrdup(filename);

            file->dir = fl_parse_dclst_recursive(fp, saved, level + 1, newpath);
            free(newpath);
            dir->nfiles += file->dir->nfiles;
            dir->size += file->dir->size;
        }

        dir->nfiles++;
        LIST_INSERT_HEAD(&dir->files, file, link);

        free(line);
    }

    return dir;
}

fl_dir_t *fl_parse_dclst(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if(fp == NULL)
    {
        g_message("failed to open %s: %s", filename, strerror(errno));
        return NULL;
    }
    char *saved = 0;
    fl_dir_t *root = fl_parse_dclst_recursive(fp, &saved, 0, NULL);
    fclose(fp);

    return root;
}

#ifdef TEST

#include "unit_test.h"

int main(void)
{
    fl_dir_t *fl = fl_parse_dclst("fl_test2.DcLst");
    fail_unless(fl);
    fail_unless(fl->nfiles == 36);
    fail_unless(fl->size == 611569);
    fl_dir_t *root = fl_find_directory(fl, "spclient\\CVS");
    fail_unless(root);
    fail_unless(root->nfiles == 3);
    fl_free_dir(fl);

    return 0;
}

#endif

