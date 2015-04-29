/*
 * job_launcher: A simple job launcher utility
 */
#ifndef _JOB_LAUNCHER_H_
#define _JOB_LAUNCHER_H_

#include "comlink.h"

/******************************************************************/

#define MAX_HOSTS        (100)
#define MAX_HOSTNAME_LEN (256)
#define MAX_FILENAME_LEN (256)

/******************************************************************/
/* host info table */

typedef struct host_info_s {
    char hostname[MAX_HOSTNAME_LEN];
}host_info_t;

/******************************************************************/
/* place holder for the context storage */

typedef struct lanucher_session_s {
    int skt_conns[MAX_HOSTS];
    /* comlink params for lancher<->listener session */
    comlink_params_t cl_params;

    /* remote host info */
    int instances;
    int host_count;
    host_info_t *host_info[MAX_HOSTS];
    char exe_name[MAX_FILENAME_LEN];
    char host_file[MAX_FILENAME_LEN];

    /* remote status info */
    int nr_active;
    int nr_ackd;
}launcher_session_t;

/******************************************************************/

#endif /* _JOB_LAUNCHER_H_ */
