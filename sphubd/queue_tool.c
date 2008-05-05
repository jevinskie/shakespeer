#include <string.h>

#include "util.h"
#include "log.h"
#include "globals.h"
#include "queue.h"

extern DB *queue_target_db;
extern DB *queue_source_db;
extern DB *queue_directory_db;

int main(int argc, char **argv)
{
    global_working_directory = get_working_directory();
    sp_log_set_level("debug");
    if(queue_init() != 0)
        return 1;

    DBT key, val;
    memset(&key, 0, sizeof(DBT));
    memset(&val, 0, sizeof(DBT));


    DBC *qfc;
    queue_source_db->cursor(queue_source_db, NULL, &qfc, 0);
    printf("Filelists:\n");
    while(qfc->c_get(qfc, &key, &val, DB_NEXT) == 0)
    {
        char *nick = key.data;
        /* queue_filelist_t *qf = val.data; */
        printf("%10s: %30s\n", (char *)key.data, nick);
    }
    qfc->c_close(qfc);


    DBC *qdc;
    queue_directory_db->cursor(queue_directory_db, NULL, &qdc, 0);
    printf("Directories:\n");
    while(qdc->c_get(qdc, &key, &val, DB_NEXT) == 0)
    {
        queue_directory_t *qd = val.data;
        printf("%s (%i/%i left)\n", qd->target_directory, qd->nleft, qd->nfiles);
    }
    qdc->c_close(qdc);


    DBC *qtc;
    queue_target_db->cursor(queue_target_db, NULL, &qtc, 0);
    printf("Targets:\n");
    while(qtc->c_get(qtc, &key, &val, DB_NEXT) == 0)
    {
        queue_target_t *qt = val.data;
        printf("%30s (directory %s)\n", qt->filename, qt->target_directory);
    }
    qtc->c_close(qtc);


    DBC *qsc;
    queue_source_db->cursor(queue_source_db, NULL, &qsc, 0);
    printf("Sources:\n");
    while(qsc->c_get(qsc, &key, &val, DB_NEXT) == 0)
    {
        queue_source_t *qs = val.data;
        printf("%10s: %30s\n", (char *)key.data, qs->target_filename);
    }
    qsc->c_close(qsc);

    queue_close();

    return 0;
}

