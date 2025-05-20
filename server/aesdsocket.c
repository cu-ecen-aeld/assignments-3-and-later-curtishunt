#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/queue.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_IO "/var/tmp/aesdsocket"

int exit_flag = 0; // Flag for main loop
int serverfd = -1; // File descriptors

int write_to_file(int clientfd);
void cleanup();
void signal_handler(int sign);
void* handle_connection(void* arg);

typedef struct thread_data {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr client_addr;
    socklen_t addr_len;
    SLIST_ENTRY(thread_data) entries;
} thread_data_t;

SLIST_HEAD(thread_list, thread_data) head;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    thread_data_t *tdata;
    int opt = 1;

    SLIST_INIT(&head);

    openlog(NULL, 0, LOG_USER); /* Initialize syslog */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // check for daemon mode
    if ((argc == 2) && (strcmp(argv[1], "-d") == 0)) {
        pid_t pid = fork();
        if ( pid < 0 ) {
            syslog(LOG_ERR, "Fork Failed.");
            cleanup();
            exit(EXIT_FAILURE);
        }
        if ( pid > 0 ) {
            syslog(LOG_INFO, "Successfully created daemon.");
            exit(EXIT_SUCCESS);
        }

        //stop output to the terminal
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    // Create Server Socket
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) { //socket error handling
        syslog(LOG_ERR, "Error creating socket");
        cleanup();
        exit(EXIT_FAILURE);
    }
    printf("created socket\n");

    // Set SO_REUSEADDR option
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "ERROR setting SO_REUSEADDR");
        cleanup();
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); 

    // Bind socket to address
    if (bind(serverfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "ERROR on binding");
        exit(EXIT_FAILURE);
    }
    printf("bound to address\n");

    // Listen for connections
    if (listen(serverfd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Error listening on socket.");
        close(serverfd);
        cleanup();
        exit(EXIT_FAILURE);
    }

    pthread_t timestamp_tid;
    if(pthread_create(&timestamp_tid, NULL, timestamp_thread, NULL) != 0){
        syslog(LOG_ERR, "Error creating thread.");
        printf("failed to create thread\n");
        close(serverfd);
        free(tdata);
        exit(EXIT_FAILURE);
    }
    
    printf("Thread created!\n");

    int addr_len = sizeof(struct sockaddr_in);

    while( !exit_flag ) {

        struct sockaddr_storage connect_addr;
        socklen_t sin_size = sizeof(connect_addr);
        int *clientfd = malloc(sizeof(int));
        if(!clientfd){
            syslog(LOG_ERR, "Failed to allocate client fd");
            sleep(1);
            continue;
        }

        printf("waiting for connection");
        // Accepting client connections
        *clientfd = accept(serverfd, (struct sockaddr *)&connect_addr, &sin_size);
        if (*clientfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection.");
            printf("failed to accept connection\n");
            exit(EXIT_FAILURE);
        }

        printf("accepted connection, creating thread...");
        // Create thread for new connection


        SLIST_INSERT_HEAD(&head, tdata, entries);

    }


    // Join all threads
    thread_data_t *cur, *tmp;
    SLIST_FOREACH(cur, &head, entries); {
        pthread_join(cur->thread_id, NULL);
        SLIST_REMOVE(&head, cur, thread_data, entries);
        free(cur);
    }

    syslog(LOG_INFO, "Graceful exit!!");
    printf("graceful exit");
    cleanup();
    return 0;

}


void* handle_connection(void* arg) {
    thread_data_t* tdata = (thread_data_t*)arg;

    // Log accepted connection
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tdata->client_addr, client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    int result = write_to_file(tdata->client_fd);
    if (result < 0) {
        syslog(LOG_ERR, "Error handling request from %s", client_ip);
    } else if (result == 0) {
        // Indicate that the client has disconnected properly
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    close(tdata->client_fd);
    tdata->client_fd = -1;
}

//cleanup steps
void cleanup() {

    if (serverfd >= 0) close(serverfd);
    if (clientfd >= 0) close(clientfd);
    if (remove(FILE_IO) != 0) syslog(LOG_ERR, "Failed to remove file: %s", strerror(errno));
    syslog(LOG_INFO, "Cleanup was reached!");
    closelog();



}

//Signal Handler
void signal_handler(int sign) {
    if (sign == SIGINT || sign == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        exit_flag = 1;
        cleanup();
        return;
    }
}

// Rec and Send file data from server and client
int write_to_file(int clientfd) {
    int valread;
    char buffer[1024];

    // Open File to write or append to
    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen(FILE_IO, "a+");
    if (fp == NULL) {
        syslog(LOG_ERR, "Server failed to open file.");
        exit(EXIT_FAILURE);
    }

    // Write string to file until eol is reached
    while ((valread = recv(clientfd, buffer, sizeof(buffer) -1, 0)) > 0) {
        buffer[valread] = '\0';
        fputs(buffer, fp);
        fflush(fp); 
        if (strchr(buffer, '\n')) break;
    }

    // Send file back to client until eof is reached
    fseek(fp, 0, SEEK_SET);
    while ((valread = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(clientfd, buffer, valread, 0);
    }
    fclose(fp);
    pthread_mutex_unlock(&file_mutex);
    return 0;
}
