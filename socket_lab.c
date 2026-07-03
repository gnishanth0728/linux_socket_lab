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
#include <fcntl.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_HOST "studentservices.jntuh.ac.in"
#define DEFAULT_PORT 80

struct sockaddr_in server_addr;
char resolved_ip[INET_ADDRSTRLEN];
int packet_detail_mode = 0;
char host_name[256] = DEFAULT_HOST;
char request_path[512] = "/oss/login.html";
int server_port = DEFAULT_PORT;
int save_output_enabled = 0;
char output_file_path[512] = "";
char output_tmp_path[640] = "";
int saved_stdout_fd = -1;
int saved_stderr_fd = -1;
pid_t output_tee_pid = -1;

/*---------------------------------------------------------*/
/* Helper Functions                                        */
/*---------------------------------------------------------*/

void line()
{
    printf("=============================================================\n");
}

void wait_enter()
{
    char input[32];

    if (!isatty(STDIN_FILENO))
        return;

    printf("\nPress ENTER to continue...");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL)
        return;

    while (strchr(input, '\n') == NULL)
    {
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;
    }
}

void run_command(const char *cmd)
{
    printf("\n$ %s\n\n", cmd);
    system(cmd);
}

int is_ipv4_address(const char *token)
{
    struct in_addr addr;
    return inet_pton(AF_INET, token, &addr) == 1;
}

