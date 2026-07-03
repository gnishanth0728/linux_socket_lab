/*
 * socket_lab.c
 *
 * Linux Networking Lab
 *
 * Stage 1
 *
 * Compile:
 * gcc socket_lab.c -o socket_lab
 *
 * Run:
 * ./socket_lab
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST "studentservices.jntuh.ac.in"
#define PORT 80

struct sockaddr_in server_addr;
char resolved_ip[INET_ADDRSTRLEN];

/*---------------------------------------------------------*/
/* Helper Functions                                        */
/*---------------------------------------------------------*/

void line()
{
    printf("=============================================================\n");
}

void wait_enter()
{
    printf("\nPress ENTER to continue...");
    getchar();
}

void run_command(const char *cmd)
{
    printf("\n$ %s\n\n", cmd);
    system(cmd);
}

void banner(const char *title)
{
    printf("\n\n");
    line();
    printf("%s\n", title);
    line();
}

/*---------------------------------------------------------*/
/* Print Process Information                               */
/*---------------------------------------------------------*/

void show_process()
{
    char cmd[512];

    sprintf(cmd,
            "ps -p %d -o pid,ppid,user,comm",
            getpid());

    run_command(cmd);
}

/*---------------------------------------------------------*/
/* Show File Descriptors                                   */
/*---------------------------------------------------------*/

void show_fd_table()
{
    char cmd[512];

    sprintf(cmd,
            "ls -l /proc/%d/fd",
            getpid());

    run_command(cmd);
}

/*---------------------------------------------------------*/
/* Show FD Info                                             */
/*---------------------------------------------------------*/

void show_fdinfo(int fd)
{
    char cmd[512];

    sprintf(cmd,
            "cat /proc/%d/fdinfo/%d",
            getpid(),
            fd);

    run_command(cmd);
}

/*---------------------------------------------------------*/
/* Show Socket State                                        */
/*---------------------------------------------------------*/

void show_ss()
{
    char cmd[512];

    sprintf(cmd,
            "sudo ss -tanpi | grep %d",
            getpid());

    run_command(cmd);
}

/*---------------------------------------------------------*/
/* Show lsof                                                */
/*---------------------------------------------------------*/

void show_lsof()
{
    char cmd[512];

    sprintf(cmd,
            "sudo lsof -p %d",
            getpid());

    run_command(cmd);
}

/*---------------------------------------------------------*/
/* Show TCP Table                                           */
/*---------------------------------------------------------*/

void show_tcp_table()
{
    run_command("cat /proc/net/tcp");
}

/*---------------------------------------------------------*/
/* Hex Dump                                                 */
/*---------------------------------------------------------*/

void hexdump(const unsigned char *data, int len)
{
    int i;
    int j;

    printf("\nHex Dump (%d bytes)\n\n", len);

    for (i = 0; i < len; i += 16)
    {
        printf("%04X  ", i);

        for (j = 0; j < 16; j++)
        {
            if (i + j < len)
                printf("%02X ", data[i + j]);
            else
                printf("   ");
        }

        printf(" ");

        for (j = 0; j < 16; j++)
        {
            if (i + j < len)
            {
                unsigned char c = data[i + j];

                if (c >= 32 && c <= 126)
                    printf("%c", c);
                else
                    printf(".");
            }
        }

        printf("\n");
    }
}

/*---------------------------------------------------------*/
/* Show External Commands                                   */
/*---------------------------------------------------------*/

void instructions()
{
    banner("Run these commands in another terminal");

    printf("1. Watch socket\n\n");
    printf("sudo watch -n 1 'ss -tanpi'\n\n");

    printf("2. Watch packets\n\n");
    printf("sudo tcpdump -i any -nn host %s\n\n", HOST);

    printf("3. Watch file descriptors\n\n");
    printf("watch -n 1 'ls -l /proc/%d/fd'\n\n", getpid());

    printf("4. Watch process\n\n");
    printf("watch -n 1 'ps -p %d -f'\n\n", getpid());

    line();
}

/*---------------------------------------------------------*/
/* Create Socket                                            */
/*---------------------------------------------------------*/

int create_socket()
{
    banner("Stage 2 : socket()");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    printf("Socket successfully created.\n");
    printf("Socket File Descriptor = %d\n", sockfd);

    return sockfd;
}

/*---------------------------------------------------------*/
/* Inspect Socket                                           */
/*---------------------------------------------------------*/

