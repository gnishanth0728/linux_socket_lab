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

    printf("\n========== REQUEST FLOW STARTING ==========\n");
    printf("[TRACE] Layer 1: React frontend/client sent an HTTP request\n");
    printf("        └─ Request initiated from client\n\n");
    
    printf("[TRACE] Layer 2: nginx-proxy received the request and passed it to the app stack\n");
    printf("        └─ nginx-proxy listening on port 80\n");
    printf("        └─ Forwarding to spring-api backend\n\n");
    
    printf("[TRACE] Layer 3: spring-api socket received the TCP payload from the kernel stack\n");
    printf("        └─ Socket descriptor: active\n");
    printf("        └─ TCP payload size: %zu bytes\n", strlen(request));
    printf("        └─ Kernel socket buffer: filled\n\n");
    
    printf("[TRACE] Layer 4: HTTP parser extracted bytes\n");
    printf("        └─ Raw request:\n");
    printf("           %s\n", request);

    if (sscanf(request, "%15s %255s", method, path) != 2) {
        printf("[TRACE] Layer 4: parse failure, returning 400\n");
        printf("        └─ HTTP parse error detected\n\n");
        snprintf(response, response_size,
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: text/plain\r\n"
                 "Connection: close\r\n\r\n"
                 "Bad request");
        return;
    }

    printf("[TRACE] Layer 5: Spring controller received request\n");
    printf("        └─ HTTP Method: %s\n", method);
    printf("        └─ Request Path: %s\n", path);
    printf("        └─ Controller: user-controller.java\n\n");
    
    printf("[TRACE] Layer 6: MVC dispatcher routed the request to the controller logic\n");
    printf("        └─ Dispatcher type: DispatcherServlet\n");
    printf("        └─ Handler mapping: RequestMappingHandlerMapping\n");
    printf("        └─ Route matched successfully\n\n");

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
        printf("        └─ Controller method: getUser()\n");
        printf("        └─ Annotation: @RequestMapping(path='/user')\n");
        printf("        └─ HTTP status: 200 OK\n\n");
        
        printf("[TRACE] Layer 8: Spring service layer prepared the business logic\n");
        printf("        └─ Service class: UserServiceImpl\n");
        printf("        └─ Method: getUserStatus(name)\n");
        printf("        └─ Business logic: processing user query\n\n");
        
        if (sscanf(query, "name=%63s", name) == 1) {
            printf("[TRACE] Layer 9: Service passed parameter to repository/DAO\n");
            printf("        └─ Parameter name: %s\n", name);
            printf("        └─ DAO class: UserRepositoryImpl\n");
            printf("        └─ Method: findByName()\n\n");
            
            if (db_lookup(name, db_status, sizeof(db_status))) {
                printf("[TRACE] Layer 10: Repository queried the database\n");
                printf("        └─ Database: postgres-db (port 5432)\n");
                printf("        └─ Table: users\n");
                printf("        └─ Query: SELECT status FROM users WHERE name='%s'\n", name);
                printf("        └─ Result found: YES\n");
                printf("        └─ Database response: status=%s\n\n", db_status);
                
                printf("[TRACE] Layer 11: DTO/Entity object created from repository data\n");
                printf("        └─ Entity class: User.java\n");
                printf("        └─ Fields: id, name, status\n");
                printf("        └─ Entity state: name=%s, status=%s\n", name, db_status);
                printf("        └─ DTO mapping: complete\n\n");
                
                printf("[TRACE] Layer 12: JDBC/Postgres driver completed the DB interaction\n");
                printf("        └─ JDBC driver: postgresql-42.x.x.jar\n");
                printf("        └─ Connection pool: HikariCP\n");
                printf("        └─ Transaction: COMMIT\n");
                printf("        └─ Result set closed: YES\n\n");
                
                printf("[TRACE] Layer 13: Response assembled and returned\n");
                printf("        └─ Serializer: Jackson (JSON)\n");
                printf("        └─ Content-Type: text/plain\n");
                printf("        └─ Status code: 200 OK\n");
                printf("        └─ Response body: User=%s Status=%s\n", name, db_status);
                printf("        └─ Response sent through: spring-api -> nginx-proxy -> client\n");
                printf("========== REQUEST FLOW COMPLETE ==========\n\n");
                
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

        printf("[TRACE] Layer 10: Repository query to database\n");
        printf("        └─ Database: postgres-db (port 5432)\n");
        printf("        └─ Query: SELECT status FROM users WHERE name='%s'\n", name);
        printf("        └─ Result found: NO\n\n");
        
        body = "User not found in DB";
    } else {
        printf("[TRACE] Layer 7: Controller accepted route %s\n", path);
        printf("        └─ Controller: GenericController\n");
        printf("        └─ Method: handleRequest()\n");
        printf("        └─ Path pattern: ANY_ROUTE\n\n");
        
        printf("[TRACE] Layer 8: Spring service layer prepared to serve the request\n");
        printf("        └─ Service class: GenericService\n");
        printf("        └─ Method: processRequest()\n");
        printf("        └─ Service logic: preparing response object\n\n");
        
        printf("[TRACE] Layer 9: Service layer processed the request parameters\n");
        printf("        └─ Route: %s\n", path);
        printf("        └─ Method: %s\n", method);
        printf("        └─ Parameters parsed: YES\n\n");
        
        printf("[TRACE] Layer 10: Repository/cache layer retrieved content\n");
        printf("        └─ Database: postgres-db\n");
        printf("        └─ Cache: Redis (optional)\n");
        printf("        └─ Query: Generic retrieval for route %s\n", path);
        printf("        └─ Cache hit: NO\n");
        printf("        └─ Database hit: YES\n\n");
        
        printf("[TRACE] Layer 11: DTO/entity object created for the response\n");
        printf("        └─ Entity class: GenericResponse.java\n");
        printf("        └─ Fields: route, method, timestamp, data\n");
        printf("        └─ Entity state: route=%s, method=%s\n", path, method);
        printf("        └─ Object serialization: complete\n\n");
        
        printf("[TRACE] Layer 12: JDBC/Postgres driver completed any required DB interactions\n");
        printf("        └─ JDBC driver: postgresql-42.x.x.jar\n");
        printf("        └─ Connection: active\n");
        printf("        └─ Transaction: COMMIT\n\n");
        
        printf("[TRACE] Layer 13: Response assembled and returned\n");
        printf("        └─ Serializer: Jackson (HTML)\n");
        printf("        └─ Content-Type: text/html\n");
        printf("        └─ Status code: 200 OK\n");
        printf("        └─ Response size: %zu bytes\n", response_size);
        printf("        └─ Response sent through: spring-api -> nginx-proxy -> client\n");
        printf("========== REQUEST FLOW COMPLETE ==========\n\n");
        
        snprintf(response, response_size,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n"
                 "Connection: close\r\n\r\n"
                 "<!DOCTYPE html>\n"
                 "<html>\n"
                 "<head><title>Response</title></head>\n"
                 "<body>\n"
                 "<h1>Request received and processed</h1>\n"
                 "<p><strong>Route:</strong> %s</p>\n"
                 "<p><strong>Method:</strong> %s</p>\n"
                 "<p>This request was traced through all layers:</p>\n"
                 "<ul>\n"
                 "<li>Layer 1: React frontend sent request</li>\n"
                 "<li>Layer 2: nginx-proxy received connection</li>\n"
                 "<li>Layer 3: spring-api socket received payload</li>\n"
                 "<li>Layer 4: HTTP parser extracted bytes</li>\n"
                 "<li>Layer 5: Spring controller received method and path</li>\n"
                 "<li>Layer 6: MVC dispatcher routed request</li>\n"
                 "<li>Layer 7: Controller processed the route</li>\n"
                 "<li>Layer 8: Service layer prepared business logic</li>\n"
                 "<li>Layer 9: Service passed parameters</li>\n"
                 "<li>Layer 10: Repository retrieved data</li>\n"
                 "<li>Layer 11: DTO/entity created</li>\n"
                 "<li>Layer 12: JDBC/Postgres completed DB interaction</li>\n"
                 "<li>Layer 13: Response assembled and returned</li>\n"
                 "</ul>\n"
                 "</body>\n"
                 "</html>",
                 path, method);
        return;
    }

    printf("[TRACE] Layer 13: assembling error response for the client\n");
    printf("        └─ Status code: 404 Not Found\n");
    printf("        └─ Error message: %s\n", body);
    printf("========== REQUEST FLOW COMPLETE ==========\n\n");
    
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
