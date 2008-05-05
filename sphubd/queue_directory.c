/*
 * Copyright 2004-2006 Martin Hedenfalk <martin@bzero.se>
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

#define _GNU_SOURCE /* needed for asprintf */

#include <stdlib.h>
#include <stdio.h>
#include <db.h>
#include <string.h>

#include "globals.h"
#include "filelist.h"
#include "queue.h"
#include "log.h"
#include "notifications.h"
#include "xstr.h"
#include "dbenv.h"

extern DB *queue_target_db;
extern DB *queue_source_db;
extern DB *queue_directory_db;

/* helper function to walk the filelist tree and add found files to the queue */
static void queue_resolve_directory_recursively(const char *nick,
        fl_dir_t *root,
        const char *directory,
        const char *target_directory,
        unsigned *nfiles_p)
{
    fl_file_t *file;
    LIST_FOREACH(file, &root->files, link)
    {
        char *target;
        asprintf(&target, "%s/%s", directory, file->name);

        if(file->dir)
        {
            queue_resolve_directory_recursively(nick, file->dir,
                    target, target_directory, nfiles_p);
        }
        else
        {
            char *source;
            asprintf(&source, "%s\\%s", root->path, file->name);

            queue_add_internal(nick, source, file->size, target,
                    file->tth, 0, target_directory);

            if(nfiles_p)
                (*nfiles_p)++;
            free(source);
        }
        free(target);
    }
}

/* Resolves all files and subdirectories in a directory download request
 * through the filelist. Adds those resolved files to the download queue.
 *
 * Returns 0 if the directory was directly resolved (filelist already exists)
 * or 1 if the filelist has been queued and another attempt at resolving the
 * directory should be done once the filelist is available.
 *
 * Returns -1 on error.
 */
int queue_resolve_directory(const char *nick,
        const char *source_directory,
        const char *target_directory,
        unsigned *nfiles_p)
{
    char *filelist_path = find_filelist(global_working_directory, nick);

    g_debug("resolving directory [%s] for nick [%s]", source_directory, nick);

    if(filelist_path)
    {
        /* parse the filelist and add all files in the directory to the queue */

        /* FIXME: might need to do this asynchronously */
        /* FIXME: no need to keep the whole filelist in memory (parse
         * incrementally) */
        fl_dir_t *root = fl_parse(filelist_path);
        if(root)
        {
            fl_dir_t *fl = fl_find_directory(root, source_directory);
            if(fl)
            {
                unsigned nfiles = 0;

                queue_resolve_directory_recursively(nick, fl,
                        target_directory, target_directory,
                        &nfiles);

                if(nfiles_p)
                    *nfiles_p = nfiles;

                /* update the resolved flag for this directory */
                queue_directory_t *qd = queue_db_lookup_directory(target_directory);
                if(qd)
                {
                    if((qd->flags & QUEUE_DIRECTORY_RESOLVED) == QUEUE_DIRECTORY_RESOLVED)
                    {
                        g_warning("Directory [%s] already resolved!",
                                target_directory);
                    }
                    else
                    {
                        qd->flags |= QUEUE_DIRECTORY_RESOLVED;
                        qd->nfiles = nfiles;
                        qd->nleft = nfiles;

                        g_debug("Updating directory [%s] with resolved flag",
                                target_directory);
                        queue_db_update_directory(target_directory, qd);
                    }
                }
            }
            else
            {
                g_message("source directory not found, removing from queue");
                queue_remove_directory(target_directory);
            }
        }
        fl_free_dir(root);
        free(filelist_path);
    }
    else
    {
        /* get the filelist first so we can see what files to download */
        queue_add_filelist(nick, 1 /* auto-matched */);
        return 1;
    }

    return 0;
}

int queue_add_directory(const char *nick,
        const char *source_directory,
        const char *target_directory)
{
    return_val_if_fail(nick, -1);
    return_val_if_fail(source_directory, -1);
    return_val_if_fail(target_directory, -1);

    while(*target_directory == '/')
        ++target_directory;

    queue_directory_t qd;
    memset(&qd, 0, sizeof(qd));
    xstrlcpy(qd.target_directory, target_directory, QUEUE_TARGET_MAXPATH);
    xstrlcpy(qd.source_directory, source_directory, QUEUE_TARGET_MAXPATH);
    xstrlcpy(qd.nick, nick, QUEUE_SOURCE_MAXNICK);

    unsigned nfiles = 0;
    int rc = queue_resolve_directory(nick, source_directory, target_directory,
            &nfiles);
    if(rc == 1)
    {
        /* Directory was not directly resolvable, need to download the
         * filelist. First add a directory download request to the queue so we
         * remember what to do with the filelist. */
    }
    else if(rc == 0)
    {
        qd.flags |= QUEUE_DIRECTORY_RESOLVED;
        qd.nfiles = nfiles;
        qd.nleft = nfiles;
    }

    if(rc != -1)
    {
        queue_db_add_directory(target_directory, &qd);
        nc_send_queue_directory_added_notification(nc_default(),
                target_directory, nick);
    }

    return 0;
}

