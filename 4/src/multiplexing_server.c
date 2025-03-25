#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include "protocol.h"
#include "utils.h"

// Maximum data chunk to read/write in a single operation
#define MAX_IO_CHUNK 512

// Client connection states
typedef enum {
    STATE_READ_REQUEST,      // Reading file request from client
    STATE_SEND_RESPONSE,     // Sending file response to client
    STATE_WAIT_DECISION,     // Waiting for client's decision to receive file
    STATE_SEND_FILE,         // Sending file content to client
    STATE_DONE               // Client handled, connection can be closed
} ClientState;

// Client context structure
typedef struct {
    int socket;                              // Client socket descriptor
    ClientState state;                       // Current state
    time_t last_activity;                    // Time of last activity
    
    // Request handling
    FileRequestHeader request;               // Client request
    size_t request_bytes_read;               // How much of the request has been read
    
    // Response handling
    FileResponseHeader response;             // Server response
    size_t response_bytes_sent;              // How much of the response has been sent
    
    // Client decision handling
    ClientResponseHeader client_response;    // Client's decision response
    size_t decision_bytes_read;              // How much of the decision has been read
    
    // File transfer
    int file_fd;                             // File descriptor
    uint64_t file_size;                      // Size of the file
    uint64_t file_bytes_sent;                // How much of the file has been sent
    char filepath[PATH_MAX];                 // Full path to the file
    
    char buffer[MAX_IO_CHUNK];               // Buffer for I/O operations
} ClientContext;

// Global variables
char server_directory[PATH_MAX];             // Directory to serve files from
int max_clients = 0;                         // Maximum number of clients
int active_clients = 0;                      // Number of active client connections

// Function prototypes
void setup_server_socket(int *server_sock, const char *server_address, int server_port);
void set_nonblocking(int socket_fd);
int accept_new_client(int server_sock, struct pollfd *fds, ClientContext *contexts, int *nfds);
void handle_client_io(struct pollfd *fd, ClientContext *context);
void cleanup_client(struct pollfd *fds, ClientContext *contexts, int index, int *nfds);
void process_read_request(struct pollfd *fd, ClientContext *context);
void process_send_response(struct pollfd *fd, ClientContext *context);
void process_wait_decision(struct pollfd *fd, ClientContext *context);
void process_send_file(struct pollfd *fd, ClientContext *context);

// Main function
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <directory> <max_clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    const char *server_address = argv[1];
    int server_port = atoi(argv[2]);
    strncpy(server_directory, argv[3], sizeof(server_directory) - 1);
    max_clients = atoi(argv[4]);
    
    if (max_clients <= 0) {
        fprintf(stderr, "Error: max_clients must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    // Check if directory exists
    DIR *dir = opendir(server_directory);
    if (!dir) {
        fprintf(stderr, "Error: Directory '%s' does not exist or is not accessible\n", server_directory);
        exit(EXIT_FAILURE);
    }
    closedir(dir);

    // Create and setup server socket
    int server_sock;
    setup_server_socket(&server_sock, server_address, server_port);
    
    // Set server socket to non-blocking mode
    set_nonblocking(server_sock);

    // Allocate memory for poll structures and client contexts
    // +1 for server socket
    struct pollfd *fds = malloc((max_clients + 1) * sizeof(struct pollfd));
    ClientContext *contexts = malloc((max_clients + 1) * sizeof(ClientContext));
    
    if (!fds || !contexts) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Initialize server socket in poll structure
    fds[0].fd = server_sock;
    fds[0].events = POLLIN;
    int nfds = 1;

    printf("Multiplexing server started at %s:%d, serving files from '%s', max clients: %d\n",
           server_address, server_port, server_directory, max_clients);

    // Main server loop
    while (1) {
        // Wait for events on sockets
        int poll_result = poll(fds, nfds, -1);
        
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted system call, try again
            }
            perror("Poll failed");
            break;
        }
        
        // Check for events on all file descriptors
        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents == 0) {
                continue;  // No events on this descriptor
            }
            
            // Check for error conditions
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (i == 0) {
                    // Error on server socket, critical error
                    fprintf(stderr, "Error on server socket, exiting\n");
                    exit(EXIT_FAILURE);
                } else {
                    // Error on client socket, close connection
                    printf("Socket error (fd=%d), closing connection\n", fds[i].fd);
                    cleanup_client(fds, contexts, i, &nfds);
                    continue;
                }
            }
            
            // Handle incoming connection on server socket
            if (i == 0 && (fds[i].revents & POLLIN)) {
                if (active_clients < max_clients) {
                    if (accept_new_client(server_sock, fds, contexts, &nfds)) {
                        active_clients++;
                    }
                }
                continue;
            }
            
            // Handle client I/O
            if (fds[i].revents & (POLLIN | POLLOUT)) {
                handle_client_io(&fds[i], &contexts[i]);
                
                // Check if client is done and needs to be closed
                if (contexts[i].state == STATE_DONE) {
                    cleanup_client(fds, contexts, i, &nfds);
                    active_clients--;
                } else {
                    // Update last activity time
                    contexts[i].last_activity = time(NULL);
                }
            }
        }
    }

    // Cleanup
    close(server_sock);
    free(fds);
    free(contexts);

    return 0;
}

