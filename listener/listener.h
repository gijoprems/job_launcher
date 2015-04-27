#ifndef _LISTENER_H_
#define _LISTENER_H_

#include "comlink.h"

/*****************************************************************************/

#define MAX_FILENAME_LEN (256)

/*****************************************************************************/
/* listener session params */

typedef struct listener_session_s {
    /* comlink params for lancher<->listener session */
    comlink_params_t cl_params;

    /* host info */
    int instances;
    char exe_name[MAX_FILENAME_LEN];
}listener_session_t;

/*****************************************************************************/

#endif /* _LISTENER_H_ */
