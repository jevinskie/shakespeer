#include "globals.h"

int global_port = -1;
void *global_share = 0;
void *global_search_listener = 0;
char *global_working_directory = 0;
char *argv0_path = 0;
const char *global_dcpp_version = "0.674";
int global_follow_redirects = 1;
int global_auto_match_filelists = 1;
int global_auto_search_sources = 1;
unsigned int global_hash_prio = 2;

char *global_download_directory = 0;
char *global_storage_directory = 0;