int lookup_ip_country(const char *ip, char *country, size_t country_size)
{
    char cmd[512];
    char buf[512];
    FILE *fp;
    char *newline;

    snprintf(cmd, sizeof(cmd),
             "timeout 2 geoiplookup %s 2>/dev/null | grep -oE '[A-Z]{2}$'",
             ip);
    fp = popen(cmd, "r");
    if (fp != NULL)
    {
        if (fgets(buf, sizeof(buf), fp) != NULL)
        {
            newline = strchr(buf, '\n');
            if (newline)
                *newline = '\0';
            if (*buf)
            {
                strncpy(country, buf, country_size);
                country[country_size - 1] = '\0';
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }

    snprintf(cmd, sizeof(cmd),
             "timeout 3 curl -s https://ipinfo.io/%s/country 2>/dev/null",
             ip);
    fp = popen(cmd, "r");
    if (fp != NULL)
    {
        if (fgets(buf, sizeof(buf), fp) != NULL)
        {
            newline = strchr(buf, '\n');
            if (newline)
                *newline = '\0';
            if (*buf && strlen(buf) <= 3)
            {
                strncpy(country, buf, country_size);
                country[country_size - 1] = '\0';
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }

    snprintf(cmd, sizeof(cmd),
             "timeout 2 whois -h whois.arin.net %s 2>/dev/null | grep -i 'country:' | head -1 | cut -d: -f2 | xargs",
             ip);
    fp = popen(cmd, "r");
    if (fp != NULL)
    {
        if (fgets(buf, sizeof(buf), fp) != NULL)
        {
            newline = strchr(buf, '\n');
            if (newline)
                *newline = '\0';
            if (*buf)
            {
                strncpy(country, buf, country_size);
                country[country_size - 1] = '\0';
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }

    return 0;
}

void banner(const char *title)
{
    printf("\n\n");
    line();
    printf("%s\n", title);
    line();
}

int start_output_tee(const char *path)
{
    int pipefd[2];
    pid_t pid;

    snprintf(output_tmp_path,
             sizeof(output_tmp_path),
             "%s.tmp.%d",
             path,
             (int)getpid());

    if (pipe(pipefd) < 0)
        return -1;

    saved_stdout_fd = dup(STDOUT_FILENO);
    saved_stderr_fd = dup(STDERR_FILENO);

    if (saved_stdout_fd < 0 || saved_stderr_fd < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    pid = fork();

    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0)
    {
        int log_fd;
        char buffer[4096];
        ssize_t n;

        close(pipefd[1]);

        log_fd = open(output_tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (log_fd < 0)
            _exit(1);

        while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        {
            write(STDOUT_FILENO, buffer, (size_t)n);
            write(log_fd, buffer, (size_t)n);
        }

        close(log_fd);
        close(pipefd[0]);
        _exit(0);
    }

    close(pipefd[0]);

    if (dup2(pipefd[1], STDOUT_FILENO) < 0 || dup2(pipefd[1], STDERR_FILENO) < 0)
    {
        close(pipefd[1]);
        return -1;
    }

    close(pipefd[1]);
    output_tee_pid = pid;
    return 0;
}

void stop_output_tee()
{
    int status;

    fflush(stdout);
    fflush(stderr);

    if (saved_stdout_fd >= 0)
    {
        dup2(saved_stdout_fd, STDOUT_FILENO);
        close(saved_stdout_fd);
        saved_stdout_fd = -1;
    }

    if (saved_stderr_fd >= 0)
    {
        dup2(saved_stderr_fd, STDERR_FILENO);
        close(saved_stderr_fd);
        saved_stderr_fd = -1;
    }

    if (output_tee_pid > 0)
    {
        waitpid(output_tee_pid, &status, 0);
        output_tee_pid = -1;
    }

    if (save_output_enabled && output_tmp_path[0] != '\0')
    {
        if (rename(output_tmp_path, output_file_path) < 0)
            perror("rename transcript");
    }
}

void format_tcp_flags(unsigned char flags, char *out, size_t out_size)
{
    size_t used = 0;
    out[0] = '\0';

    if (flags & TH_SYN)
        used += snprintf(out + used, out_size - used, "SYN,");
    if (flags & TH_ACK)
        used += snprintf(out + used, out_size - used, "ACK,");
    if (flags & TH_FIN)
        used += snprintf(out + used, out_size - used, "FIN,");
    if (flags & TH_RST)
        used += snprintf(out + used, out_size - used, "RST,");
    if (flags & TH_PUSH)
        used += snprintf(out + used, out_size - used, "PSH,");
    if (flags & TH_URG)
        used += snprintf(out + used, out_size - used, "URG,");
    if (flags & 0x40)
        used += snprintf(out + used, out_size - used, "ECE,");
    if (flags & 0x80)
        used += snprintf(out + used, out_size - used, "CWR,");

    if (used == 0)
    {
        snprintf(out, out_size, "NONE");
        return;
    }

    if (out[used - 1] == ',')
        out[used - 1] = '\0';
}

void parse_runtime_options(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--packet-detail") == 0)
            packet_detail_mode = 1;
        else if (strcmp(argv[i], "--packet-compact") == 0)
            packet_detail_mode = 0;
        else if (strcmp(argv[i], "--host") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("Missing value for --host\n");
                exit(EXIT_FAILURE);
            }

            i++;

            if (strlen(argv[i]) >= sizeof(host_name))
            {
                printf("Host is too long (max %zu chars)\n", sizeof(host_name) - 1);
                exit(EXIT_FAILURE);
            }

            size_t j;
            for (j = 0; argv[i][j] != '\0'; j++)
            {
                unsigned char c = (unsigned char)argv[i][j];
                if (!(isalnum(c) || c == '.' || c == '-'))
                {
                    printf("Invalid host value. Allowed chars: a-z A-Z 0-9 . -\n");
                    exit(EXIT_FAILURE);
                }
            }

            strcpy(host_name, argv[i]);
        }
        else if (strcmp(argv[i], "--port") == 0)
        {
            char *endptr = NULL;
            long p;

            if (i + 1 >= argc)
            {
                printf("Missing value for --port\n");
                exit(EXIT_FAILURE);
            }

            i++;
            p = strtol(argv[i], &endptr, 10);

            if (*argv[i] == '\0' || *endptr != '\0' || p < 1 || p > 65535)
            {
                printf("Invalid port. Use a number between 1 and 65535\n");
                exit(EXIT_FAILURE);
            }

            server_port = (int)p;
        }
        else if (strcmp(argv[i], "--path") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("Missing value for --path\n");
                exit(EXIT_FAILURE);
            }

            i++;

            if (argv[i][0] != '/')
            {
                printf("Invalid path. Path must start with '/'.\n");
                exit(EXIT_FAILURE);
            }

            if (strlen(argv[i]) >= sizeof(request_path))
            {
                printf("Path is too long (max %zu chars)\n", sizeof(request_path) - 1);
                exit(EXIT_FAILURE);
            }

            strcpy(request_path, argv[i]);
        }
        else if (strcmp(argv[i], "--save-output") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("Missing value for --save-output\n");
                exit(EXIT_FAILURE);
            }

            i++;

            if (strlen(argv[i]) >= sizeof(output_file_path))
            {
                printf("Output file path too long (max %zu chars)\n", sizeof(output_file_path) - 1);
                exit(EXIT_FAILURE);
            }

            strcpy(output_file_path, argv[i]);
            save_output_enabled = 1;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: ./socket_lab [--packet-compact | --packet-detail] [--host NAME] [--port N] [--save-output FILE]\n\n");
            printf("  --packet-compact   Show one-line packet summary table (default)\n");
            printf("  --packet-detail    Show multi-line verbose packet decode\n");
            printf("  --host NAME        Target hostname (default: %s)\n", DEFAULT_HOST);
            printf("  --port N           Target TCP port (default: %d)\n", DEFAULT_PORT);
            printf("  --path PATH        Request path (default: %s)\n", "/oss/login.html");
            printf("  --save-output FILE Save full stage output to FILE while running\n");
            exit(EXIT_SUCCESS);
        }
    }
}

void show_packet_headers_snapshot()
{
    char cmd[1024];

    sprintf(cmd,
            "sudo timeout 4 tcpdump -i any -nn -e -vvv -c 6 'host %s and tcp port %d'",
            host_name,
            server_port);

    printf("\nPacket header snapshot (Ethernet/IP/TCP)\n");
    printf("This uses tcpdump because recv() on SOCK_STREAM returns payload only.\n");
    run_command(cmd);
}

void print_mac(const unsigned char *mac)
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]);
}

