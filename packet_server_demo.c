/*
 * packet_server_demo.c
 *
 * A simple TCP server example that receives a request packet,
 * processes it through layered stages matching a Docker-based stack
 * (frontend -> nginx -> Spring API -> PostgreSQL -> response), and
 * returns an HTTP response.
 *
 * Compile:
 *   gcc packet_server_demo.c -o packet_server_demo
 *
 * Run:
 *   ./packet_server_demo
 *
 * Test with:
 *   curl http://127.0.0.1:8080/user?name=alice
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9090
#define BUFFER_SIZE 4096

struct UserRecord {
    const char *name;
    const char *status;
};

static const struct UserRecord db_rows[] = {
    {"alice", "active"},
    {"bob", "inactive"},
};

static int db_lookup(const char *name, char *status_out, size_t status_size) {
    size_t i;
    for (i = 0; i < sizeof(db_rows) / sizeof(db_rows[0]); ++i) {
        if (strcmp(db_rows[i].name, name) == 0) {
            snprintf(status_out, status_size, "%s", db_rows[i].status);
            return 1;
        }
    }
    return 0;
}

static void process_request(const char *request, char *response, size_t response_size) {
    char method[16] = {0};
    char path[256] = {0};
    char query[256] = {0};
    char name[64] = {0};
    char db_status[32] = "not-found";
    char *query_start = NULL;
    const char *body = "";

    printf("[TRACE] Layer 1: React frontend/client sent an HTTP request\n");
    printf("[TRACE] Layer 2: nginx-proxy received the request and passed it to the app stack\n");
    printf("[TRACE] Layer 3: spring-api socket received the TCP payload from the kernel stack\n");
    printf("[TRACE] Layer 4: HTTP parser extracted bytes -> %s\n", request);

    if (sscanf(request, "%15s %255s", method, path) != 2) {
        printf("[TRACE] Layer 4: parse failure, returning 400\n");
        snprintf(response, response_size,
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: text/plain\r\n"
                 "Connection: close\r\n\r\n"
                 "Bad request");
        return;
    }

    printf("[TRACE] Layer 5: Spring controller received method=%s path=%s\n", method, path);
    printf("[TRACE] Layer 6: MVC dispatcher routed the request to the controller logic\n");

    query_start = strchr(path, '?');
    if (query_start != NULL) {
        size_t path_len = (size_t)(query_start - path);
        if (path_len > 0 && path_len < sizeof(path)) {
            memcpy(query, query_start + 1, sizeof(query) - 1);
            query[sizeof(query) - 1] = '\0';
        }
        if (path_len < sizeof(path)) {
            memcpy(path, path, path_len);
            path[path_len] = '\0';
        }
    }

    if (strcmp(path, "/user") == 0) {
        printf("[TRACE] Layer 7: Controller accepted route /user\n");
        printf("[TRACE] Layer 8: Spring service layer prepared the business logic\n");
        if (sscanf(query, "name=%63s", name) == 1) {
            printf("[TRACE] Layer 9: Service passed parameter name=%s to repository/DAO\n", name);
            if (db_lookup(name, db_status, sizeof(db_status))) {
                printf("[TRACE] Layer 10: Repository queried the database and returned -> %s\n", db_status);
                printf("[TRACE] Layer 11: DTO/entity object created from repository data\n");
                printf("[TRACE] Layer 12: JDBC/Postgres driver completed the DB interaction\n");
                printf("[TRACE] Layer 13: Response assembled and returned through spring-api -> nginx-proxy -> client\n");
                snprintf(response, response_size,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Connection: close\r\n\r\n"
                         "Trace: React frontend -> nginx-proxy -> spring-api -> controller -> "
                         "service -> repository -> DTO/entity -> postgres-db -> response.\n"
                         "User=%s Status=%s",
                         name, db_status);
                return;
            }
        }

        printf("[TRACE] Layer 10: no matching record found in the database\n");
        body = "User not found in DB";
    } else {
        printf("[TRACE] Layer 7: no matching controller for route %s\n", path);
        body = "Route not found";
    }

    printf("[TRACE] Layer 13: assembling error response for the client\n");
    snprintf(response, response_size,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             body);
}

static int create_listener(int port) {
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int main(void) {
    int server_fd;
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    ssize_t received;

    server_fd = create_listener(PORT);
    if (server_fd < 0) {
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[TRACE] Layer 2: nginx-proxy accepted the connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        memset(buffer, 0, sizeof(buffer));
        received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            printf("[TRACE] received %zd bytes from kernel socket buffer\n", received);
            printf("[TRACE] raw request bytes:\n%s\n", buffer);
            process_request(buffer, response, sizeof(response));
            printf("[TRACE] sending response to client\n");
            send(client_fd, response, strlen(response), 0);
        } else if (received == 0) {
            printf("Client disconnected.\n");
        } else {
            perror("recv");
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
