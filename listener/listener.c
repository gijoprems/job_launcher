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
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

#include "listener.h"
#include "common.h"

/*****************************************************************************/

#define COMLINK_PORT     (25000)
#define COMLINK_BUF_SIZE (1024)

/*****************************************************************************/

static char comlink_buf[COMLINK_BUF_SIZE];
static listener_session_t listener_session;

/*****************************************************************************/

static listener_session_t * get_listener_session(void)
{
    return &listener_session;
}

/*****************************************************************************/
/* cleanup the spawned processes */

static void cleanup_spawned_instances(listener_session_t *session)
{
    int i;

    for(i = 0; i < session->instances; i++) {
        if (kill(session->spawned[i], SIGTERM) == -1)
            kill(session->spawned[i], SIGKILL);
    }
}

/*****************************************************************************/
/* actual instaces handler */

static void * spawn_task_main(void *arg)
{
    int i, status;
    pid_t wpid;
    listener_session_t *session = (listener_session_t *)arg;
    char *argv[2] = { session->exe_name, NULL };

    session->nr_failed = 0;
    while(!session->spawn_task_stop) {
        for (i = 0; i < session->instances; i++) {
            session->spawned[i] = fork();
            if (session->spawned[i] == 0) {
                execvp(argv[0], argv);
            }
        }

        while ((wpid = wait(&status)) > 0) {
            fprintf(stdout, "termination status(%d) = %d ",
                    (int)wpid, status);
            if (WIFEXITED(status))
                fprintf(stdout, "status = %d \n", WEXITSTATUS(status));
        }
        break;
    }
    
    return NULL;
}

/*****************************************************************************/
/* creates a task for handling  multiple instaces of the command */

static int spawn_task_setup(void)
{
    int ret;
    listener_session_t *session = get_listener_session();

    session->spawn_task_stop = 0;
    ret = pthread_attr_init(&session->spawn_task_attr);
    if(ret != 0) {
        fprintf(stderr,"listener: ptherad attr_init, %s(%d) \n",
                strerror(errno), errno);
        return -1;
    }

    ret = pthread_create(&session->spawn_task,
            &session->spawn_task_attr, spawn_task_main, (void *)session);
    if (ret != 0) {
        fprintf(stderr,"listener: ptherad create, %s(%d) \n",
                strerror(errno), errno);
        return -1;
    }

    return 0;
}

/*****************************************************************************/
/* to handle the ctrl messages like start, break */

static int listener_handle_ctrlmsg(char *buf, listener_session_t *s)
{
    int ret = 0;
    
    /* do normal strcmp; improve later */
    fprintf(stdout, "listener: ctrl message = %s \n", buf);

    if (strcmp(buf, "start") == 0) {
        ret = spawn_task_setup();
    }
    else if (strcmp(buf, "stop") == 0) {
        cleanup_spawned_instances(s);
        s->spawn_task_stop = 1;
        ret = 0;
    }
    else
        return -1;

    return ret;
}

/*****************************************************************************/

static void listener_rxmsg_callback(int fd, unsigned int msg_type,
        char *buf, int len)
{
    listener_session_t *session = get_listener_session();
    char temp_buf[256];

    switch(msg_type) {
        case PROC_INSTANCES:
            session->instances = *(int *)buf;
            fprintf(stdout, "listener: instances = %d \n", session->instances);    
            break;
            
        case EXEC_FILENAME:
            strcpy(session->exe_name, buf);
            fprintf(stdout, "listener: exec name = %s \n", session->exe_name);    
            break;

        case CTRL_MESSAGE:
            memset(temp_buf, '\n', 256);
            strcpy(temp_buf, buf);
            listener_handle_ctrlmsg(buf, session);
            break;
            
        default:
            fprintf(stderr,
                    "listener: unknown msg type(%d), ignoring \n", msg_type);
    }
}

/*****************************************************************************/

static void listener_shutdown_callback(int fd)
{
    fprintf(stderr, "server: peer shotdown, cleaning-up \n");
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
/* handles SIGINT */

static void listener_signal_handler(int signal)
{
    listener_session_t *session = get_listener_session();

    fprintf(stdout, "Ctrl+C, exiting \n");
    listener_session_cleanup(session);
}

/*****************************************************************************/

int main(void)
{
    struct sigaction sa;
    listener_session_t *session = get_listener_session();

    /* session setup */
    if (listener_session_setup(session) != 0)
        exit(2);

    /* regster the signal handler for handing terminal signals */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = listener_signal_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stdout,
                "Warning, session will be unstable \n");
    }
        
    /* starts the execution; waits until done */
    listener_session_start(session);

    /* done with the session; cleans-up */
    listener_session_cleanup(session);

    return 0;
}

/*****************************************************************************/