// Set up server socket
void setup_server_socket(int *server_sock, const char *server_address, int server_port) {
    *server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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

    if (bind(*server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(*server_sock, max_clients) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

// Set socket to non-blocking mode
void set_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        exit(EXIT_FAILURE);
    }
}

// Accept new client connection
int accept_new_client(int server_sock, struct pollfd *fds, ClientContext *contexts, int *nfds) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Accept connection
    int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No pending connections
        }
        perror("Accept failed");
        return 0;
    }
    
    // Set client socket to non-blocking mode
    set_nonblocking(client_sock);
    
    // Get client information
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    printf("Accepted connection from %s:%d (fd=%d)\n", client_ip, client_port, client_sock);
    
    // Initialize client context
    int index = *nfds;
    fds[index].fd = client_sock;
    fds[index].events = POLLIN;  // Initially, wait for client request
    
    memset(&contexts[index], 0, sizeof(ClientContext));
    contexts[index].socket = client_sock;
    contexts[index].state = STATE_READ_REQUEST;
    contexts[index].last_activity = time(NULL);
    contexts[index].file_fd = -1;
    
    (*nfds)++;
    return 1;
}

// Handle client I/O based on current state
void handle_client_io(struct pollfd *fd, ClientContext *context) {
    switch (context->state) {
        case STATE_READ_REQUEST:
            process_read_request(fd, context);
            break;
        case STATE_SEND_RESPONSE:
            process_send_response(fd, context);
            break;
        case STATE_WAIT_DECISION:
            process_wait_decision(fd, context);
            break;
        case STATE_SEND_FILE:
            process_send_file(fd, context);
            break;
        case STATE_DONE:
            // Nothing to do, will be cleaned up in main loop
            break;
    }
}