void inspect_socket(int fd)
{
    banner("Socket Inspection");

    printf("PID = %d\n", getpid());
    printf("Socket FD = %d\n\n", fd);

    printf("This socket is NOT connected yet.\n");
    printf("No TCP handshake has happened.\n\n");

    show_process();

    show_fd_table();

    show_fdinfo(fd);

    show_lsof();

    show_ss();

    printf("\n/proc/net/tcp\n\n");

    show_tcp_table();

    line();

    printf("\nLinux has created:\n\n");

    printf("Application\n");
    printf("      |\n");
    printf("      v\n");
    printf("FD = %d\n", fd);
    printf("      |\n");
    printf("      v\n");
    printf("Kernel Socket Object\n");
    printf("      |\n");
    printf("State = CLOSED\n");

    line();

    wait_enter();
}

/*---------------------------------------------------------*/
/* DNS Resolution                                           */
/*---------------------------------------------------------*/

void resolve_dns()
{
    banner("Stage 3 : DNS Resolution");

    struct hostent *host = gethostbyname(HOST);

    if (host == NULL)
    {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    inet_ntop(AF_INET,
              host->h_addr_list[0],
              resolved_ip,
              sizeof(resolved_ip));

    printf("Hostname : %s\n", HOST);
    printf("Resolved IP : %s\n\n", resolved_ip);

    memset(&server_addr,0,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    memcpy(&server_addr.sin_addr,
           host->h_addr_list[0],
           host->h_length);

    printf("Destination Socket Address Prepared\n");

    line();

    printf("\nCurrent Flow\n\n");

    printf("Application\n");
    printf("      |\n");
    printf("      v\n");
    printf("DNS Resolver\n");
    printf("      |\n");
    printf("      v\n");
    printf("%s\n", resolved_ip);

    line();

    wait_enter();
}

/*---------------------------------------------------------*/
/* Connect                                                  */
/*---------------------------------------------------------*/

void connect_socket(int fd)
{
    banner("Stage 4 : connect()");

    printf("Calling connect() ...\n\n");

    int rc = connect(fd,
                     (struct sockaddr *)&server_addr,
                     sizeof(server_addr));

    if(rc < 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("TCP Connection Established.\n");

    line();

    printf("\nKernel now knows:\n\n");

    printf("Source IP        : Assigned automatically\n");
    printf("Source Port      : Ephemeral Port\n");
    printf("Destination IP   : %s\n", resolved_ip);
    printf("Destination Port : %d\n", PORT);

    line();

    show_process();

    show_fd_table();

    show_fdinfo(fd);

    show_lsof();

    show_ss();

    printf("\nConnection should now appear as ESTABLISHED.\n");

    wait_enter();
}

/*---------------------------------------------------------*/
/* Build HTTP Request                                       */
/*---------------------------------------------------------*/

char http_request[2048];

void build_http_request()
{
    banner("Stage 5 : Construct HTTP Request");

    memset(http_request,0,sizeof(http_request));

    sprintf(http_request,

        "GET /oss/login.html HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Socket-Lab/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",

        HOST);

    printf("HTTP Request\n\n");

    printf("%s\n",http_request);

    line();

    printf("\nHTTP Request Length = %ld bytes\n",
           strlen(http_request));

    line();

    hexdump((unsigned char *)http_request,
            strlen(http_request));

    line();

    printf("\nNothing has been sent yet.\n");
    printf("The request only exists in user-space memory.\n");

    printf("\nCurrent Flow\n\n");

    printf("Application\n");
    printf("      |\n");
    printf("      |\n");
    printf("HTTP Bytes (User Memory)\n");
    printf("      |\n");
    printf("      |\n");
    printf("Not yet in Linux Kernel\n");

    wait_enter();
}

/*---------------------------------------------------------*/
/* Send HTTP Request                                        */
/*---------------------------------------------------------*/

void send_request(int fd)
{
    banner("Stage 6 : send()");

    printf("Calling send()...\n\n");

    int bytes = send(fd,
                     http_request,
                     strlen(http_request),
                     0);

    if(bytes < 0)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("Bytes Sent = %d\n", bytes);

    line();

    printf("\nLinux Kernel has copied the bytes into\n");
    printf("the Socket Send Buffer.\n");

    line();

    show_ss();

    show_fdinfo(fd);

    printf("\nCurrent Flow\n\n");

    printf("Application\n");
    printf("      |\n");
    printf("HTTP Bytes\n");
    printf("      |\n");
    printf("send(fd,...)\n");
    printf("      |\n");
    printf("Kernel Socket Send Buffer\n");
    printf("      |\n");
    printf("TCP Layer\n");
    printf("      |\n");
    printf("Waiting for transmission...\n");

    wait_enter();
}

/*---------------------------------------------------------*/
/* Receive HTTP Response                                    */
/*---------------------------------------------------------*/

void receive_response(int fd)
{
    banner("Stage 7 : recv()");

    char buffer[8192];

    memset(buffer,0,sizeof(buffer));

    printf("Waiting for HTTP Response...\n\n");

    int bytes = recv(fd,
                     buffer,
                     sizeof(buffer)-1,
                     0);

    if(bytes < 0)
    {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    printf("Bytes Received = %d\n\n", bytes);

    line();

    printf("\nLinux Kernel copied data from\n");
    printf("Socket Receive Buffer\n");
    printf("to\n");
    printf("Application Buffer.\n");

    line();

    printf("\nHTTP Response\n\n");

    printf("%s\n",buffer);

    line();

    printf("\nHex Dump\n");

    hexdump((unsigned char *)buffer,bytes);

    line();

    show_ss();

    show_fdinfo(fd);

    printf("\nCurrent Flow\n\n");

    printf("NIC\n");
    printf(" |\n");
    printf(" v\n");
    printf("Ethernet Frame\n");
    printf(" |\n");
    printf(" v\n");
    printf("IP Packet\n");
    printf(" |\n");
    printf(" v\n");
    printf("TCP Segment\n");
    printf(" |\n");
    printf(" v\n");
    printf("Socket Receive Buffer\n");
    printf(" |\n");
    printf("recv(fd,...)\n");
    printf(" |\n");
    printf(" v\n");
    printf("Application Buffer\n");

    wait_enter();
}

/*---------------------------------------------------------*/
/* Close Socket                                             */
/*---------------------------------------------------------*/

void close_socket(int fd)
{
    banner("Stage 8 : close()");

    printf("Closing socket...\n\n");

    close(fd);

    printf("Socket Closed Successfully.\n");

    line();

    printf("\nApplication\n");
    printf("      |\n");
    printf("close(fd)\n");
    printf("      |\n");
    printf("      v\n");
    printf("Kernel releases Socket Object\n");
    printf("      |\n");
    printf("      v\n");
    printf("TCP FIN\n");
    printf("      |\n");
    printf("      v\n");
    printf("FIN-ACK\n");
    printf("      |\n");
    printf("      v\n");
    printf("ACK\n");
    printf("      |\n");
    printf("Connection Closed\n");

    line();

    printf("\nChecking ss...\n");

    show_ss();

    printf("\nSocket should no longer appear.\n");

    wait_enter();
}

/*---------------------------------------------------------*/
/* Main                                                     */
/*---------------------------------------------------------*/

int main()
{
    banner("Linux Networking Lab");

    printf("PID = %d\n", getpid());

    instructions();

    wait_enter();

    int sockfd = create_socket();

    inspect_socket(sockfd);

    resolve_dns();

    connect_socket(sockfd);

    build_http_request();

    send_request(sockfd);

    receive_response(sockfd);

    close_socket(sockfd);

    banner("LAB COMPLETE");

    printf("\nYou have now observed the complete lifecycle.\n\n");

    printf("Application\n");
    printf("      |\n");
    printf("socket()\n");
    printf("      |\n");
    printf("FD Created\n");
    printf("      |\n");
    printf("DNS Resolution\n");
    printf("      |\n");
    printf("connect()\n");
    printf("      |\n");
    printf("TCP 3-Way Handshake\n");
    printf("      |\n");
    printf("HTTP Request Constructed\n");
    printf("      |\n");
    printf("send()\n");
    printf("      |\n");
    printf("Kernel Socket Send Buffer\n");
    printf("      |\n");
    printf("TCP Layer\n");
    printf("      |\n");
    printf("IP Layer\n");
    printf("      |\n");
    printf("Ethernet Layer\n");
    printf("      |\n");
    printf("NIC\n");
    printf("      |\n");
    printf("Internet\n");
    printf("      |\n");
    printf("Server\n");
    printf("      |\n");
    printf("HTTP Response\n");
    printf("      |\n");
    printf("recv()\n");
    printf("      |\n");
    printf("Application Buffer\n");
    printf("      |\n");
    printf("close()\n");

    return 0;
}
