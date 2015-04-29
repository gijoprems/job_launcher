#ifndef _LISTENER_H_
#define _LISTENER_H_

#include <pthread.h>
#include "comlink.h"

/*****************************************************************************/

#define MAX_INSTANCES    (100)
#define MAX_FILENAME_LEN (256)
#define MAX_HOSTNAME_LEN (256)

/*****************************************************************************/
/* listener session params */

typedef struct listener_session_s {
    /* comlink params for lancher<->listener session */
    comlink_params_t cl_params;
    int skt_fd; /* keep the client fd for reply */
    struct sockaddr_in skt_addr;
    
    /* host info */
    int instances;
    char hostname[MAX_HOSTNAME_LEN];
    char exe_name[MAX_FILENAME_LEN];
    
    /* for managing the proc spawn thread */
    int spawn_task_stop;
    pthread_t spawn_task;
    pthread_attr_t spawn_task_attr;

    /* for the status spawned processes */
    pid_t spawned[MAX_INSTANCES];
    int nr_failed;
    char status[32];
}listener_session_t;

/*****************************************************************************/

#endif /* _LISTENER_H_ */