/* Remove all files in the directory.
 */
int queue_remove_directory(const char *target_directory)
{
    return_val_if_fail(target_directory, -1);

    while(*target_directory == '/')
        ++target_directory;

    /* Loop through all targets and look for targets belonging to the
     * target_directory. This is a possibly expensive operation.
     */
    DB_TXN *txn = NULL;
    return_val_if_fail(db_transaction(&txn) == 0, -1);

    DBC *qtc = NULL;
    queue_target_db->cursor(queue_target_db, txn, &qtc, 0);
    txn_return_val_if_fail(qtc, -1);

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));

    g_debug("removing targets in directory [%s]", target_directory);

    while(qtc->c_get(qtc, &key, &val, DB_NEXT) == 0)
    {
        queue_target_t *qt = val.data;

        if(strcmp(target_directory, qt->target_directory) == 0)
        {
            queue_remove_sources(qt->filename);
            g_debug("removing target [%s]", qt->filename);
            if(qtc->c_del(qtc, 0) != 0)
            {
                g_warning("failed to remove target [%s]", qt->filename);
                qtc->c_close(qtc);
                txn->abort(txn);
                return -1;
            }
        }
    }

    txn_return_val_if_fail(qtc->c_close(qtc) == 0, -1);
    return_val_if_fail(txn->commit(txn, 0) == 0, -1);

    /* FIXME: shouldn't the directory removal be part of the transaction? */
    queue_db_remove_directory(target_directory);

    return 0;
}

#ifdef TEST

#include "unit_test.h"

int got_filelist_notification = 0;
int got_directory_notification = 0;
int got_directory_removed_notification = 0;
int got_target_removed_notification = 0;

void handle_filelist_added_notification(nc_t *nc, const char *channel,
        nc_filelist_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->nick);
    g_debug("added filelist for %s", data->nick);
    fail_unless(strcmp(data->nick, "bar") == 0);
    got_filelist_notification = 1;
}

void handle_queue_directory_added_notification(nc_t *nc, const char *channel,
        nc_queue_directory_added_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_directory);
    g_debug("added directory %s", data->target_directory);
    fail_unless(strcmp(data->target_directory, "target/directory") == 0);
    fail_unless(data->nick);
    fail_unless(strcmp(data->nick, "bar") == 0);
    got_directory_notification = 1;
}

void handle_queue_directory_removed_notification(nc_t *nc, const char *channel,
        nc_queue_directory_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_directory);
    g_debug("removed directory %s", data->target_directory);
    fail_unless(strcmp(data->target_directory, "target/directory") == 0);
    got_directory_removed_notification = 1;
}

void handle_queue_target_removed_notification(nc_t *nc, const char *channel,
        nc_queue_target_removed_t *data, void *user_data)
{
    fail_unless(data);
    fail_unless(data->target_filename);
    g_debug("removed target %s", data->target_filename);
    /* fail_unless(strcmp(data->target_directory, "target/directory") == 0); */
    ++got_target_removed_notification;
}

void test_setup(void)
{
    global_working_directory = "/tmp/sp-queue_directory-test.d";
    system("/bin/rm -rf /tmp/sp-queue_directory-test.d");
    system("mkdir /tmp/sp-queue_directory-test.d");

    fail_unless(queue_init() == 0);
}

void test_teardown(void)
{
    fail_unless(queue_close() == 0);
    system("/bin/rm -rf /tmp/sp-queue_directory-test.d");
}

/* create a sample filelist for the "bar" user */
void test_create_filelist(void)
{
    /* create a filelist in our working directory */
    char *fl_path;
    asprintf(&fl_path, "%s/files.xml.bar", global_working_directory);

    FILE *fp = fopen(fl_path, "w");
    free(fl_path);
    fail_unless(fp);

    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\n"
            "<FileListing Version=\"1\" CID=\"NOFUKZZSPMR4M\" Base=\"/\" Generator=\"DC++ 0.674\">\n"
            "<Directory Name=\"source\">\n"
            "  <Directory Name=\"directory\">\n"
            "    <File Name=\"filen\" Size=\"26577\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMHIWXVSY\"/>\n"
            "    <File Name=\"filen2\" Size=\"1234567\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMXXXXXXX\"/>\n"
            "      <Directory Name=\"subdir\">\n"
            "        <File Name=\"filen3\" Size=\"2345678\" TTH=\"ABAJCAPSGKJMY7IFTZA7XSE2AINPGZYMXXXZZZZ\"/>\n"
            "      </Directory>\n"
            "  </Directory>\n"
            "</Directory>\n"
            "</FileListing>\n");
    fail_unless(fclose(fp) == 0);
}

/* add a directory and make sure we get notifications of added directory + the
 * users filelist */
