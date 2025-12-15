#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *port_env = getenv("GATEWAY_PORT");
    int rc = http_server_run(port_env);
    if (rc != 0) {
        fprintf(stderr, "C-Gateway exited with code %d\n", rc);
    }
    return rc;
}