// Process reading client request
void process_read_request(struct pollfd *fd, ClientContext *context) {
    // Calculate how many bytes we still need to read
    size_t bytes_to_read = sizeof(FileRequestHeader) - context->request_bytes_read;
    
    // Limit by MAX_IO_CHUNK
    if (bytes_to_read > MAX_IO_CHUNK)
        bytes_to_read = MAX_IO_CHUNK;
    
    // Read from socket
    char *buffer_ptr = (char *)&context->request + context->request_bytes_read;
    ssize_t bytes_read = read(context->socket, buffer_ptr, bytes_to_read);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // No data available, try again later
        }
        perror("Read error");
        context->state = STATE_DONE;
        return;
    }
    
    if (bytes_read == 0) {
        printf("Client closed connection during request read\n");
        context->state = STATE_DONE;
        return;
    }
    
    context->request_bytes_read += bytes_read;
    
    // If we've read the entire request
    if (context->request_bytes_read == sizeof(FileRequestHeader)) {
        // Prepare response
        memset(&context->response, 0, sizeof(FileResponseHeader));
        context->response.protocol_version = PROTOCOL_VERSION;
        context->response.message_type = MSG_FILE_RESPONSE;
        
        // Check protocol version
        if (context->request.protocol_version != PROTOCOL_VERSION) {
            printf("Error: Protocol version mismatch (client: %d, server: %d)\n", 
                   context->request.protocol_version, PROTOCOL_VERSION);
            context->response.status = ERR_PROTOCOL_MISMATCH;
            context->state = STATE_SEND_RESPONSE;
            fd->events = POLLOUT;
            return;
        }
        
        // Null-terminate filename to be safe
        context->request.filename[context->request.filename_length] = '\0';
        
        printf("Received file request from fd=%d for: %s\n", context->socket, context->request.filename);
        
        // Validate filename
        if (!validate_filename(context->request.filename)) {
            printf("Error: Invalid filename: %s\n", context->request.filename);
            context->response.status = ERR_INVALID_FILENAME;
            context->state = STATE_SEND_RESPONSE;
            fd->events = POLLOUT;
            return;
        }
        
        // Build full path
        snprintf(context->filepath, sizeof(context->filepath), "%s/%s", 
                server_directory, context->request.filename);
        
        // Check if the file exists and is a regular file
        struct stat file_stat;
        if (stat(context->filepath, &file_stat) != 0) {
            printf("Error: File not found: %s\n", context->filepath);
            context->response.status = ERR_FILE_NOT_FOUND;
            context->state = STATE_SEND_RESPONSE;
            fd->events = POLLOUT;
            return;
        }
        
        if (!S_ISREG(file_stat.st_mode)) {
            printf("Error: Not a regular file: %s\n", context->filepath);
            context->response.status = ERR_FILE_NOT_FOUND;
            context->state = STATE_SEND_RESPONSE;
            fd->events = POLLOUT;
            return;
        }
        
        // Get file size
        context->file_size = file_stat.st_size;
        
        // Prepare success response
        context->response.status = 0;  // Success
        context->response.file_size = context->file_size;
        
        printf("Sending file response to fd=%d: status=%d, file_size=%lu\n", 
               context->socket, context->response.status, context->response.file_size);
        
        // Change state and update poll events
        context->state = STATE_SEND_RESPONSE;
        fd->events = POLLOUT;
    }
}

// Process sending response to client
void process_send_response(struct pollfd *fd, ClientContext *context) {
    // Calculate how many bytes we still need to send
    size_t bytes_to_send = sizeof(FileResponseHeader) - context->response_bytes_sent;
    
    // Limit by MAX_IO_CHUNK
    if (bytes_to_send > MAX_IO_CHUNK)
        bytes_to_send = MAX_IO_CHUNK;
    
    // Write to socket
    char *buffer_ptr = (char *)&context->response + context->response_bytes_sent;
    ssize_t bytes_sent = write(context->socket, buffer_ptr, bytes_to_send);
    
    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // Buffer full, try again later
        }
        perror("Write error");
        context->state = STATE_DONE;
        return;
    }
    
    context->response_bytes_sent += bytes_sent;
    
    // If we've sent the entire response
    if (context->response_bytes_sent == sizeof(FileResponseHeader)) {
        // If response indicated an error, we're done
        if (context->response.status != 0) {
            context->state = STATE_DONE;
            return;
        }
        
        // Wait for client's decision
        context->state = STATE_WAIT_DECISION;
        fd->events = POLLIN;
    }
}

// Process waiting for client decision
void process_wait_decision(struct pollfd *fd, ClientContext *context) {
    // Calculate how many bytes we still need to read
    size_t bytes_to_read = sizeof(ClientResponseHeader) - context->decision_bytes_read;
    
    // Limit by MAX_IO_CHUNK
    if (bytes_to_read > MAX_IO_CHUNK)
        bytes_to_read = MAX_IO_CHUNK;
    
    // Read from socket
    char *buffer_ptr = (char *)&context->client_response + context->decision_bytes_read;
    ssize_t bytes_read = read(context->socket, buffer_ptr, bytes_to_read);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // No data available, try again later
        }
        perror("Read error");
        context->state = STATE_DONE;
        return;
    }
    
    if (bytes_read == 0) {
        printf("Client closed connection during decision read\n");
        context->state = STATE_DONE;
        return;
    }
    
    context->decision_bytes_read += bytes_read;
    
    // If we've read the entire decision
    if (context->decision_bytes_read == sizeof(ClientResponseHeader)) {
        if (context->client_response.message_type == MSG_REFUSE_TO_RECEIVE) {
            printf("Client fd=%d refused to receive the file (too large)\n", context->socket);
            context->state = STATE_DONE;
            return;
        }
        
        if (context->client_response.message_type != MSG_READY_TO_RECEIVE) {
            printf("Error: Unexpected client response message type: %d\n", 
                   context->client_response.message_type);
            context->state = STATE_DONE;
            return;
        }
        
        printf("Client fd=%d is ready to receive the file\n", context->socket);
        
        // Open the file
        context->file_fd = open(context->filepath, O_RDONLY);
        if (context->file_fd < 0) {
            perror("Failed to open file");
            context->state = STATE_DONE;
            return;
        }
        
        // Change state to send file and update poll events
        context->state = STATE_SEND_FILE;
        context->file_bytes_sent = 0;
        fd->events = POLLOUT;
    }
}

