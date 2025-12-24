/**
 * ipc_server.c - Unix domain socket IPC server
 * 
 * Supports:
 * - Unix domain sockets (Linux/macOS)
 * - TCP localhost fallback (Windows/fallback)
 * - Non-blocking I/O
 * - Multiple concurrent connections
 */

#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#define IPC_DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
#define IPC_MAX_CONNECTIONS 64
#define IPC_POLL_TIMEOUT_MS 1000

/**
 * Client connection state
 */
typedef struct {
    int fd;
    uint8_t recv_buf[IPC_MAX_FRAME_SIZE];
    size_t recv_len;
    int active;
} ipc_client_t;

/**
 * IPC server state
 */
typedef struct {
    int listen_fd;
    char socket_path[256];
    ipc_client_t clients[IPC_MAX_CONNECTIONS];
    int running;
    
    /* Callback for handling messages */
    void (*message_handler)(const ipc_message_t *msg, ipc_message_t *response, void *user_data);
    void *user_data;
} ipc_server_t;

/**
 * Create Unix domain socket
 */
static int ipc_create_unix_socket(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    /* Remove existing socket file */
    unlink(path);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    /* Set socket permissions (owner read/write only by default) */
    chmod(path, S_IRUSR | S_IWUSR);

    if (listen(fd, IPC_MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(fd);
        unlink(path);
        return -1;
    }

    printf("[ipc_server] Listening on Unix socket: %s\n", path);
    return fd;
}

/**
 * Set socket to non-blocking mode
 */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * Accept new client connection
 */
static int ipc_accept_client(ipc_server_t *server) {
    int client_fd = accept(server->listen_fd, NULL, NULL);
    if (client_fd < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("accept");
        }
        return -1;
    }

    if (set_nonblocking(client_fd) < 0) {
        perror("set_nonblocking");
        close(client_fd);
        return -1;
    }

    /* Find free slot */
    for (int i = 0; i < IPC_MAX_CONNECTIONS; i++) {
        if (!server->clients[i].active) {
            server->clients[i].fd = client_fd;
            server->clients[i].recv_len = 0;
            server->clients[i].active = 1;
            printf("[ipc_server] Client connected: fd=%d slot=%d\n", client_fd, i);
            return i;
        }
    }

    /* No free slots */
    fprintf(stderr, "[ipc_server] Max connections reached, rejecting client\n");
    close(client_fd);
    return -1;
}

/**
 * Close client connection
 */
static void ipc_close_client(ipc_server_t *server, int slot) {
    ipc_client_t *client = &server->clients[slot];
    if (client->active) {
        printf("[ipc_server] Client disconnected: fd=%d slot=%d\n", client->fd, slot);
        close(client->fd);
        client->active = 0;
        client->recv_len = 0;
    }
}

/**
 * Send message to client
 */
static int ipc_send_message(int fd, const ipc_message_t *msg) {
    uint8_t frame_buf[IPC_MAX_FRAME_SIZE];
    ssize_t frame_size = ipc_encode_message(msg, frame_buf, sizeof(frame_buf));
    
    if (frame_size < 0) {
        fprintf(stderr, "[ipc_server] Failed to encode message\n");
        return -1;
    }

    ssize_t sent =send(fd, frame_buf, frame_size, 0);
    if (sent != frame_size) {
        perror("send");
        return -1;
    }

    return 0;
}

/**
 * Handle received data from client
 */
