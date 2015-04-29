/*
 * comlink: communication interface using tcp sockets
 */

/* comlink.c  -- server and client side implementations */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#include "comlink.h"
#include "common.h"

/*****************************************************************************/

static comlink_t comlink;

/*****************************************************************************/
/* get the comlink params instance */

static comlink_params_t * get_comlink_params(void)
{
    return &comlink.params;
}

/*****************************************************************************/
/* get the server instance */

static comlink_server_t * get_comlink_server(void)
{
    return &comlink.server;
}

/*****************************************************************************/
/* get the client instance */

static comlink_client_t * get_comlink_client(void)
{
    return &comlink.client;
}

/*****************************************************************************/
/* server side implementation */

static int comlink_server_init(comlink_params_t *cl_params)
{
    int i;

    comlink_server_t *cl_server = get_comlink_server();

    if (cl_params->init_done)
        return 0;

    cl_server->skt_listen = -1;
    cl_server->nr_clients = 0;

    for(i = 0; i < MAX_CONNECTIONS; i++)
        cl_server->skt_clients[i] = -1;

    comlink.params = *cl_params;
    cl_params->init_done = 1;
    
    return 0;
}

/*****************************************************************************/

static void comlink_server_cleanup(void)
{
    int i;

    comlink_server_t *server = get_comlink_server();

    if (server->skt_listen != -1)
        close(server->skt_listen);

    for(i = 0; i < MAX_CONNECTIONS; i++) {
        if (server->skt_clients[i] != -1)
            close(server->skt_clients[i]);
    }
}

/*****************************************************************************/
/* returns the length for subsequent read */

