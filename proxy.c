/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 */

/* Some useful includes to help you get started */

#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <http_parser.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)
#define HOSTLEN 256
#define SERVLEN 8

/* Typedef for convenience */
typedef struct sockaddr SA;

/* Information about a connected client. */
typedef struct {
    struct sockaddr_in addr; // Socket address
    socklen_t addrlen;       // Socket address length
    int connfd;              // Client connection file descriptor
    char host[HOSTLEN];      // Client host
    char serv[SERVLEN];      // Client service (port)
} client_info;

typedef void (*sighandler_t)(int);

/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
static const char *header_user_agent = "Mozilla/5.0"
                                       " (X11; Linux x86_64; rv:3.10.0)"
                                       " Gecko/20191101 Firefox/63.0.1";

sighandler_t signal(int signum, sighandler_t handler);

/*
 * proxy
 */
void serve(client_info *client) {

    rio_t rio;
    rio_readinitb(&rio, client->connfd);

    /* Read request line */
    char buf[MAXLINE];
    parser_t *r_line = parser_new();

    if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
        fprintf(stderr, "request line empty\n");
        return;
    }
    // how to loop through client request and headers?
    while (strncmp(buf, "\r\n", sizeof("\r\n")) != 0) {
        parser_state r_state = parser_parse_line(r_line, buf);

        if (r_state == ERROR) {
            fprintf(stderr, "404 bad request\n");
            return;
        }

        rio_readlineb(&rio, buf, MAXLINE);
    }
    fprintf(stderr, "finished parsing\n");
    /* Check that the method is GET */
    // ????????
    const char *method;
    const char *host;
    const char *port;
    const char *path;
    if (parser_retrieve(r_line, PATH, &path) != 0) {
        fprintf(stderr, "failed to get path\n");
        return;
    }
    if (parser_retrieve(r_line, METHOD, &method) != 0) {
        fprintf(stderr, "failed to get method\n");
        return;
    }
    if (parser_retrieve(r_line, HOST, &host) != 0) {
        fprintf(stderr, "failed to get host\n");
        return;
    }
    if (parser_retrieve(r_line, PORT, &port) != 0) {
        fprintf(stderr, "failed to get port\n");
        return;
    }

    if (strncmp(method, "GET", 3) != 0) {
        fprintf(stderr, "501 Not Implemented\n");
        rio_writen(client->connfd,
                   "HTTP/1.0 501 Not Implemented\r\nContent-Length: 0\r\n\r\n",
                   MAXLINE);

        return;
    }

    // do we need an equivalent to requesthdrs?
    int clientfd;
    if ((clientfd = open_clientfd(host, port)) == -1) {
        fprintf(stderr, "open_clientfd failed\n");
        return;
    }

    // request line
    // don't use maxline for parsing
    char request[MAXLINE];
    strncat(request, method, strlen(method));
    strncat(request, " ", MAXLINE);
    strncat(request, path, strlen(path));
    strncat(request, " ", MAXLINE);
    strncat(request, "HTTP/1.0\r\n", MAXLINE);

    // Host
    strncat(request, "Host: ", MAXLINE);
    strncat(request, host, strlen(host));
    strncat(request, ":", MAXLINE);
    strncat(request, port, strlen(port));
    strncat(request, "\r\n", MAXLINE);

    // User-Agent
    strncat(request, "User-Agent: ", MAXLINE);
    strncat(request, header_user_agent, strlen(header_user_agent));
    strncat(request, "\r\n", MAXLINE);

    // connection
    strncat(request, "Connection: close\r\n", MAXLINE);

    // Proxy-Connection
    strncat(request, "Proxy-Connection: close\r\n", MAXLINE);

    // adding additional headers
    header_t *header;
    while ((header = parser_retrieve_next_header(r_line)) != NULL) {
        if (strncmp(header->name, "Host", 4) != 0 &&
            strncmp(header->name, "User-Agent", 10) != 0 &&
            strncmp(header->name, "Connection", 10) != 0 &&
            strncmp(header->name, "Proxy-Connection", 16) != 0) {
            strncat(request, header->name, strlen(header->name));
            strncat(request, ": ", 2);
            strncat(request, header->value, strlen(header->value));
            strncat(request, "\r\n", 4);
        }
    }
    fprintf(stderr, "finished getting headers\n");
    // \r\n
    strncat(request, "\r\n", MAXLINE);
    fprintf(stderr, "request to be sent to server:\n");
    fprintf(stderr, request);
    fprintf(stderr, "size of request = %ld \n", sizeof(request));

    // write request block to clientfd
    int descriptor = rio_writen(clientfd, request, sizeof(request));

    fprintf(stderr, "finished writing request to clientfd\n");

    if (descriptor == -1) {
        fprintf(stderr, "rio_writen failed\n");
        return;
    } else if (descriptor == 0) {
        fprintf(stderr, "rio_writen reached EOF\n");
        return;
    }

    // read the response using a while loop and send it back
    // rio_readn(clientfd, buf, MAXLINE);
    rio_t new;
    char destination[MAX_OBJECT_SIZE] = {""};
    int total = 0;
    size_t size;
    rio_readinitb(&new, clientfd);
    fprintf(stderr, "clientfd: %d \n", clientfd);
    while ((size = rio_readnb(&new, buf, MAXLINE)) > 0) {
        rio_writen(client->connfd, buf, size);
        if ((total + size) > MAX_OBJECT_SIZE) {
            fprintf(stderr, "(total + size) > MAX_OBJECT_SIZE\n");
        } else {
            memcpy(destination + total, buf, size);
        }
        total += size;
        memset(buf, 0, MAXLINE);
    }
    // rio_writen(client->connfd, "\r\n", MAXLINE);
    memset(request, 0, MAXLINE);
    close(clientfd);
    // free(request);
    return;
}

void *thread(void *vargp) {
    pthread_detach(pthread_self());
    client_info *client = (client_info *)vargp;
    serve(client);
    close(client->connfd);
    return NULL;
}

// secnod part
// create thread in main
// before cache
// revert checkpoint(sequencial) then add cache
// don't use linked list, use array
// pass c1-c5 with sequencial

// Part 2: what to lock?
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    int listenfd;
    pthread_t tid;
    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Failed to listen on port: %s\n", argv[1]);
        exit(1);
    }

    while (1) {
        /* Allocate space on the stack for client info */
        // malloc?
        client_info client_data;
        client_info *client = malloc(sizeof(client_data));

        /* Initialize the length of the address */
        client->addrlen = sizeof(client->addr);

        /* accept() will block until a client connects to the port */
        client->connfd =
            accept(listenfd, (SA *)&client->addr, &client->addrlen);
        if (client->connfd < 0) {
            perror("accept");
            continue;
        }

        /* Connection is established; serve client */
        pthread_create(&tid, NULL, thread, client);
    }
    return 0;
}
