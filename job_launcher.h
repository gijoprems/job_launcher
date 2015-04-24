/*
 * job_launcher: A simple job launcher utility
 */
#ifndef _JOB_LAUNCHER_H_
#define _JOB_LAUNCHER_H_

#include <netinet/in.h>

/******************************************************************/

#define MAX_HOSTNAME_LEN (256)
#define MAX_HOSTS        (100)

/******************************************************************/

/* host info table */
typedef struct host_info_s {
    char hostname[MAX_HOSTNAME_LEN];
    struct sockaddr_in sa_comlink;

    /* no of instances, keeping it host specific */
    int instances;
}host_info_t;

/******************************************************************/

/* place holder for the context storage */
typedef struct lanucher_session_s {
    int host_count;
    host_info_t *host_info[MAX_HOSTS];
}launcher_session_t;

/******************************************************************/

#endif /* _JOB_LAUNCHER_H_ */
