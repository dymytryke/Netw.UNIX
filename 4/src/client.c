#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include "protocol.h"

// Function to print error messages
void print_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Function to print error code description
void print_error_code(int code) {
    printf("Error: ");
    switch (code) {
        case ERR_PROTOCOL_MISMATCH:
            printf("Protocol version mismatch\n");
            break;
        case ERR_INVALID_FILENAME:
            printf("Invalid filename\n");
            break;
        case ERR_FILE_NOT_FOUND:
            printf("File not found\n");
            break;
        case ERR_FILE_ACCESS:
            printf("Access to file denied\n");
            break;
        case ERR_INTERNAL:
            printf("Internal server error\n");
            break;
        default:
            printf("Unknown error (code %d)\n", code);
    }
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <filename> <max_file_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    const char *server_address = argv[1];
    int server_port = atoi(argv[2]);
    const char *filename = argv[3];
    uint64_t max_file_size = strtoull(argv[4], NULL, 10);

    // Validate filename length
    size_t filename_len = strlen(filename);
    if (filename_len == 0 || filename_len > MAX_FILENAME_LENGTH) {
        fprintf(stderr, "Error: Filename must be between 1 and %d characters\n", MAX_FILENAME_LENGTH);
        exit(EXIT_FAILURE);
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        print_error("Socket creation failed");
    }

    // Connect to server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to server at %s:%d\n", server_address, server_port);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        print_error("Connection failed");
    }
    printf("Connected to server\n");

    // Prepare file request
    FileRequestHeader request;
    memset(&request, 0, sizeof(request));
    request.protocol_version = PROTOCOL_VERSION;
    request.message_type = MSG_FILE_REQUEST;
    request.filename_length = filename_len;
    strncpy(request.filename, filename, MAX_FILENAME_LENGTH);
    
    printf("Sending file request for: %s\n", filename);
    
    // Send file request
    if (send(sock, &request, sizeof(request), 0) != sizeof(request)) {
        print_error("Failed to send file request");
    }

    // Receive server response
    FileResponseHeader response;
    ssize_t bytes_received = recv(sock, &response, sizeof(response), 0);
    if (bytes_received != sizeof(response)) {
        print_error("Failed to receive response header");
    }

    printf("Received response: protocol version %d, message type %d, status %d, file size %lu\n",
           response.protocol_version, response.message_type, response.status, response.file_size);

    // Check protocol version
    if (response.protocol_version != PROTOCOL_VERSION) {
        fprintf(stderr, "Error: Protocol version mismatch\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Check for error
    if (response.status != 0) {
        print_error_code(response.status);
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Check file size against maximum
    ClientResponseHeader client_response;
    if (response.file_size > max_file_size) {
        printf("File size (%lu bytes) exceeds maximum allowed size (%lu bytes)\n", 
               response.file_size, max_file_size);
        client_response.message_type = MSG_REFUSE_TO_RECEIVE;
        send(sock, &client_response, sizeof(client_response), 0);
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Send ready to receive
    client_response.message_type = MSG_READY_TO_RECEIVE;
    printf("Sending ready to receive message\n");
    if (send(sock, &client_response, sizeof(client_response), 0) != sizeof(client_response)) {
        print_error("Failed to send ready message");
    }

    // Create output file
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        print_error("Failed to create output file");
    }

    // Receive file content
    printf("Receiving file content (%lu bytes)...\n", response.file_size);
    uint64_t total_received = 0;
    char buffer[4096];
    
    while (total_received < response.file_size) {
        bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Connection closed by server\n");
                break;
            } else {
                print_error("Error receiving file content");
            }
        }

        if (write(file_fd, buffer, bytes_received) != bytes_received) {
            print_error("Failed to write to output file");
        }

        total_received += bytes_received;
        printf("\rReceived: %lu/%lu bytes (%.1f%%)", 
               total_received, response.file_size, 
               (float)total_received / response.file_size * 100);
        fflush(stdout);
    }
    printf("\nFile transfer complete\n");

    // Close file and socket
    close(file_fd);
    close(sock);

    return 0;
}