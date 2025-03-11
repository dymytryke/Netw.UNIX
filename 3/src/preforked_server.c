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
#include <sys/wait.h>
#include <signal.h>
#include "protocol.h"
#include "utils.h"

// Global variables for signal handling
volatile sig_atomic_t terminate = 0;  // Flag to terminate child processes

// Function to print error messages
void print_error(const char *message) {
    perror(message);
}

// Signal handler for termination signals
void termination_handler(int sig) {
    terminate = 1;
}

// Function to handle a client connection
void handle_client(int client_sock, const char *directory) {
    // Receive file request
    FileRequestHeader request;
    ssize_t bytes_received = recv(client_sock, &request, sizeof(request), 0);
    
    if (bytes_received != sizeof(request)) {
        printf("[PID %d] Error: Failed to receive file request (received %zd bytes, expected %zu)\n", 
               getpid(), bytes_received, sizeof(request));
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
        printf("[PID %d] Error: Protocol version mismatch (client: %d, server: %d)\n", 
               getpid(), request.protocol_version, PROTOCOL_VERSION);
        response.status = ERR_PROTOCOL_MISMATCH;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    // Null-terminate filename to be safe
    request.filename[request.filename_length] = '\0';
    
    printf("[PID %d] Received file request for: %s\n", getpid(), request.filename);
    
    // Validate filename
    if (!validate_filename(request.filename)) {
        printf("[PID %d] Error: Invalid filename: %s\n", getpid(), request.filename);
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
        printf("[PID %d] Error: File not found: %s\n", getpid(), filepath);
        response.status = ERR_FILE_NOT_FOUND;
        send(client_sock, &response, sizeof(response), 0);
        close(client_sock);
        return;
    }
    
    if (!S_ISREG(file_stat.st_mode)) {
        printf("[PID %d] Error: Not a regular file: %s\n", getpid(), filepath);
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
    
    printf("[PID %d] Sending file response: status=%d, file_size=%lu\n", 
           getpid(), response.status, response.file_size);
    
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
        printf("[PID %d] Error: Failed to receive client response\n", getpid());
        close(client_sock);
        return;
    }
    
    if (client_response.message_type == MSG_REFUSE_TO_RECEIVE) {
        printf("[PID %d] Client refused to receive the file (too large)\n", getpid());
        close(client_sock);
        return;
    }
    
    if (client_response.message_type != MSG_READY_TO_RECEIVE) {
        printf("[PID %d] Error: Unexpected client response message type: %d\n", 
               getpid(), client_response.message_type);
        close(client_sock);
        return;
    }
    
    printf("[PID %d] Client is ready to receive the file\n", getpid());
    
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
    
    printf("[PID %d] Sending file content (%lu bytes)...\n", getpid(), file_size);
    
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = send(client_sock, buffer, bytes_read, 0);
        if (bytes_sent != bytes_read) {
            print_error("Failed to send file content");
            close(file_fd);
            close(client_sock);
            return;
        }
        
        total_sent += bytes_sent;
        printf("[PID %d] \rSent: %lu/%lu bytes (%.1f%%)", 
               getpid(), total_sent, file_size, 
               (float)total_sent / file_size * 100);
        fflush(stdout);
    }
    
    printf("\n[PID %d] File transfer complete\n", getpid());
    
    // Close file and client connection
    close(file_fd);
    close(client_sock);
}

// Function for child process to handle client connections
void child_process(int server_sock, const char *directory) {
    // Set up signal handler for termination
    struct sigaction sa;
    sa.sa_handler = termination_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    
    printf("[PID %d] Child process started, waiting for connections\n", getpid());
    
    while (!terminate) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept a new connection
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we need to terminate
                if (terminate) {
                    break;
                }
                continue;
            }
            print_error("Accept failed");
            continue;  // Continue accepting connections
        }
        
        // Get client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);
        
        printf("[PID %d] Accepted connection from %s:%d\n", getpid(), client_ip, client_port);
        
        // Handle the client
        handle_client(client_sock, directory);
        
        printf("[PID %d] Client connection handled and closed\n", getpid());
    }
    
    printf("[PID %d] Child process terminating\n", getpid());
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory> <num_children>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    const char *server_address = argv[1];
    int server_port = atoi(argv[2]);
    const char *directory = argv[3];
    int num_children = atoi(argv[4]);
    
    if (num_children <= 0) {
        fprintf(stderr, "Error: num_children must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

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

    printf("Pre-forked server started at %s:%d, serving files from '%s', creating %d child processes\n", 
           server_address, server_port, directory, num_children);

    // Store child PIDs
    pid_t *child_pids = malloc(num_children * sizeof(pid_t));
    if (!child_pids) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    
    // Pre-fork child processes
    for (int i = 0; i < num_children; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
        
        if (pid == 0) {
            // Child process
            free(child_pids);  // Child doesn't need this array
            child_process(server_sock, directory);
            // Should not reach here
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            child_pids[i] = pid;
            printf("Child process created (PID: %d, %d/%d)\n", pid, i + 1, num_children);
        }
    }
    
    printf("All child processes created. Parent process is now waiting.\n");

    // Set up signal handler for parent to pass signals to children
    struct sigaction sa;
    sa.sa_handler = termination_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    // Wait for termination signal
    while (!terminate) {
        sleep(1);
    }
    
    printf("Parent process received termination signal. Shutting down...\n");
    
    // Signal all children to terminate
    for (int i = 0; i < num_children; i++) {
        printf("Sending SIGTERM to child process %d\n", child_pids[i]);
        kill(child_pids[i], SIGTERM);
    }
    
    // Wait for all children to exit
    printf("Waiting for all child processes to exit...\n");
    for (int i = 0; i < num_children; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        printf("Child process %d exited with status %d\n", child_pids[i], 
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    
    free(child_pids);
    close(server_sock);
    printf("Server shutdown complete\n");
    
    return 0;
}