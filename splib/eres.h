#ifndef _eres_h_
#define _eres_h_

typedef void (*eres_callback_t)(int error, struct hostent *host,
        int af, void *user_data);

int eres_query(const char *host, int af, eres_callback_t cb, void *user_data);

#endif