void print_tcp_flags(unsigned char flags)
{
    if (flags & 0x02)
        printf("SYN ");
    if (flags & 0x10)
        printf("ACK ");
    if (flags & 0x01)
        printf("FIN ");
    if (flags & 0x04)
        printf("RST ");
    if (flags & 0x08)
        printf("PSH ");
    if (flags & 0x20)
        printf("URG ");
    if (flags & 0x40)
        printf("ECE ");
    if (flags & 0x80)
        printf("CWR ");
}

int belongs_to_flow(const struct iphdr *ip,
                    const struct tcphdr *tcp,
                    unsigned int local_ip,
                    unsigned int remote_ip,
                    unsigned short local_port,
                    unsigned short remote_port)
{
    unsigned int src_ip = ip->saddr;
    unsigned int dst_ip = ip->daddr;
    unsigned short src_port = ntohs(tcp->source);
    unsigned short dst_port = ntohs(tcp->dest);

    int c2s = (src_ip == local_ip &&
               dst_ip == remote_ip &&
               src_port == local_port &&
               dst_port == remote_port);

    int s2c = (src_ip == remote_ip &&
               dst_ip == local_ip &&
               src_port == remote_port &&
               dst_port == local_port);

    return c2s || s2c;
}

int flow_direction(const struct iphdr *ip,
                   const struct tcphdr *tcp,
                   unsigned int local_ip,
                   unsigned int remote_ip,
                   unsigned short local_port,
                   unsigned short remote_port)
{
    unsigned int src_ip = ip->saddr;
    unsigned int dst_ip = ip->daddr;
    unsigned short src_port = ntohs(tcp->source);
    unsigned short dst_port = ntohs(tcp->dest);

    if (src_ip == local_ip &&
        dst_ip == remote_ip &&
        src_port == local_port &&
        dst_port == remote_port)
        return 1;

    if (src_ip == remote_ip &&
        dst_ip == local_ip &&
        src_port == remote_port &&
        dst_port == local_port)
        return -1;

    return 0;
}

