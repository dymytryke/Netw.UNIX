#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>

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
    return 1; // Valid numeric port
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

    
}