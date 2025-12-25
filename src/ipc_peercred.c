/**
 * ipc_peercred.c - PeerCred implementation (Linux)
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include "ipc_peercred.h"
#include <stdio.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/socket.h>

/* ucred defined in sys/socket.h with _GNU_SOURCE */

int ipc_peercred_get(int sockfd, ipc_peercred_t *cred) {
    if (!cred || sockfd < 0) {
        return -1;
    }
    
    struct ucred ucred;
    socklen_t len = sizeof(struct ucred);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) < 0) {
        perror("getsockopt(SO_PEERCRED)");
        return -1;
    }
    
    cred->uid = ucred.uid;
    cred->gid = ucred.gid;
    cred->pid = ucred.pid;
    
    return 0;
}

int ipc_peercred_is_authorized(int sockfd, uid_t server_uid) {
    ipc_peercred_t cred;
    
    if (ipc_peercred_get(sockfd, &cred) < 0) {
        return 0;  /* Failed to get creds = not authorized */
    }
    
    /* Policy: same user or root */
    if (cred.uid == server_uid || cred.uid == 0) {
        printf("[peercred] Authorized: uid=%d, gid=%d, pid=%d\n",
               cred.uid, cred.gid, cred.pid);
        return 1;
    }
    
    printf("[peercred] Denied: uid=%d (server=%d)\n", cred.uid, server_uid);
    return 0;
}

#else
/* Non-Linux platforms: stub implementation */
int ipc_peercred_get(int sockfd, ipc_peercred_t *cred) {
    (void)sockfd;
    (void)cred;
    fprintf(stderr, "[peercred] Not supported on this platform\n");
    return -1;
}

int ipc_peercred_is_authorized(int sockfd, uid_t server_uid) {
    (void)sockfd;
    (void)server_uid;
    /* On non-Linux: allow all (no auth) */
    return 1;
}
#endif