void capture_raw_headers_for_flow(int tcp_fd)
{
    struct sockaddr_in local_addr;
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int raw_fd;
    struct timeval tv;
    unsigned char frame[65536];
    int shown = 0;

    memset(&local_addr, 0, sizeof(local_addr));
    memset(&peer_addr, 0, sizeof(peer_addr));

    if (getsockname(tcp_fd, (struct sockaddr *)&local_addr, &addr_len) < 0)
    {
        perror("getsockname");
        return;
    }

    addr_len = sizeof(struct sockaddr_in);

    if (getpeername(tcp_fd, (struct sockaddr *)&peer_addr, &addr_len) < 0)
    {
        perror("getpeername");
        return;
    }

    raw_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));

    if (raw_fd < 0)
    {
        perror("socket(AF_PACKET)");
        printf("Run as root (or grant CAP_NET_RAW) to decode L2/L3/L4 headers in code.\n");
        return;
    }

    tv.tv_sec = 4;
    tv.tv_usec = 0;

    if (setsockopt(raw_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("setsockopt SO_RCVTIMEO raw");
        close(raw_fd);
        return;
    }

    line();
    printf("\nRaw Socket Decode (AF_PACKET)\n\n");
    printf("Local  : %s:%d\n",
           inet_ntoa(local_addr.sin_addr),
           ntohs(local_addr.sin_port));
    printf("Remote : %s:%d\n\n",
           inet_ntoa(peer_addr.sin_addr),
           ntohs(peer_addr.sin_port));

    printf("Capturing up to 6 matching frames...\n\n");

    if (!packet_detail_mode)
    {
        printf("%-5s %-5s %-21s %-21s %-8s %-8s %-18s %-8s %-8s\n",
               "No",
               "Dir",
               "Src",
               "Dst",
               "SPort",
               "DPort",
               "Flags",
               "Win",
               "Payload");
        printf("-----------------------------------------------------------------------------------------------\n");
    }

    while (shown < 6)
    {
        int n = recvfrom(raw_fd,
                         frame,
                         sizeof(frame),
                         0,
                         NULL,
                         NULL);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("Raw capture timeout reached.\n");
                break;
            }

            if (errno == EINTR)
                continue;

            perror("recvfrom raw");
            break;
        }

        if (n < (int)(sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct tcphdr)))
            continue;

        struct ether_header *eth = (struct ether_header *)frame;

        if (ntohs(eth->ether_type) != ETH_P_IP)
            continue;

        struct iphdr *ip = (struct iphdr *)(frame + sizeof(struct ether_header));
        int ip_header_len = ip->ihl * 4;

        if (ip->protocol != IPPROTO_TCP)
            continue;

        if (n < (int)(sizeof(struct ether_header) + ip_header_len + sizeof(struct tcphdr)))
            continue;

        struct tcphdr *tcp = (struct tcphdr *)(frame + sizeof(struct ether_header) + ip_header_len);

        int direction = flow_direction(ip,
                           tcp,
                           local_addr.sin_addr.s_addr,
                           peer_addr.sin_addr.s_addr,
                           ntohs(local_addr.sin_port),
                           ntohs(peer_addr.sin_port));

        if (direction == 0)
            continue;

        int tcp_header_len = tcp->doff * 4;
        int payload_len = n - (int)sizeof(struct ether_header) - ip_header_len - tcp_header_len;
        unsigned int seq = ntohl(tcp->seq);
        unsigned int ack = ntohl(tcp->ack_seq);
        unsigned char flags = *(((unsigned char *)tcp) + 13);
         char src_ip[INET_ADDRSTRLEN];
         char dst_ip[INET_ADDRSTRLEN];
         char flags_text[64];

         inet_ntop(AF_INET, &ip->saddr, src_ip, sizeof(src_ip));
         inet_ntop(AF_INET, &ip->daddr, dst_ip, sizeof(dst_ip));
         format_tcp_flags(flags, flags_text, sizeof(flags_text));

         if (packet_detail_mode)
         {
             printf("Frame %d\n", shown + 1);

             printf("  Ethernet: ");
             print_mac((unsigned char *)eth->ether_shost);
             printf(" -> ");
             print_mac((unsigned char *)eth->ether_dhost);
             printf("  type=0x%04X\n", ntohs(eth->ether_type));

             printf("  IPv4    : %s -> %s  ihl=%d ttl=%d total_len=%d\n",
                 src_ip,
                 dst_ip,
                 ip_header_len,
                 ip->ttl,
                 ntohs(ip->tot_len));

             printf("  TCP     : %u -> %u  seq=%u ack=%u flags=%s",
                 ntohs(tcp->source),
                 ntohs(tcp->dest),
                 seq,
                 ack,
                 flags_text);
             printf(" win=%u payload=%d\n\n",
                 ntohs(tcp->window),
                 payload_len > 0 ? payload_len : 0);
         }
         else
         {
             printf("%-5d %-5s %-21s %-21s %-8u %-8u %-18s %-8u %-8d\n",
                 shown + 1,
                 direction > 0 ? "C->S" : "S->C",
                 src_ip,
                 dst_ip,
                 ntohs(tcp->source),
                 ntohs(tcp->dest),
                 flags_text,
                 ntohs(tcp->window),
                 payload_len > 0 ? payload_len : 0);
         }

        shown++;
    }

    close(raw_fd);
}