// Process sending file content
void process_send_file(struct pollfd *fd, ClientContext *context) {
    // If we've sent the entire file
    if (context->file_bytes_sent >= context->file_size) {
        printf("\nFile transfer complete for client fd=%d\n", context->socket);
        // Close file descriptor and mark as done
        if (context->file_fd >= 0) {
            close(context->file_fd);
            context->file_fd = -1;
        }
        context->state = STATE_DONE;
        return;
    }
    
    // Read from file into buffer (limited by MAX_IO_CHUNK)
    size_t bytes_to_read = MAX_IO_CHUNK;
    if (context->file_size - context->file_bytes_sent < bytes_to_read) {
        bytes_to_read = context->file_size - context->file_bytes_sent;
    }
    
    ssize_t bytes_read = read(context->file_fd, context->buffer, bytes_to_read);
    
    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            perror("File read error");
        }
        // End of file or error
        context->state = STATE_DONE;
        close(context->file_fd);
        context->file_fd = -1;
        return;
    }
    
    // Send buffer to socket
    ssize_t bytes_sent = write(context->socket, context->buffer, bytes_read);
    
    if (bytes_sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Rewind file pointer as we couldn't send data
            if (lseek(context->file_fd, -bytes_read, SEEK_CUR) < 0) {
                perror("lseek error");
                context->state = STATE_DONE;
                close(context->file_fd);
                context->file_fd = -1;
            }
            return;  // Buffer full, try again later
        }
        perror("Write error");
        context->state = STATE_DONE;
        close(context->file_fd);
        context->file_fd = -1;
        return;
    }
    
    if (bytes_sent < bytes_read) {
        // We couldn't send all bytes, rewind file pointer
        off_t rewind_offset = bytes_read - bytes_sent;
        if (lseek(context->file_fd, -rewind_offset, SEEK_CUR) < 0) {
            perror("lseek error");
            context->state = STATE_DONE;
            close(context->file_fd);
            context->file_fd = -1;
            return;
        }
    }
    
    // Update bytes sent
    context->file_bytes_sent += bytes_sent;
    
    // Print progress (only for significant changes to avoid console flooding)
    static int last_percent[FD_SETSIZE] = {0};
    int current_percent = (int)((double)context->file_bytes_sent / context->file_size * 100);
    
    if (current_percent > last_percent[context->socket] || 
        context->file_bytes_sent >= context->file_size) {
        printf("\rClient fd=%d: Sent %lu/%lu bytes (%d%%)", 
               context->socket, context->file_bytes_sent, context->file_size, current_percent);
        fflush(stdout);
        last_percent[context->socket] = current_percent;
    }
}

// Clean up client resources and remove from poll array
void cleanup_client(struct pollfd *fds, ClientContext *contexts, int index, int *nfds) {
    // Close file descriptor if open
    if (contexts[index].file_fd >= 0) {
        close(contexts[index].file_fd);
    }
    
    // Close socket
    close(fds[index].fd);
    printf("Closed connection for client fd=%d\n", fds[index].fd);
    
    // Remove from poll array by shifting remaining descriptors
    (*nfds)--;
    for (int i = index; i < *nfds; i++) {
        fds[i] = fds[i + 1];
        contexts[i] = contexts[i + 1];
    }
}