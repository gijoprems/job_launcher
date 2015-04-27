/*
 * listener: listener module is a part of the job launcher. This acts
 *           as the stub which listens for commands from the launcher.
*/

/* listener.c -- uses comlink to establish a connection with the launcher */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "listener.h"

/*****************************************************************************/

#define COMLINK_PORT     (25000)
#define COMLINK_BUF_SIZE (1024)

/*****************************************************************************/

static char comlink_buf[COMLINK_BUF_SIZE];
static listener_session_t listener_session;

/*****************************************************************************/

static void listener_rxmsg_callback(int fd, char *buf, int len)
{
    fprintf(stdout, "listener: rxdata = %s \n", buf);
}

/*****************************************************************************/

static void listener_shutdown_callback(int fd)
{
    fprintf(stdout, "listener: peer shutdown \n");
    if (fd != -1)
        close(fd);
}

/*****************************************************************************/
/* listener session setup is essentially setting up comlink */

static int listener_session_setup(listener_session_t *session)
{
    comlink_params_t *cl_params = &session->cl_params;

    memset(cl_params, 0, sizeof(comlink_params_t));
    cl_params->buffer = comlink_buf;
    cl_params->buf_len = COMLINK_BUF_SIZE;
    cl_params->local_port = COMLINK_PORT;
    cl_params->remote_port = COMLINK_PORT;
    cl_params->receive_cb = listener_rxmsg_callback;
    cl_params->shutdown_cb = listener_shutdown_callback;        
    if (comlink_server_setup(cl_params) == -1) {
        fprintf(stderr,
                "listener: comlink server setup failed \n");
        return -1;
    }
    
    return 0;
}

/*****************************************************************************/
/* main handler for the listener */

static int listener_session_start(listener_session_t *session)
{
    comlink_server_start();

    return 0;
}

/*****************************************************************************/

static void listener_session_cleanup(listener_session_t *session)
{
    comlink_server_shutdown();
}

/*****************************************************************************/

int main(void)
{
    listener_session_t *session = &listener_session;

    /* session setup */
    if (listener_session_setup(session) != 0)
        exit(2);

    /* starts the execution; waits until done */
    listener_session_start(session);

    /* done with the session; cleans-up */
    listener_session_cleanup(session);

    return 0;
}

/*****************************************************************************/