int configure_recv_timeout(int fd, int timeout_seconds)
{
    struct timeval tv;

    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;

    return setsockopt(fd,
                      SOL_SOCKET,
                      SO_RCVTIMEO,
                      &tv,
                      sizeof(tv));
}

int send_all(int fd, const char *data, size_t len)
{
    size_t sent_total = 0;

    while (sent_total < len)
    {
        int sent = send(fd,
                        data + sent_total,
                        len - sent_total,
                        0);

        if (sent < 0)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (sent == 0)
            break;

        sent_total += (size_t)sent;
    }

    return (sent_total == len) ? 0 : -1;
}

void analyze_http_response(const char *buffer, size_t total)
{
    const char *status_end = strstr(buffer, "\r\n");
    const char *headers_end = strstr(buffer, "\r\n\r\n");

    line();
    printf("\nHTTP Framing Analysis\n\n");

    if (status_end != NULL)
        printf("Status Line   : %.*s\n",
               (int)(status_end - buffer),
               buffer);
    else
        printf("Status Line   : Not found\n");

    if (headers_end != NULL)
    {
        size_t header_bytes = (size_t)(headers_end - buffer) + 4;
        size_t body_bytes = (total > header_bytes) ? (total - header_bytes) : 0;

        printf("Header Bytes  : %zu\n", header_bytes);
        printf("Body Bytes    : %zu\n", body_bytes);
    }
    else
    {
        printf("Header Bytes  : Could not detect header terminator\n");
        printf("Body Bytes    : Unknown\n");
    }
}

int parse_content_length(const char *headers, size_t headers_len, size_t *out_len)
{
    const char *key = "Content-Length:";
    const char *found = strstr(headers, key);

    if (found == NULL)
        return 0;

    if ((size_t)(found - headers) >= headers_len)
        return 0;

    found += strlen(key);

    while (*found == ' ' || *found == '\t')
        found++;

    char *endptr = NULL;
    unsigned long long value = strtoull(found, &endptr, 10);

    if (endptr == found)
        return 0;

    *out_len = (size_t)value;
    return 1;
}

int is_chunked_transfer(const char *headers)
{
    const char *key = "Transfer-Encoding:";
    const char *found = strstr(headers, key);

    if (found == NULL)
        return 0;

    return strstr(found, "chunked") != NULL;
}

