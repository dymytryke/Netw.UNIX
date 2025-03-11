#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include "protocol.h"
#include "utils.h"

// Function to print error messages
void print_error(const char *message) {
    perror(message);
}

// Function to handle a client connection
void handle_client(int client_sock, const char *directory) {
    // Receive file request
    FileRequestHeader request;
    ssize_t bytes_received = recv(client_sock, &request, sizeof(request), 0);
    
    if (bytes_received != sizeof(request)) {
        printf("Error: Failed to receive file request (received %zd bytes, expected %zu)\n", 
               bytes_received, sizeof(request));
        close(client_sock);
        return;
    }
    
    // Prepare response
    FileResponseHeader response;
    memset(&response, 0, sizeof(response));
    response.protocol_version = PROTOCOL_VERSION;
    response.message_type = MSG_FILE_RESPONSE;
    
    // Check protocol version
    if (request.protocol_version != PROTOCOL_VERSION) {
        printf("Error: Protocol version mismatch (client: %d, server: %d)\n", 
               request.protocol_version, PROTOCOL_VERSION);
        response.status = ERR_PROTOCOL_MISMATCH;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    // Null-terminate filename to be safe
    request.filename[request.filename_length] = '\0';
    
    printf("Received file request for: %s\n", request.filename);
    
    // Validate filename
    if (!validate_filename(request.filename)) {
        printf("Error: Invalid filename: %s\n", request.filename);
        response.status = ERR_INVALID_FILENAME;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    // Build full path
    char filepath[PATH_MAX];
    snprintf(filepath, sizeof(filepath), "%s/%s", directory, request.filename);
    
    // Check if the file exists and is a regular file
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        printf("Error: File not found: %s\n", filepath);
        response.status = ERR_FILE_NOT_FOUND;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    if (!S_ISREG(file_stat.st_mode)) {
        printf("Error: Not a regular file: %s\n", filepath);
        response.status = ERR_FILE_NOT_FOUND;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    // Get file size
    uint64_t file_size = file_stat.st_size;
    
    // Prepare success response
    response.status = 0;  // Success
    response.file_size = file_size;
    
    printf("Sending file response: status=%d, file_size=%lu\n", 
           response.status, response.file_size);
    
    // Send response
    if (send(client_sock, &response, sizeof(response), 0) != sizeof(response)) {
        print_error("Failed to send file response");
        close(client_sock);
        return;
    }
    
    // Receive client's decision
    ClientResponseHeader client_response;
    bytes_received = recv(client_sock, &client_response, sizeof(client_response), 0);
    
    if (bytes_received != sizeof(client_response)) {
        printf("Error: Failed to receive client response\n");
        close(client_sock);
        return;
    }
    
    if (client_response.message_type == MSG_REFUSE_TO_RECEIVE) {
        printf("Client refused to receive the file (too large)\n");
        close(client_sock);
        return;
    }
    
    if (client_response.message_type != MSG_READY_TO_RECEIVE) {
        printf("Error: Unexpected client response message type: %d\n", client_response.message_type);
        close(client_sock);
        return;
    }
    
    printf("Client is ready to receive the file\n");
    
    // Open the file
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        print_error("Failed to open file");
        close(client_sock);
        return;
    }
    
    // Send file content in chunks
    char buffer[DEFAULT_CHUNK_SIZE];
    ssize_t bytes_read;
    uint64_t total_sent = 0;
    
    printf("Sending file content (%lu bytes)...\n", file_size);
    
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = send(client_sock, buffer, bytes_read, 0);
        if (bytes_sent != bytes_read) {
            print_error("Failed to send file content");
            close(file_fd);
            close(client_sock);
            return;
        }
        
        total_sent += bytes_sent;
        printf("\rSent: %lu/%lu bytes (%.1f%%)", 
               total_sent, file_size, 
               (float)total_sent / file_size * 100);
        fflush(stdout);
    }
    
    printf("\nFile transfer complete\n");
    
    // Close file and client connection
    close(file_fd);
    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    const char *server_address = argv[1];
    int server_port = atoi(argv[2]);
    const char *directory = argv[3];

    // Check if directory exists
    DIR *dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "Error: Directory '%s' does not exist or is not accessible\n", directory);
        exit(EXIT_FAILURE);
    }
    closedir(dir);

    // Create socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to specified address and port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        exit(EXIT_FAILURE);
    }

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Iterative server started at %s:%d, serving files from '%s'\n", 
           server_address, server_port, directory);

    // Main server loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        printf("Waiting for connections...\n");
        
        // Accept a new connection
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            print_error("Accept failed");
            continue;  // Continue accepting connections
        }
        
        // Get client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        
        printf("Accepted connection from %s:%d\n", client_ip, client_port);
        
        // Handle client in the same process (iterative server)
        handle_client(client_sock, directory);
        
        printf("Client connection handled and closed\n");
    }

    // Close server socket (never reached in this example)
    close(server_sock);
    
    return 0;
}