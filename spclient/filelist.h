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
 */

#ifndef _fl_h_
#define _fl_h_

#include "sys_queue.h"

#include <stdio.h>

#include "xml.h"
#include "util.h"

struct fl_file;

typedef struct fl_dir fl_dir_t;
struct fl_dir
{
    LIST_ENTRY(fl_dir) link;

    LIST_HEAD(fl_file_list, fl_file) files;
    char *path;
    unsigned nfiles;
    uint64_t size;
};

typedef struct fl_file fl_file_t;
struct fl_file
{
    LIST_ENTRY(fl_file) link;
    
    char *name;
    share_type_t type;
    uint64_t size;
    char *tth;

    fl_dir_t *dir;
};

fl_dir_t *fl_parse(const char *filename);

typedef void (*fl_xml_file_callback_t)(const char *path, const char *tth,
        uint64_t size, void *user_data);

typedef struct fl_xml_ctx fl_xml_ctx_t;
struct fl_xml_ctx
{
    LIST_HEAD(, fl_dir) dir_stack;
    fl_dir_t *root;
    FILE *fp;
    void *user_data;
    fl_xml_file_callback_t file_callback;
    xml_ctx_t *xml;
};

int fl_parse_xml_chunk(fl_xml_ctx_t *ctx);
fl_xml_ctx_t *fl_xml_prepare_file(const char *filename,
        fl_xml_file_callback_t file_callback, void *user_data);
void fl_xml_free_context(fl_xml_ctx_t *ctx);
fl_dir_t *fl_parse_xml(const char *filename);

fl_dir_t *fl_parse_dclst(const char *filename);
void fl_sort_recursive(fl_dir_t *dir);
void fl_free_dir(fl_dir_t *dir);
fl_dir_t *fl_find_directory(fl_dir_t *root, const char *directory);

#endif