int chunked_body_complete(const char *body)
{
    if (strncmp(body, "0\r\n\r\n", 5) == 0)
        return 1;

    if (strstr(body, "\r\n0\r\n\r\n") != NULL)
        return 1;

    return 0;
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
    printf("sudo tcpdump -i any -nn host %s\n\n", host_name);

    printf("3. Watch file descriptors\n\n");
    printf("watch -n 1 'ls -l /proc/%d/fd'\n\n", getpid());

    printf("4. Watch process\n\n");
    printf("watch -n 1 'ps -p %d -f'\n\n", getpid());

    printf("5. Trace route to target\n\n");
    printf("traceroute -n %s\n\n", host_name);

    line();
}

/*---------------------------------------------------------*/
/* Traceroute                                               */
/*---------------------------------------------------------*/

void show_traceroute()
{
    char cmd[1024];
    char line_buf[1024];
    char line_copy[1024];
    char country[256];
    FILE *fp;

    banner("Stage 3.5 : Traceroute Path");

    printf("Tracing route to %s ...\n\n", host_name);
    printf("This shows each hop between your machine and the server.\n");
    printf("When available, each hop IP will also show country/location info.\n\n");

    sprintf(cmd,
            "(traceroute -n %s || tracepath -n %s)",
            host_name,
            host_name);

    fp = popen(cmd, "r");
    if (fp == NULL)
    {
        perror("popen traceroute");
        run_command(cmd);
    }
    else
    {
        while (fgets(line_buf, sizeof(line_buf), fp) != NULL)
        {
            printf("%s", line_buf);
            strncpy(line_copy, line_buf, sizeof(line_copy));
            line_copy[sizeof(line_copy) - 1] = '\0';

            char *token = strtok(line_copy, " \t\r\n");
            if (token == NULL)
                continue;

            if (!isdigit((unsigned char)token[0]))
                continue;

            while ((token = strtok(NULL, " \t\r\n")) != NULL)
            {
                if (is_ipv4_address(token))
                {
                    if (lookup_ip_country(token, country, sizeof(country)))
                    {
                        printf("    Location/Country: %s\n", country);
                    }
                    break;
                }
            }
        }
        pclose(fp);
    }

    line();
    printf("\nTraceroute output above is now part of this lab session output.\n");

    wait_enter();
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

    struct hostent *host = gethostbyname(host_name);

    if (host == NULL)
    {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    inet_ntop(AF_INET,
              host->h_addr_list[0],
              resolved_ip,
              sizeof(resolved_ip));

    printf("Hostname : %s\n", host_name);
    printf("Resolved IP : %s\n\n", resolved_ip);

    memset(&server_addr,0,sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

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

    if (configure_recv_timeout(fd, 5) < 0)
    {
        perror("setsockopt SO_RCVTIMEO");
        exit(EXIT_FAILURE);
    }

    printf("TCP Connection Established.\n");
    printf("Receive timeout set to 5 seconds.\n");

    line();

    printf("\nKernel now knows:\n\n");

    printf("Source IP        : Assigned automatically\n");
    printf("Source Port      : Ephemeral Port\n");
    printf("Destination IP   : %s\n", resolved_ip);
    printf("Destination Port : %d\n", server_port);

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

        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Socket-Lab/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",

        request_path,
        host_name);

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

    size_t req_len = strlen(http_request);

    if(send_all(fd,
                http_request,
                req_len) < 0)
    {
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("Bytes Sent = %zu\n", req_len);

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

    printf("\nNow capturing live packet headers for a few packets...\n");
    printf("You should see Ethernet, IP and TCP fields below.\n");
    show_packet_headers_snapshot();

    printf("\nNow decoding those headers directly in C using AF_PACKET...\n");
    capture_raw_headers_for_flow(fd);

    wait_enter();
}

/*---------------------------------------------------------*/
/* Receive HTTP Response                                    */
/*---------------------------------------------------------*/

void receive_response(int fd)
{
    banner("Stage 7 : recv()");

    char chunk[4096];
    size_t total = 0;
    size_t capacity = 16384;
    char *buffer = malloc(capacity);
    int response_complete = 0;
    int used_timeout = 0;
    int has_content_length = 0;
    int uses_chunked = 0;
    size_t content_length = 0;

    if (buffer == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for HTTP Response...\n\n");

    while (1)
    {
        int bytes = recv(fd,
                         chunk,
                         sizeof(chunk),
                         0);

        if (bytes < 0)
        {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("Receive timeout reached; stopping read loop.\n");
                used_timeout = 1;
                break;
            }

            perror("recv");
            free(buffer);
            exit(EXIT_FAILURE);
        }

        if (bytes == 0)
            break;

        printf("Received chunk = %d bytes\n", bytes);

        if (total + (size_t)bytes + 1 > capacity)
        {
            while (total + (size_t)bytes + 1 > capacity)
                capacity *= 2;

            char *new_buffer = realloc(buffer, capacity);

            if (new_buffer == NULL)
            {
                perror("realloc");
                free(buffer);
                exit(EXIT_FAILURE);
            }

            buffer = new_buffer;
        }

        memcpy(buffer + total, chunk, (size_t)bytes);
        total += (size_t)bytes;
        buffer[total] = '\0';

        if (!response_complete)
        {
            char *headers_end = strstr(buffer, "\r\n\r\n");

            if (headers_end != NULL)
            {
                size_t header_bytes = (size_t)(headers_end - buffer) + 4;
                char *body = buffer + header_bytes;
                size_t body_bytes = (total > header_bytes) ? (total - header_bytes) : 0;

                if (!has_content_length)
                    has_content_length = parse_content_length(buffer,
                                                              header_bytes,
                                                              &content_length);

                if (!uses_chunked)
                    uses_chunked = is_chunked_transfer(buffer);

                if (has_content_length && body_bytes >= content_length)
                {
                    response_complete = 1;
                    printf("Response complete via Content-Length framing.\n");
                    break;
                }

                if (uses_chunked && chunked_body_complete(body))
                {
                    response_complete = 1;
                    printf("Response complete via chunked terminator.\n");
                    break;
                }
            }
        }
    }

    buffer[total] = '\0';

    printf("Bytes Received = %zu\n\n", total);

    line();
    printf("\nImportant\n\n");
    printf("recv() on a normal TCP socket does not return Ethernet/IP/TCP headers.\n");
    printf("Linux strips those headers in the kernel before data reaches this buffer.\n");
    printf("To view wire-level headers, use tcpdump/raw sockets/AF_PACKET.\n");

    if (!response_complete)
    {
        if (used_timeout)
            printf("Read loop ended due to timeout fallback.\n\n");
        else
            printf("Read loop ended because peer closed the connection.\n\n");
    }

    analyze_http_response(buffer, total);

    line();

    printf("\nLinux Kernel copied data from\n");
    printf("Socket Receive Buffer\n");
    printf("to\n");
    printf("Application Buffer.\n");

    line();

    printf("\nHTTP Response\n\n");

    printf("%s\n", buffer);

    line();

    printf("\nHex Dump\n");

    hexdump((unsigned char *)buffer, (int)total);

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

    free(buffer);

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

int main(int argc, char *argv[])
{
    parse_runtime_options(argc, argv);

    if (save_output_enabled)
    {
        if (start_output_tee(output_file_path) < 0)
        {
            perror("start_output_tee");
            exit(EXIT_FAILURE);
        }
    }

    banner("Linux Networking Lab");

    printf("PID = %d\n", getpid());
    printf("Packet Decode Mode = %s\n",
           packet_detail_mode ? "Detailed" : "Compact");
    printf("Target Host = %s\n", host_name);
    printf("Target Port = %d\n", server_port);

    instructions();

    wait_enter();

    int sockfd = create_socket();

    inspect_socket(sockfd);

    resolve_dns();

    show_traceroute();

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

    if (save_output_enabled)
    {
        stop_output_tee();
    }

    return 0;
}
