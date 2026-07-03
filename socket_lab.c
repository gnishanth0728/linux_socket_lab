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

    printf("\nNext Stage\n");
    printf("Construct HTTP Request\n");

    close(sockfd);

    return 0;
}
