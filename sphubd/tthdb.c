/*
 * Copyright (c) 2007 Martin Hedenfalk <martin@bzero.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tthdb.h"
#include "base64.h"
#include "log.h"
#include "globals.h"

int tth_entry_cmp(struct tth_entry *a, struct tth_entry *b)
{
	return strcmp(a->tth, b->tth);
}

int tth_inode_cmp(struct tth_inode *a, struct tth_inode *b)
{
	return b->inode - a->inode;
}

RB_GENERATE(tth_entries_head, tth_entry, link, tth_entry_cmp);
RB_GENERATE(tth_inodes_head, tth_inode, link, tth_inode_cmp);

static void tth_parse_add_tth(struct tth_store *store,
	char *buf, size_t len, off_t offset)
{
	buf += 3; /* skip past "+T:" */
	len -= 3;

	/* syntax of buf is 'tth:leafdata_base64' */

	if(len <= 40 || buf[39] != ':')
	{
		WARNING("failed to load tth on line %u", store->line_number);
	}
	else
	{
		/* Don't read the leafdata into memory. Instead we
		 * store the offset for this entry in the file, so
		 * we easily can retrieve it when needed.
		 */

		buf[39] = 0;
		tth_store_add_entry(store, buf, NULL, offset);
	}
}

static void tth_parse_add_inode(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "+I:" */
	len -= 3;

	/* syntax of buf is 'inode:mtime:tth' */
	/* numeric values are stored in hex */

	uint64_t inode;
	unsigned long mtime;
	char tth[40];

	int rc = sscanf(buf, "%llX:%lX:%s", &inode, &mtime, tth);
	if(rc != 3 || inode == 0 || mtime == 0 || tth[39] != 0)
		WARNING("failed to load inode on line %u", store->line_number);
	else
		tth_store_add_inode(store, inode, mtime, tth);
}

static void tth_parse_remove_tth(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "-T:" */
	len -= 3;

	tth_store_remove(store, buf);
}

static void tth_parse_remove_inode(struct tth_store *store, char *buf, size_t len)
{
	buf += 3; /* skip past "-I:" */
	len -= 3;

	char *endptr = NULL;
	uint64_t inode = strtoull(buf, &endptr, 16);
	if(endptr == NULL || *endptr != 0 || inode == 0)
	{
		WARNING("failed to load TTH remove on line %u",
			store->line_number);
	}
	else
		tth_store_remove_inode(store, inode);
}

static void tth_parse(struct tth_store *store)
{
	int ntth = 0;
	int ninode = 0;

	store->loading = true;
	store->need_normalize = false;

	char *buf, *lbuf = NULL;
	size_t len;
	off_t offset = 0;
	while((buf = fgetln(store->fp, &len)) != NULL)
	{
		store->line_number++;

		if(buf[len - 1] == '\n')
			buf[len - 1] = 0;
		else
		{
			/* EOF without EOL, copy and add the NUL */
			lbuf = malloc(len + 1);
			assert(lbuf);
			memcpy(lbuf, buf, len);
			lbuf[len] = 0;
			buf = lbuf;
		}

		if(len < 3 || buf[2] != ':')
			continue;

		if(strncmp(buf, "+T:", 3) == 0)
		{
			tth_parse_add_tth(store, buf, len, offset);
			ntth++;
		}
		else if(strncmp(buf, "+I:", 3) == 0)
		{
			tth_parse_add_inode(store, buf, len);
			ninode++;
		}
		else if(strncmp(buf, "-T:", 3) == 0)
		{
			tth_parse_remove_tth(store, buf, len);
			ntth--;
			store->need_normalize = true;
		}
		else if(strncmp(buf, "-I:", 3) == 0)
		{
			tth_parse_remove_inode(store, buf, len);
			ninode--;
			store->need_normalize = true;
		}
		else
		{
			INFO("unknown type %02X%02X, skipping line %u",
				(unsigned char)buf[0], (unsigned char)buf[1],
				store->line_number);
			store->need_normalize = true;
		}

		offset = ftell(store->fp);
	}
	free(lbuf);

	INFO("done loading TTH store (%i TTHs, %i inodes)", ntth, ninode);

	store->loading = false;
}

static struct tth_store *tth_load(const char *filename)
{
	struct tth_store *store = calloc(1, sizeof(struct tth_store));

	store->filename = strdup(filename);
	RB_INIT(&store->entries);
	RB_INIT(&store->inodes);

	store->fp = fopen(filename, "a+");
	rewind(store->fp);
	INFO("loading TTH store from [%s]", filename);
	tth_parse(store);

	return store;
}

void tth_store_init(void)
{
	return_if_fail(global_working_directory);

	char *tth_store_filename;
	asprintf(&tth_store_filename, "%s/tth2.db", global_working_directory);
	global_tth_store = tth_load(tth_store_filename);
	free(tth_store_filename);
}

static void tth_close_database(struct tth_store *store)
{
	INFO("closing TTH database");

	fclose(store->fp);
	free(store->filename);
	free(store);
}

void tth_store_close(void)
{
	tth_close_database(global_tth_store);
}

void tth_store_add_inode(struct tth_store *store,
	 uint64_t inode, time_t mtime, const char *tth)
{
	struct tth_inode *ti;

	ti = tth_store_lookup_inode(store, inode);
	if(ti == NULL)
	{
		ti = calloc(1, sizeof(struct tth_inode));
		ti->inode = inode;
		RB_INSERT(tth_inodes_head, &store->inodes, ti);
	}

	if(ti->mtime != mtime || strcmp(ti->tth, tth) != 0)
	{
		ti->mtime = mtime;
		strlcpy(ti->tth, tth, sizeof(ti->tth));

		if(!store->loading)
		{
			fprintf(store->fp, "+I:%llX:%lX:%s\n",
				inode, (unsigned long)mtime, tth);
		}
	}
}

