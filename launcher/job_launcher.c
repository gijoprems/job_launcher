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

/*****************************************************************************/

launcher_session_t launcher_session;

/*****************************************************************************/

static int usage(char *program)
{
    fprintf(stderr, "\n%s: -np <instances> -hostfile <hostfile>"
            " <path-to-executable> \n", program);

    return 0;
}

/*****************************************************************************/

int alloc_host_entry(launcher_session_t *session, int host_index)
{
    int k;

    k = sizeof(host_info_t);
    session->host_info[host_index] = (host_info_t *)malloc(k);
    
    if (session->host_info[host_index] == NULL) {
        fprintf(stderr, "error allocating session, %s(%d) \n",
	        strerror(errno), errno);
        return -1;  
    }

    return 0;
} 

/*****************************************************************************/

void cleanup_session(launcher_session_t *session)
{
    while(session->host_count--) {
      if (session->host_info[session->host_count])
	  free(session->host_info[session->host_count]);
    }
}

/*****************************************************************************/

/* returns the number of hosts */
static int parse_hostfile(char *file)
{
    int count = 0, status = 0; 
    FILE *fp = NULL;
    char buffer[MAX_HOSTNAME_LEN];
    launcher_session_t *session = &launcher_session;
    
    if ((fp = fopen(file, "rb")) == NULL) {
        fprintf(stderr, "error opening hostfile %s: %s(%d) \n", 
                file, strerror(errno), errno);	
        exit(2);
    }
    session->host_count = 0;
    
    while(!feof(fp)) {
        memset(buffer, '\n', MAX_HOSTNAME_LEN);
	
        if (fgets(buffer, MAX_HOSTNAME_LEN, fp) != NULL) {
	    status = alloc_host_entry(session, session->host_count);
	    
	    if (status == 0)
	        count = session->host_count;
	        strcpy(session->host_info[count]->hostname, buffer);
		printf("host name: %s, count = %d \n",
		        session->host_info[session->host_count]->hostname,
		        session->host_count);
	        session->host_count += 1;
	}
    }
    fclose(fp);
    count = session->host_count;
    
    /* using count for count and index; hence +1 */
    return count;
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
    char hostfile[256];
    char executable[256];
    int instances = 0;
    launcher_session_t *session = &launcher_session;
     
    /* simple cmdline parser; use getopt instead */
    if (argc < MAX_CMD_ARGS) {
        usage(argv[0]);
        exit(2);
    }

    if (strncmp(argv[1], "-np", 3) == 0)
        instances = atoi(argv[2]); 

    if (strncmp(argv[3], "-hostfile", 9) == 0)
        strcpy(hostfile, argv[4]);

    strcpy(executable, argv[5]);
    
    if ((instances < 0 && instances > MAX_INSTANCES) ||
            strncmp(hostfile, "", 1) == 0 || 
            strncmp(executable, "", 1) == 0) {      
        fprintf(stderr, "invalid command options \n");
        exit(2);
    }
    fprintf(stdout, "instances = %d, hostfile = %s, exec = %s \n", 
            instances, hostfile, executable);

    /* reads hostfile returns the number of hosts */
    if (parse_hostfile(hostfile) <= 0) {
        fprintf(stderr, "no hosts found in the hostfile \n");
        exit(2);
    }

    cleanup_session(session);
    return 0;
}

/*****************************************************************************/
