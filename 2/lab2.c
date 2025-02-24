#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

void print_usage(const char *prog_name)
{
    printf("Usage: %s [-t] [-v <4|6>] [-d] [-s] -a <ip/hostname> -p <port/service>\n", prog_name);
    printf("  -a: IP address or hostname to be converted\n");
    printf("  -p: Port number or service name to be converted\n");
    printf("  -t: Prohibit domain names and service names (use only numeric values)\n");
    printf("  -v <4|6>: Use specific IP version (IPv4 or IPv6)\n");
    printf("  -d: Output domain name for specified IP\n");
    printf("  -s: Output service name for specified port\n");
}

// Function to check if a string represents a valid numeric IP address
int is_valid_ip(const char *ip)
{
    struct in_addr ipv4_addr;
    struct in6_addr ipv6_addr;

    if (inet_pton(AF_INET, ip, &ipv4_addr) == 1)
    {
        return 1; // Valid IPv4
    }
    if (inet_pton(AF_INET6, ip, &ipv6_addr) == 1)
    {
        return 1; // Valid IPv6
    }
    return 0; // Not a valid IP address
}

// Function to check if a string contains only digits (valid port number)
int is_valid_port(const char *port)
{
    for (int i = 0; port[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)port[i]))
        {
            return 0; // Contains non-numeric character
        }
    }
    
    int port_num = atoi(port);
    return (port_num >= 0 && port_num <= 65535); // Valid port range
}

// Function to print protocol info
void print_protocol_info(int protocol)
{
    struct protoent *proto = getprotobynumber(protocol);
    printf("Protocol: %d", protocol);
    if (proto != NULL) {
        printf(" (%s)", proto->p_name);
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
    int opt;
    char *hostname = NULL;
    char *port = NULL;
    int only_numeric = 0;
    int ip_version = 0; // 4 for IPv4, 6 for IPv6, 0 for any
    int show_domain = 0;
    int show_service = 0;

    // Обробка аргументів командного рядка
    while ((opt = getopt(argc, argv, "a:p:tv:ds")) != -1)
    {
        switch (opt)
        {
        case 'a':
            hostname = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 't':
            only_numeric = 1;
            break;
        case 'v':
            if (strcmp(optarg, "4") == 0)
                ip_version = 4;
            else if (strcmp(optarg, "6") == 0)
                ip_version = 6;
            else
            {
                fprintf(stderr, "Invalid IP version: %s\n", optarg);
                return 1;
            }
            break;
        case 'd':
            show_domain = 1;
            break;
        case 's':
            show_service = 1;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!hostname || !port)
    {
        fprintf(stderr, "Missing required arguments: -a <ip/hostname> and -p <port/service>\n");
        print_usage(argv[0]);
        return 1;
    }

    // If -t is specified, ensure -a contains only a numeric IP and -p contains only a numeric port
    if (only_numeric)
    {
        if (!is_valid_ip(hostname))
        {
            fprintf(stderr, "Error: With -t, the argument for -a must be a numeric IP address.\n");
            return 1;
        }
        if (!is_valid_port(port))
        {
            fprintf(stderr, "Error: With -t, the argument for -p must be a numeric port number.\n");
            return 1;
        }
    }

    // Prepare hints for getaddrinfo
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    
    if (ip_version == 4)
        hints.ai_family = AF_INET;    // IPv4
    else if (ip_version == 6)
        hints.ai_family = AF_INET6;   // IPv6
    else
        hints.ai_family = AF_UNSPEC;  // Allow any address family
        
    hints.ai_socktype = SOCK_STREAM;
    
    if (only_numeric) {
        hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV; // Numeric host and service
    }

    // Get address info
    int status = getaddrinfo(hostname, port, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // Print the results
    printf("Results for %s:%s\n", hostname, port);
    printf("------------------------\n");
    
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        char addrstr[INET6_ADDRSTRLEN];
        void *addr;
        int port_num;
        
        // Get the pointer to the address itself
        if (rp->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
            port_num = ntohs(ipv4->sin_port);
            printf("IPv4 Address: ");
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
            port_num = ntohs(ipv6->sin6_port);
            printf("IPv6 Address: ");
        }
        
        // Convert IP to string
        inet_ntop(rp->ai_family, addr, addrstr, sizeof(addrstr));
        printf("%s\n", addrstr);
        
        // Print port
        printf("Port: %d\n", port_num);
        
        // Print protocol info
        print_protocol_info(rp->ai_protocol);
        
        // If -d is specified, try to resolve hostname
        if (show_domain) {
            struct sockaddr_storage temp_addr;
            memcpy(&temp_addr, rp->ai_addr, rp->ai_addrlen);
            char host[NI_MAXHOST];
            
            if (getnameinfo((struct sockaddr*)&temp_addr, rp->ai_addrlen, 
                           host, NI_MAXHOST, NULL, 0, 0) == 0) {
                printf("Hostname: %s\n", host);
            } else {
                printf("Hostname: Unable to resolve\n");
            }
        }
        
        // If -s is specified, try to resolve service name
        if (show_service) {
            struct servent *service = getservbyport(htons(port_num), 
                                                 (rp->ai_socktype == SOCK_DGRAM) ? "udp" : "tcp");
            if (service != NULL) {
                printf("Service: %s\n", service->s_name);
            } else {
                printf("Service: Unknown\n");
            }
        }
        
        printf("Socket type: %s\n", 
              (rp->ai_socktype == SOCK_STREAM) ? "SOCK_STREAM" :
              (rp->ai_socktype == SOCK_DGRAM) ? "SOCK_DGRAM" :
              (rp->ai_socktype == SOCK_RAW) ? "SOCK_RAW" : "Unknown");
              
        printf("------------------------\n");
    }

    freeaddrinfo(result);
    return 0;
}