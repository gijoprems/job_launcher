/*
 *  job_launcher: A simple job launcher utility to launch/execute multiple instances
 *               of another program/executable on a specified set of compute 
 *               cores/nodes.
 * 
 *     - The launcher waits until the target executable finishes its
 *       execution and report the termination status of the executable.
 *
 *     - It handles signals and does a graceful exit in case of a termination
 *       signal. All the resources are cleaned-up both on the local and remote
 *       machines in case of an exit request. 
 */

/* job_launcher.c  -- Application entry point. Handles the command line options */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "job_launcher.h"

/*****************************************************************************/

#define MAX_CMD_ARGS  (6)
#define MAX_INSTANCES (100)

#define COMLINK_PORT     (25000)
#define COMLINK_BUF_SIZE (1024)

/*****************************************************************************/

static char comlink_buf[COMLINK_BUF_SIZE];
static launcher_session_t launcher_session;

/*****************************************************************************/

static int usage(char *program)
{
    fprintf(stderr, "\n%s: -np <instances> -hostfile <hostfile>"
            " <exe-name including path> \n", program);

    return 0;
}

/*****************************************************************************/
/* cmdline parser */

static int parse_cmdline(int argc, char *argv[],
        launcher_session_t *session)
{
    if (argc < MAX_CMD_ARGS) {
        usage(argv[0]);
        return -1;
    }

    /* TODO: use getopt instead */
    
    if (strncmp(argv[1], "-np", 3) == 0)
        session->instances = atoi(argv[2]); 

    if (strncmp(argv[3], "-hostfile", 9) == 0)
        strcpy(session->host_file, argv[4]);

    strcpy(session->exe_name, argv[5]);
    /* validate the options */
    if ((session->instances < 0 &&
            session->instances > MAX_INSTANCES) ||
            strncmp(session->host_file, "", 1) == 0 || 
            strncmp(session->exe_name, "", 1) == 0) {      
        return -1;
    }

    fprintf(stdout, "launcher: instances = %d, hostfile = %s, exec = %s \n", 
            session->instances, session->host_file, session->exe_name);

    return 0;
}

/*****************************************************************************/

static int alloc_host_entry(launcher_session_t *session, int host_index)
{
    int k;

    k = sizeof(host_info_t);
    session->host_info[host_index] = (host_info_t *)malloc(k);    
    if (session->host_info[host_index] == NULL) {
        fprintf(stderr, "launcher: error allocating session, %s(%d) \n",
	        strerror(errno), errno);
        return -1;  
    }

    return 0;
} 

/*****************************************************************************/
/* get the hostnames and returns the number of hosts */

static int parse_hostfile(char *file)
{
    int count = 0, status = 0; 
    FILE *fp = NULL;
    char buffer[MAX_HOSTNAME_LEN];
    launcher_session_t *session = &launcher_session;
    
    if ((fp = fopen(file, "rb")) == NULL) {
        fprintf(stderr, "launcher: error opening hostfile %s: %s(%d) \n", 
                file, strerror(errno), errno);	
        exit(2);
    }

    memset(buffer, '\n', MAX_HOSTNAME_LEN);
    session->host_count = 0;
    while(fgets(buffer, MAX_HOSTNAME_LEN, fp) != NULL) {
	status = alloc_host_entry(session, session->host_count);
	if (status == 0) {
	    count = session->host_count;
	    strcpy(session->host_info[count]->hostname, buffer);
            fprintf(stdout, "launcher: host name: %s \n",
                    session->host_info[session->host_count]->hostname);
            session->host_count += 1;
        }
        memset(buffer, '\n', MAX_HOSTNAME_LEN);
    }

    fclose(fp);
    count = session->host_count;

    /* using count for count and index; hence +1 */
    return count;
}

/*****************************************************************************/

static void launcher_rxmsg_callback(int fd, char *buf, int len)
{
    fprintf(stdout, "launcher: rxdata = %s \n", buf);
}

/*****************************************************************************/

/* launcher session setup is essentially setting up comlink */

static int launcher_session_setup(launcher_session_t *session)
{
    int fd, i;
    struct sockaddr skt_addr;
    struct sockaddr_in *remote_addr;
    
    comlink_params_t *cl_params = &session->cl_params;

    memset(cl_params, 0, sizeof(comlink_params_t));
    cl_params->buffer = comlink_buf;
    cl_params->buf_len = COMLINK_BUF_SIZE;
    cl_params->local_port = COMLINK_PORT;
    cl_params->remote_port = COMLINK_PORT;
    cl_params->receive_cb = launcher_rxmsg_callback;
    cl_params->shutdown_cb = NULL;

    for(i = 0; i < session->host_count; i++) {
        if (hostname_to_netaddr(session->host_info[i]->hostname,
                &skt_addr) != 0)
            continue;    

        remote_addr = (struct sockaddr_in *)&skt_addr;
        cl_params->remote_ip = ntohl(remote_addr->sin_addr.s_addr);
        fd = comlink_client_setup(cl_params);
        session->skt_conns[i] = fd;
        if (fd == -1)
            fprintf(stderr, "launcher: comlink client setup failed for ip %08x \n",
                    cl_params->remote_ip);
    }
    
    return 0;
}

/*****************************************************************************/
/* main handler for the launcher */

static int launcher_session_start(launcher_session_t *session)
{
    int i, len;
    char *msg = session->exe_name;

    len = strlen(msg);
    for(i = 0; i < session->host_count; i++) {
        printf("host(%d) = %s \n", i, session->host_info[i]->hostname);
        printf("sent %d bytes \n", comlink_sendto_server(i, msg, len));
    }

    return 0;
}

/*****************************************************************************/

static void launcher_session_cleanup(launcher_session_t *session)
{
    while(session->host_count--) {
      if (session->host_info[session->host_count])
	  free(session->host_info[session->host_count]);
    }

    comlink_client_shutdown();
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
    launcher_session_t *session = &launcher_session;
     
    /* simple cmdline parser; use getopt instead */
    if (parse_cmdline(argc, argv, session) == -1) {
        fprintf(stderr, "invalid command options \n");
        exit(2);
    }

    /* reads hostfile returns the number of hosts */
    if (parse_hostfile(session->host_file) <= 0) {
        fprintf(stderr, "no hosts found in the hostfile \n");
        exit(2);
    }

    /* session setup */
    if (launcher_session_setup(session) != 0)
        exit(2);

    /* starts the remote execution; waits until done */
    launcher_session_start(session);

    /* done with the session; cleans-up */
    launcher_session_cleanup(session);
    
    return 0;
}

/*****************************************************************************/
