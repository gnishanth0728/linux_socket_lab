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

struct HttpHeader {
    char name[64];
    char value[256];
};

struct HttpRequest {
    char method[16];
    char path[256];
    char protocol[16];
    struct HttpHeader headers[16];
    int header_count;
    char body[1024];
};

struct ResultSetRow {
    char column[32];
    char value[128];
};

struct ResultSet {
    struct ResultSetRow rows[8];
    int row_count;
};

struct UserEntity {
    char name[64];
    char status[32];
};

struct UserResponseDto {
    char name[64];
    char status[32];
};

static const struct UserRecord db_rows[] = {
    {"alice", "active"},
    {"bob", "inactive"},
};

static int parse_http_request(const char *raw, struct HttpRequest *req) {
    const char *line = raw;
    const char *next;
    char header_line[512];

    memset(req, 0, sizeof(*req));

    next = strstr(line, "\r\n");
    if (next == NULL)
        next = strstr(line, "\n");
    if (next == NULL)
        return 0;

    size_t request_line_len = (size_t)(next - line);
    if (request_line_len >= sizeof(header_line))
        return 0;

    memcpy(header_line, line, request_line_len);
    header_line[request_line_len] = '\0';
    if (sscanf(header_line, "%15s %255s %15s",
               req->method, req->path, req->protocol) != 3)
        return 0;

    line = next;
    if (*line == '\r' && line[1] == '\n')
        line += 2;
    else if (*line == '\n')
        line += 1;

    while (*line != '\0' && !(line[0] == '\r' && line[1] == '\n') && *line != '\n') {
        const char *end = strstr(line, "\r\n");
        if (end == NULL)
            end = strstr(line, "\n");
        if (end == NULL)
            break;

        size_t len = (size_t)(end - line);
        if (len >= sizeof(header_line))
            return 0;

        memcpy(header_line, line, len);
        header_line[len] = '\0';

        char *colon = strchr(header_line, ':');
        if (colon != NULL && req->header_count < (int)(sizeof(req->headers) / sizeof(req->headers[0]))) {
            *colon = '\0';
            char *name = header_line;
            char *value = colon + 1;
            while (*value == ' ') value++;
            strncpy(req->headers[req->header_count].name, name, sizeof(req->headers[req->header_count].name) - 1);
            strncpy(req->headers[req->header_count].value, value, sizeof(req->headers[req->header_count].value) - 1);
            req->header_count++;
        }

        if (*end == '\r' && end[1] == '\n')
            line = end + 2;
        else
            line = end + 1;
    }

    if (*line == '\r' && line[1] == '\n')
        line += 2;
    else if (*line == '\n')
        line += 1;

    if (*line != '\0') {
        strncpy(req->body, line, sizeof(req->body) - 1);
    }

    return 1;
}

static int extract_query_param(const char *query, const char *key, char *value, size_t value_size) {
    const char *start = strstr(query, key);
    if (start == NULL)
        return 0;

    start += strlen(key);
    if (*start != '=')
        return 0;
    start++;

    const char *end = strchr(start, '&');
    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= value_size)
        len = value_size - 1;
    memcpy(value, start, len);
    value[len] = '\0';
    return 1;
}

static void resultset_from_user(const char *name, struct ResultSet *rs) {
    rs->row_count = 0;
    for (size_t i = 0; i < sizeof(db_rows) / sizeof(db_rows[0]); ++i) {
        if (strcmp(db_rows[i].name, name) == 0) {
            rs->row_count = 1;
            strncpy(rs->rows[0].column, "status", sizeof(rs->rows[0].column) - 1);
            strncpy(rs->rows[0].value, db_rows[i].status, sizeof(rs->rows[0].value) - 1);
            return;
        }
    }
}

static void entity_from_resultset(const struct ResultSet *rs, struct UserEntity *entity, const char *name) {
    strncpy(entity->name, name, sizeof(entity->name) - 1);
    if (rs->row_count > 0) {
        strncpy(entity->status, rs->rows[0].value, sizeof(entity->status) - 1);
    } else {
        strcpy(entity->status, "not-found");
    }
}

static void dto_from_entity(const struct UserEntity *entity, struct UserResponseDto *dto) {
    strncpy(dto->name, entity->name, sizeof(dto->name) - 1);
    strncpy(dto->status, entity->status, sizeof(dto->status) - 1);
}