static int comlink_read_header(int fd, comlink_header_t *header, int len)
{
    int ret;
    
    ret = recv(fd, (char *)header, len, 0);
    if (ret == -1) {
        fprintf(stderr, "server: recv error, %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    if (ret == 0) {
        if (comlink.params.shutdown_cb != NULL)
            comlink.params.shutdown_cb(fd);

        return -1;
    }

    header->type = ntohl(header->type);
    header->len = ntohl(header->len);
    
    return header->len;
}

/*****************************************************************************/
/* read the actual comlink data */

static int comlink_read_data(int fd, char *buf, int len)
{
    int ret;

    memset(buf, 0, len);
    ret = recv(fd, buf, len, 0);
    if (ret == -1) {
        fprintf(stderr, "server: recv error, %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    if (ret == 0) {
        if (comlink.params.shutdown_cb != NULL)
            comlink.params.shutdown_cb(fd);

        return -1;
    }

    return ret;
}

/*****************************************************************************/
/* main server task; busy loop */

static int comlink_server_task(int fd, struct sockaddr_in *addr)
{
    int ret = 0;
    int rx_len;
    comlink_header_t header;
    
    char *buf = comlink.params.buffer;
    int buf_len = comlink.params.buf_len;
    
    comlink_params_t *cl = get_comlink_params();
    
    if (cl->rx_len == 0) { /* start of a message */
    /* read header to get type and length */
        if ((ret = comlink_read_header(fd, &header,
                sizeof(comlink_header_t))) == -1)
            return -1;

        /* now we know the len to read */
        cl->rx_len = ret;
    }

    rx_len = comlink_read_data(fd, buf, cl->rx_len);

    /* normal operation, data received; pas it to the callback */
    if (comlink.params.receive_cb != NULL && rx_len > 0)
        comlink.params.receive_cb(fd, addr, header.type, buf, buf_len);

    cl->rx_len = 0;    
    
    return 0;
}

/*****************************************************************************/
/* setup the server instance; creates socket and listens on it */

int comlink_server_setup(comlink_params_t *cl_params)
{
    int fd;
    int optval = 1;
    struct sockaddr_in skt_addr;

    comlink_server_t *cl_server = get_comlink_server();

    if (comlink_server_init(cl_params)) {
        fprintf(stderr, "server: setup failed \n");
        abort();
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "server: error in listener socket, %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    cl_server->skt_listen = fd;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
            &optval, sizeof(optval)) == -1) {
        fprintf(stderr, "server: error in setsockopt, %s(%d) \n",
            strerror(errno), errno);
        comlink_server_cleanup();
        return -1;
    }

    memset(&skt_addr, 0, sizeof(struct sockaddr_in));
    skt_addr.sin_family = AF_INET;
    skt_addr.sin_addr.s_addr = htonl(cl_params->local_ip);
    skt_addr.sin_port = htons(cl_params->local_port);
    if (bind(fd, (struct sockaddr *)&skt_addr,
            sizeof(struct sockaddr_in)) == -1) {
        fprintf(stderr, "server: bind error, %s(%d) \n",
            strerror(errno), errno);
        comlink_server_cleanup();
        return -1;
    }

    if (listen(fd, 3) == -1) {
        fprintf(stderr, "server: error in listen, %s(%d) \n",
            strerror(errno), errno);
        comlink_server_cleanup();
        return -1;
    }

    comlink.comlink_break = 0;

    return 0;
}

/*****************************************************************************/
/* exposed function to start the server task */

int comlink_server_start(void)
{
    int fd;
    socklen_t skt_len;
    struct sockaddr_in skt_addr;

    comlink_server_t *cl_server = get_comlink_server();
    
    skt_len = sizeof(sizeof(struct sockaddr_in));
    fd = accept(cl_server->skt_listen, (struct sockaddr *)&skt_addr,
            &skt_len);

    /* store the fd for replying later */        
    cl_server->skt_clients[cl_server->nr_clients] = fd;
    cl_server->nr_clients += 1;

    fprintf(stdout, "server: new connection from %08x:%05d \n",
        ntohl(skt_addr.sin_addr.s_addr), ntohs(skt_addr.sin_port));

    while(comlink.comlink_break == 0) {
        if (comlink_server_task(fd, &skt_addr) == -1)
            break;
        usleep(200 * 1000); /*200ms polling period */
    }

    return 0;
}

/*****************************************************************************/

int comlink_server_shutdown(void)
{
    fprintf(stdout, "server: shutdown, cleaning-up \n");
    if (comlink.comlink_break == 0) {
        comlink.comlink_break = 1;
        comlink_server_cleanup();
    }

    return 0;
}

/*****************************************************************************/
/* client side implementation */

static int comlink_client_init(comlink_params_t *cl_params)
{
    int i;
    
    comlink_client_t *cl_client = get_comlink_client();

    if (cl_params->init_done)
        return 0;
    
    cl_client->nr_conns = 0;
    for(i = 0; i < MAX_CONNECTIONS; i++)
        cl_client->skt_conns[i] = -1;

    comlink.params = *cl_params;
    cl_params->init_done = 1;
    FD_ZERO(&cl_client->rd_fds);
    FD_ZERO(&cl_client->t_fds);
    
    return 0;
}

/*****************************************************************************/

static void comlink_client_cleanup(void)
{
    int i;
    
    comlink_client_t *cl_client = get_comlink_client();

    for(i = 0; i < cl_client->nr_conns; i++) {
        if (cl_client->skt_conns[i] != -1)
            close(cl_client->skt_conns[i]);
    }
}

/*****************************************************************************/
/* add new connections to the read fd_set */

static void update_client_fdset(comlink_client_t *cl, int fd)
{
    if (fd > MAX_CONNECTIONS) {
        fprintf(stderr,
            "comlink: reached the connection limit at client\n");
        return;
    }

    FD_SET(fd, &cl->rd_fds);

    /* update the max fd */
    if (cl->max_fds <= fd)
        cl->max_fds = fd + 1;
}

/*****************************************************************************/

static int comlink_client_task(void)
{
    int i;
    int rx_len;
    int buf_len = comlink.params.buf_len;
    char *buf = comlink.params.buffer;
    
    comlink_client_t *cl = get_comlink_client();

    memcpy(&cl->t_fds, &cl->rd_fds, sizeof(fd_set));
    if (select(cl->max_fds, &cl->t_fds, NULL, NULL, NULL) == -1) {
        fprintf(stderr, "comlink: select, %s(%d) \n",
            strerror(errno), errno);
        return -1;    
    }

    for(i = 0; i < cl->nr_conns; i++) {
        if(FD_ISSET(cl->skt_conns[i], &cl->t_fds)) {
            rx_len = comlink_read_data(cl->skt_conns[i], buf, buf_len);
            if (comlink.params.receive_cb != NULL && rx_len > 0)
                /* text status message; FIX */
                comlink.params.receive_cb(cl->skt_conns[i],
                    NULL, STATUS_MESSAGE, buf, buf_len);
        }
    }
   
    return 0;
}

/*****************************************************************************/

int comlink_client_setup(comlink_params_t *cl_params)
{
    int fd;
    struct sockaddr_in skt_addr;
    
    comlink_client_t *cl_client = get_comlink_client();

    if (comlink_client_init(cl_params)) {
        fprintf(stderr, "client: setup failed \n");
        abort();
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "client: error opening connect socket, %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    memset(&skt_addr, 0, sizeof(struct sockaddr_in));
    skt_addr.sin_family = AF_INET;
    skt_addr.sin_addr.s_addr = htonl(cl_params->remote_ip);
    skt_addr.sin_port = htons(cl_params->remote_port);
    if (connect(fd, (struct sockaddr *)&skt_addr,
            sizeof(struct sockaddr_in)) == -1) {
        fprintf(stderr, "client: connect error, %s(%d) \n",
            strerror(errno), errno);
        comlink_client_cleanup();
        return -1;
    }

    /* new connection, store it for receiving the replies */
    cl_client->skt_conns[cl_client->nr_conns] = fd;
    cl_client->nr_conns += 1; /* TODO: find a bettwer way for the index */
    update_client_fdset(cl_client, fd);

    comlink.comlink_break = 0;
        
    return fd;
}

/*****************************************************************************/

int comlink_client_start(void)
{
    while(comlink.comlink_break == 0) {
        comlink_client_task();
        usleep(200 * 1000); /*200ms polling period */
    }

    return 0;
}

/*****************************************************************************/

int comlink_client_shutdown(void)
{
    fprintf(stdout, "client: shutdown, cleaning-up \n");
    if (comlink.comlink_break == 0)
        comlink.comlink_break = 1;
        
    comlink_client_cleanup();

    return 0;
}

/*****************************************************************************/

void comlink_client_close(int fd)
{
    comlink_client_t *cl = get_comlink_client();

    close(fd);
    FD_CLR(fd, &cl->rd_fds);
}

/*****************************************************************************/

int comlink_sendto_server(int con_index, comlink_header_t *hdr,
        char *buf, int buf_len)
{
    int ret;
    comlink_header_t header;
    
    comlink_client_t *cl = get_comlink_client();

    if (con_index > cl->nr_conns) {
        fprintf(stderr, "client: invalid con_index \n");
        return -1;
    }

    header.type = ntohl(hdr->type);
    header.len = ntohl(hdr->len);
    
    ret = send(cl->skt_conns[con_index], (void *)&header,
            sizeof(comlink_header_t), 0);
    if (ret == -1) {
        fprintf(stderr, "client: send failed %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }
    
    ret = send(cl->skt_conns[con_index], buf, buf_len, 0);
    if (ret != buf_len) {
        fprintf(stderr, "client: send failed %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    return ret;
}

/*****************************************************************************/

int hostname_to_netaddr(char *hostname, struct sockaddr *addr)
{
    struct addrinfo *res;

    if (getaddrinfo(hostname, NULL, NULL, &res) != 0) {
        fprintf(stderr, "error in getaddrinfo: %s(%d) \n",
            strerror(errno), errno);
        return -1;
    }

    *addr = *(struct sockaddr *)res->ai_addr;
    freeaddrinfo(res);

    return 0;
}

/*****************************************************************************/
