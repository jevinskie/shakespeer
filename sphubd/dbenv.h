#ifndef _dbenv_h_
#define _dbenv_h_

#include <db.h>

DB_ENV *get_default_db_environment(void);
void close_default_db_environment(void);
int verify_db(const char *dbfile, const char *db_list[]);
void backup_db(const char *dbfile);
int open_database(DB **db, const char *dbfile, const char *dbname,
        int type, int flags);
int close_db(DB **db, const char *dbname);

#endif

