#include "globals.h"

int global_port = -1;
void *global_share = 0;
void *global_search_listener = 0;
char *global_working_directory = 0;
char *argv0_path = 0;
bool global_follow_redirects = true;
bool global_auto_match_filelists = true;
bool global_auto_search_sources = true;
unsigned global_hash_prio = 2;

char *global_incomplete_directory = 0;
char *global_download_directory = 0;

char *global_id_version = 0;
char *global_id_tag = 0;
char *global_id_generator = 0;
char *global_id_lock = 0;

/* If true, complete files in partially downloaded directories are moved to the
 * "complete" download directory (global_download_directory). Otherwise the
 * files are moved once the whole directory is complete (all files are done).
 */
bool global_move_partial_directories = false;