void test_add_directory_no_filelist(void)
{
    test_setup();

    got_filelist_notification = 0;
    got_directory_notification = 0;

    unlink("/tmp/files.xml.bar");

    g_debug("adding directory");
    fail_unless(queue_add_directory("bar",
                "source\\directory", "target/directory") == 0);
    fail_unless(got_filelist_notification == 1);
    fail_unless(got_directory_notification == 1);

    /* now we should download the filelist */
    g_debug("downloading the filelist");
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "bar") == 0);
    fail_unless(q->is_filelist == 1);
    queue_free(q);

    /* we got it */ 
    test_create_filelist();
    fail_unless(queue_remove_filelist("bar") == 0);

    /* Next queue should be the placeholder directory. This time we have the
     * filelist, so resolving it should be successful. */
    g_debug("resolving the filelist");
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->nick);
    fail_unless(strcmp(q->nick, "bar") == 0);
    fail_unless(q->is_directory);
    fail_unless(q->source_filename);
    fail_unless(strcmp(q->source_filename, "source\\directory") == 0);
    fail_unless(q->target_filename);
    fail_unless(strcmp(q->target_filename, "target/directory") == 0);

    unsigned nfiles = 0;
    got_directory_removed_notification = 0;
    fail_unless(queue_resolve_directory(q->nick,
                q->source_filename, q->target_filename, &nfiles) == 0);
    queue_free(q);
    /* fail_unless(got_directory_removed_notification == 1); */
    fail_unless(nfiles == 3);

    test_teardown();
    puts("PASSED: add directory w/o filelist");
}

/* Add a directory from a user whose filelist we've already downloaded.
 */
void test_add_directory_existing_filelist(void)
{
    test_setup();
    test_create_filelist();

    got_filelist_notification = 0;
    got_directory_notification = 0;

    /* add a directory to the download queue, make sure the filelist we created
     * above is used (ie, no new filelist should be added to the queue) */
    fail_unless(queue_add_directory("bar", "source\\directory",
                "target/directory") == 0);
    fail_unless(got_filelist_notification == 0);
    fail_unless(got_directory_notification == 1);

    /* we should be able to download the file in the filelist */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q);
    fail_unless(q->source_filename);
    fail_unless(q->target_filename);
    g_debug("source_filename = %s", q->source_filename);
    fail_unless(q->tth);
    fail_unless(strncmp(q->tth, "ABAJCAPSGKJMY7IFTZA7XSE2AINPGZ", 30) == 0);
    queue_free(q);

    /* All resolved files should belong to the same queue_directory_t
     */
    queue_target_t *qt = queue_lookup_target("target/directory/subdir/filen3");
    fail_unless(qt);
    g_debug("target_directory = [%s]", qt->target_directory);
    fail_unless(strcmp(qt->target_directory, "target/directory") == 0);

    qt = queue_lookup_target("target/directory/filen");
    fail_unless(qt);
    g_debug("target_directory = [%s]", qt->target_directory);
    fail_unless(strcmp(qt->target_directory, "target/directory") == 0);

    got_directory_removed_notification = 0;
    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("target/directory/filen") == 0);
    fail_unless(queue_remove_target("target/directory/subdir/filen3") == 0);
    fail_unless(got_target_removed_notification == 2);
    fail_unless(got_directory_removed_notification == 0);

    /* We've download the whole directory. When removing the last file in the
     * directory we should get a notification that the whole directory is
     * removed from the queue. */

    got_directory_removed_notification = 0;
    got_target_removed_notification = 0;
    fail_unless(queue_remove_target("target/directory/filen2") == 0);
    fail_unless(got_target_removed_notification == 1);
    fail_unless(got_directory_removed_notification == 1);

    test_teardown();
    puts("PASSED: add directory w/ existing filelist");
}

void test_remove_directory(void)
{
    test_setup();
    test_create_filelist();

    /* there should be nothing in the queue to start with */
    queue_t *q = queue_get_next_source_for_nick("bar");
    fail_unless(q == NULL);

    /* add a directory */
    fail_unless(queue_add_directory("bar", "source\\directory",
                "target/directory") == 0);

    /* now we should have a directory to download */
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q);

    /* and remove the directory */
    fail_unless(queue_remove_directory("target/directory") == 0);

    /* again, nothing in the queue */
    q = queue_get_next_source_for_nick("bar");
    fail_unless(q == NULL);

    test_teardown();
    puts("PASSED: remove directory");
}

int main(void)
{
    sp_log_set_level("debug");

    nc_add_filelist_added_observer(nc_default(),
            handle_filelist_added_notification, NULL);
    nc_add_queue_directory_added_observer(nc_default(),
            handle_queue_directory_added_notification, NULL);
    nc_add_queue_directory_removed_observer(nc_default(),
            handle_queue_directory_removed_notification, NULL);
    nc_add_queue_target_removed_observer(nc_default(),
            handle_queue_target_removed_notification, NULL);

    test_add_directory_no_filelist();
    test_add_directory_existing_filelist();
    test_remove_directory();

    return 0;
}

#endif