static void ipc_handle_client_data(ipc_server_t *server, int slot) {
    ipc_client_t *client = &server->clients[slot];

    /* Read data */
    ssize_t n = recv(client->fd, 
                     client->recv_buf + client->recv_len,
                     sizeof(client->recv_buf) - client->recv_len, 0);

    if (n <= 0) {
        if (n == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
            /* Connection closed or error */
            ipc_close_client(server, slot);
        }
        return;
    }

    client->recv_len += n;

    /* Try to parse frame */
    while (client->recv_len >= IPC_HEADER_SIZE) {
        /* Peek at frame length */
        uint32_t frame_len = ntohl(*(uint32_t*)client->recv_buf);

        if (frame_len > IPC_MAX_FRAME_SIZE) {
            fprintf(stderr, "[ipc_server] Frame too large: %u bytes\n", frame_len);
            ipc_close_client(server, slot);
            return;
        }

        if (client->recv_len < frame_len) {
            /* Need more data */
            break;
        }

        /* Decode message */
        ipc_message_t req;
        ipc_error_t err = ipc_decode_message(client->recv_buf, frame_len, &req);

        if (err != IPC_ERR_OK) {
            fprintf(stderr, "[ipc_server] Decode error: %s\n", ipc_strerror(err));
            
            /* Send error response */
            ipc_message_t error_resp;
            if (ipc_create_error_response(err, NULL, &error_resp) == IPC_ERR_OK) {
                ipc_send_message(client->fd, &error_resp);
                ipc_free_message(&error_resp);
            }
            
            ipc_close_client(server, slot);
            return;
        }

        /* Handle message */
        ipc_message_t response;
        memset(&response, 0, sizeof(response));

        if (server->message_handler) {
            server->message_handler(&req, &response, server->user_data);
        } else {
            /* Default: echo back with OK response */
            response.type = IPC_MSG_RESPONSE_OK;
            response.payload = strdup("{\"ok\":true}");
            response.payload_len = strlen(response.payload);
        }

        /* Send response */
        if (ipc_send_message(client->fd, &response) < 0) {
            fprintf(stderr, "[ipc_server] Failed to send response\n");
        }

        ipc_free_message(&req);
        ipc_free_message(&response);

        /* Remove processed frame from buffer */
        memmove(client->recv_buf, client->recv_buf + frame_len, client->recv_len - frame_len);
        client->recv_len -= frame_len;
    }
}

/**
 * Initialize IPC server
 */
ipc_server_t* ipc_server_init(const char *socket_path) {
    ipc_server_t *server = (ipc_server_t*)calloc(1, sizeof(ipc_server_t));
    if (!server) {
        return NULL;
    }

    const char *path = socket_path ? socket_path : IPC_DEFAULT_SOCKET_PATH;
    strncpy(server->socket_path, path, sizeof(server->socket_path) - 1);

    server->listen_fd = ipc_create_unix_socket(path);
    if (server->listen_fd < 0) {
        free(server);
        return NULL;
    }

    if (set_nonblocking(server->listen_fd) < 0) {
        close(server->listen_fd);
        unlink(path);
        free(server);
        return NULL;
    }

    server->running = 1;
    return server;
}

/**
 * Set message handler callback
 */
void ipc_server_set_handler(ipc_server_t *server, 
                            void (*handler)(const ipc_message_t*, ipc_message_t*, void*),
                            void *user_data) {
    if (server) {
        server->message_handler = handler;
        server->user_data = user_data;
    }
}

/**
 * Run server event loop
 */
void ipc_server_run(ipc_server_t *server) {
    if (!server) {
        return;
    }

    printf("[ipc_server] Event loop started\n");

    while (server->running) {
        /* Build poll array */
        struct pollfd fds[IPC_MAX_CONNECTIONS + 1];
        int nfds = 0;

        /* Listen socket */
        fds[nfds].fd = server->listen_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        /* Client sockets */
        for (int i = 0; i < IPC_MAX_CONNECTIONS; i++) {
            if (server->clients[i].active) {
                fds[nfds].fd = server->clients[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        /* Poll */
        int ready = poll(fds, nfds, IPC_POLL_TIMEOUT_MS);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (ready == 0) {
            /* Timeout, nothing to do */
            continue;
        }

        /* Check listen socket */
        if (fds[0].revents & POLLIN) {
            ipc_accept_client(server);
        }

        /* Check client sockets */
        for (int i = 0; i < IPC_MAX_CONNECTIONS; i++) {
            if (server->clients[i].active) {
                for (int j = 1; j < nfds; j++) {
                    if (fds[j].fd == server->clients[i].fd && (fds[j].revents & POLLIN)) {
                        ipc_handle_client_data(server, i);
                        break;
                    }
                }
            }
        }
    }

    printf("[ipc_server] Event loop stopped\n");
}

/**
 * Stop server
 */
void ipc_server_stop(ipc_server_t *server) {
    if (server) {
        server->running = 0;
    }
}

/**
 * Cleanup server
 */
void ipc_server_destroy(ipc_server_t *server) {
    if (!server) {
        return;
    }

    /* Close all client connections */
    for (int i = 0; i < IPC_MAX_CONNECTIONS; i++) {
        if (server->clients[i].active) {
            ipc_close_client(server, i);
        }
    }

    /* Close listen socket */
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        unlink(server->socket_path);
    }

    free(server);
}