void tth_store_add_entry(struct tth_store *store,
	const char *tth, const char *leafdata_base64,
	off_t leafdata_offset)
{
	struct tth_entry *te;

	te = tth_store_lookup(store, tth);

	if(te == NULL)
	{
		te = calloc(1, sizeof(struct tth_entry));
		strlcpy(te->tth, tth, sizeof(te->tth));
		te->leafdata_offset = leafdata_offset;

		RB_INSERT(tth_entries_head, &store->entries, te);
	}

	if(!store->loading)
	{
		return_if_fail(leafdata_base64);

		int len = fprintf(store->fp, "+T:%s:%s\n",
			tth, leafdata_base64);

		/* Call ftell() _after_ we have written the +T line, because
		 * the file is opened in append mode and we might have
		 * seek'd earlier to read leafdata. This way we are certain
		 * we get the correct offset.
		 */
		if(len > 0)
			te->leafdata_offset = ftell(store->fp) - len;
	}
}

/* load the leafdata for the given TTH from the backend store */
int tth_store_load_leafdata(struct tth_store *store, struct tth_entry *entry)
{
	return_val_if_fail(store, -1);
	return_val_if_fail(entry, -1);

	if(entry->leafdata)
		return 0; /* already loaded */

	INFO("loading leafdata for tth [%s]", entry->tth);

	/* seek to the entry->leafdata_offset position in the backend store */
	int rc = fseek(store->fp, entry->leafdata_offset, SEEK_SET);
	if(rc == -1)
	{
		WARNING("seek to %llu failed", entry->leafdata_offset);
		goto failed;
	}

	size_t len;
	char *buf = fgetln(store->fp, &len);
	if(buf == NULL)
	{
		WARNING("failed to read line @offset %llu",
			entry->leafdata_offset);
		goto failed;
	}

	if(len < 3 || strncmp(buf, "+T:", 3) != 0)
	{
		WARNING("invalid start tag: [%s]", buf);
		goto failed;
	}

	buf += 3;
	len -= 3;

	if(len <= 40 || buf[39] != ':' || strncmp(buf, entry->tth, 39) != 0)
	{
		WARNING("offset points to wrong tth: [%s]", buf);
		goto failed;
	}

	buf += 40;
	len -= 40;

	unsigned leafdata_len = strcspn(buf, ":");
	if(buf[leafdata_len] != ':')
	{
		WARNING("missing delimiter at end of leafdata: [%s]", buf);
		goto failed;
	}

	buf[leafdata_len] = 0; /* nul-terminate base64 encoded leafdata */

	entry->leafdata = malloc(leafdata_len);
	assert(entry->leafdata);
	int outlen = base64_pton(buf,
		(unsigned char *)entry->leafdata, leafdata_len);
	if(outlen <= 0)
	{
		WARNING("invalid base64 encoded leafdata");
		free(entry->leafdata);
		entry->leafdata = NULL;
		goto failed;
	}

	return 0;

failed:
	WARNING("failed to load leafdata for tth [%s]: %s",
		entry->tth, strerror(errno));

	/* FIXME: should we re-load the tth store ? */
	return -1;
}

struct tth_entry *tth_store_lookup(struct tth_store *store, const char *tth)
{
	struct tth_entry find;
	strlcpy(find.tth, tth, sizeof(find.tth));
	return RB_FIND(tth_entries_head, &store->entries, &find);
}

static void tth_entry_free(struct tth_entry *entry)
{
	if(entry)
	{
		free(entry->leafdata);
		free(entry);
	}
}

void tth_store_remove(struct tth_store *store, const char *tth)
{
	struct tth_entry *entry = tth_store_lookup(store, tth);
	if(entry)
	{
		RB_REMOVE(tth_entries_head, &store->entries, entry);

		if(!store->loading)
		{
			fprintf(store->fp, "-T:%s\n", tth);
		}

		tth_entry_free(entry);
	}
}

struct tth_entry *tth_store_lookup_by_inode(struct tth_store *store, uint64_t inode)
{
	struct tth_inode *ti = tth_store_lookup_inode(store, inode);
	if(ti)
		return tth_store_lookup(store, ti->tth);
	return NULL;
}

static void tth_inode_free(struct tth_inode *ti)
{
	if(ti)
	{
		free(ti);
	}
}

void tth_store_remove_inode(struct tth_store *store, uint64_t inode)
{
	struct tth_inode *ti = tth_store_lookup_inode(store, inode);
	if(ti)
	{
		RB_REMOVE(tth_inodes_head, &store->inodes, ti);

		if(!store->loading)
		{
			fprintf(store->fp, "-I:%llX\n", inode);
		}

		tth_inode_free(ti);
	}
}

struct tth_inode *tth_store_lookup_inode(struct tth_store *store, uint64_t inode)
{
	struct tth_inode find;
	find.inode = inode;
	return RB_FIND(tth_inodes_head, &store->inodes, &find);
}

void tth_store_set_active_inode(struct tth_store *store, const char *tth, uint64_t inode)
{
	return_if_fail(store);
	return_if_fail(tth);

	struct tth_entry *te = tth_store_lookup(store, tth);
	return_if_fail(te);

	/* switch active inode for this TTH */
	te->active_inode = inode;
}

#ifdef TEST

int main(void)
{
	return 0;
}

#endif