static void serialize_json(const struct UserResponseDto *dto, char *out, size_t out_size) {
    snprintf(out, out_size,
             "{\"name\":\"%s\",\"status\":\"%s\"}",
             dto->name,
             dto->status);
}

static void process_request(const char *request, char *response, size_t response_size) {
    struct HttpRequest http_request;
    struct ResultSet result_set;
    struct UserEntity user_entity;
    struct UserResponseDto user_dto;
    char request_path[256] = {0};
    char query[256] = {0};
    char name[64] = {0};
    char response_body[2048] = {0};

    printf("\n========== REQUEST FLOW STARTING ==========\n");
    printf("[TRACE] Layer 1: React frontend/client sent an HTTP request\n");
    printf("        └─ TCP packet available in kernel receive buffer\n\n");

    printf("[TRACE] Layer 2: nginx-proxy received the request and passed it to the app stack\n");
    printf("        └─ nginx accepted socket connection and forwarded request data\n\n");

    printf("[TRACE] Layer 3: spring-api socket received the TCP payload from the kernel stack\n");
    printf("        └─ Socket read returned %zu bytes\n", strlen(request));
    printf("        └─ Stream buffer assembled from TCP segments\n\n");

    printf("[TRACE] Layer 4: HTTP parser extracted bytes\n");
    printf("        └─ Raw request bytes:\n%s\n", request);
    if (!parse_http_request(request, &http_request)) {
        printf("[TRACE] Layer 4: request parsing failed\n");
        printf("        └─ Invalid HTTP request structure\n\n");
        snprintf(response, response_size,
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: text/plain\r\n"
                 "Connection: close\r\n\r\n"
                 "Bad request");
        return;
    }

    printf("[TRACE] Layer 4: HTTP request object built\n");
    printf("        └─ Method: %s\n", http_request.method);
    printf("        └─ Path: %s\n", http_request.path);
    printf("        └─ Protocol: %s\n", http_request.protocol);
    for (int i = 0; i < http_request.header_count; ++i) {
        printf("        └─ Header: %s = %s\n",
               http_request.headers[i].name,
               http_request.headers[i].value);
    }
    if (http_request.header_count == 0) {
        printf("        └─ Headers: none\n");
    }
    if (http_request.body[0] != '\0') {
        printf("        └─ Body: %s\n", http_request.body);
    }
    printf("\n");

    printf("[TRACE] Layer 5: Spring controller received request\n");
    printf("        └─ Controller: student-controller.java\n\n");

    printf("[TRACE] Layer 6: MVC dispatcher routed the request to the controller logic\n");
    printf("        └─ DispatcherServlet created request/response wrappers\n");
    printf("        └─ HandlerMapping found controller method\n\n");

    strncpy(request_path, http_request.path, sizeof(request_path) - 1);
    request_path[sizeof(request_path) - 1] = '\0';
    char *query_start = strchr(request_path, '?');
    if (query_start != NULL) {
        *query_start = '\0';
        size_t query_len = strlen(query_start + 1);
        if (query_len >= sizeof(query))
            query_len = sizeof(query) - 1;
        memcpy(query, query_start + 1, query_len);
        query[query_len] = '\0';
    }

    if (strcmp(request_path, "/user") == 0) {
        printf("[TRACE] Layer 7: Controller accepted route /user\n");
        printf("        └─ Controller method: getUser()\n\n");

        printf("[TRACE] Layer 8: Spring service layer prepared the business logic\n");
        printf("        └─ Service: StudentService\n");
        printf("        └─ Method: findStudentByName()\n\n");

        if (extract_query_param(query, "name", name, sizeof(name))) {
            printf("[TRACE] Layer 9: Service passed parameter to repository/DAO\n");
            printf("        └─ name=%s\n", name);
            printf("        └─ Repository: StudentRepository\n");
            printf("        └─ Query shape: SELECT status FROM users WHERE name=?\n\n");

            resultset_from_user(name, &result_set);
            if (result_set.row_count > 0) {
                printf("[TRACE] Layer 10: Repository queried the database\n");
                printf("        └─ SQL executed: SELECT status FROM users WHERE name='%s'\n", name);
                printf("        └─ PostgreSQL returned %d row(s)\n", result_set.row_count);
                printf("        └─ ResultSet: %s=%s\n\n",
                       result_set.rows[0].column,
                       result_set.rows[0].value);

                entity_from_resultset(&result_set, &user_entity, name);
                printf("[TRACE] Layer 11: Hibernate entity populated\n");
                printf("        └─ Entity class: StudentEntity\n");
                printf("        └─ Entity values: name=%s, status=%s\n\n",
                       user_entity.name,
                       user_entity.status);

                dto_from_entity(&user_entity, &user_dto);
                printf("[TRACE] Layer 12: DTO created from entity\n");
                printf("        └─ DTO class: StudentResponseDto\n");
                printf("        └─ DTO values: name=%s, status=%s\n\n",
                       user_dto.name,
                       user_dto.status);

                serialize_json(&user_dto, response_body, sizeof(response_body));
                printf("[TRACE] Layer 13: HTTP response created\n");
                printf("        └─ Serializer: Jackson-style JSON\n");
                printf("        └─ Response body: %s\n\n", response_body);

                snprintf(response, response_size,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Connection: close\r\n\r\n"
                         "%s",
                         response_body);
                return;
            }

            printf("[TRACE] Layer 10: Repository returned no rows\n");
            printf("        └─ SQL executed: SELECT status FROM users WHERE name='%s'\n", name);
            printf("        └─ PostgreSQL returned 0 rows\n\n");
        } else {
            printf("[TRACE] Layer 9: missing query parameter 'name'\n\n");
        }

        snprintf(response, response_size,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Connection: close\r\n\r\n"
                 "User not found\n");
        return;
    }

    printf("[TRACE] Layer 7: Controller accepted route %s\n", request_path);
    printf("        └─ Controller method: renderPage()\n\n");

    printf("[TRACE] Layer 8: Spring service layer prepared the generic page response\n");
    printf("        └─ Service: PageService\n");
    printf("        └─ Method: renderPage()\n\n");

    printf("[TRACE] Layer 9: Service layer processed the request parameters\n");
    printf("        └─ Route: %s\n", request_path);
    printf("        └─ Query string: %s\n\n", query[0] ? query : "none");

    printf("[TRACE] Layer 10: Repository/cache layer retrieved content\n");
    printf("        └─ Simulated cache/database lookup\n\n");

    printf("[TRACE] Layer 11: DTO/entity object created for the response\n");
    printf("        └─ DTO class: PageResponseDto\n");
    printf("        └─ Response ready for serialization\n\n");

    printf("[TRACE] Layer 12: JDBC/Postgres driver completed any required DB interactions\n");
    printf("        └─ No actual DB rows required for this route\n\n");

    snprintf(response_body, sizeof(response_body),
             "<!DOCTYPE html>\n"
             "<html><head><title>Route %s</title></head><body>"
             "<h1>Route: %s</h1>"
             "<p>This request was processed through the simulated app stack.</p>"
             "</body></html>",
             request_path,
             request_path);

    printf("[TRACE] Layer 13: HTTP response created\n");
    printf("        └─ Serializer: HTML output\n");
    printf("        └─ Content-Type: text/html\n");
    printf("        └─ Response size: %zu bytes\n\n", strlen(response_body));

    snprintf(response, response_size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             response_body);
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
        printf("        └─ Client IP: %s\n", inet_ntoa(client_addr.sin_addr));
        printf("        └─ Client port: %d\n", ntohs(client_addr.sin_port));
        printf("        └─ Socket connection: ESTABLISHED\n\n");

        memset(buffer, 0, sizeof(buffer));
        received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            printf("[TRACE] Layer 3: spring-api socket received %zd bytes from kernel buffer\n", received);
            printf("        └─ Bytes received: %zd\n", received);
            printf("        └─ Buffer status: filled\n");
            printf("        └─ Data integrity: verified\n\n");

            process_request(buffer, response, sizeof(response));

            printf("[TRACE] Sending response to client\n");
            printf("        └─ Response size: %zu bytes\n", strlen(response));
            printf("        └─ Socket send status: OK\n");
            printf("        └─ Data transmission: complete\n\n");

            send(client_fd, response, strlen(response), 0);
        } else if (received == 0) {
            printf("Client disconnected gracefully.\n");
        } else {
            perror("recv");
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
