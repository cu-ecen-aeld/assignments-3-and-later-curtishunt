#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_IO "/var/tmp/aesdsocket"
#define BUFFER_SIZE 1024

int exit_flag = 0; // Flag for main loop
int serverfd = -1; // File descriptors

// Timer globabl var
timer_t timer_id;

//function declarations
void signal_handler(int sign);
void *handle_connection(void* arg);
void timer_handler(union sigval dummyval);
void setup_timer();

// struct type for linked list
typedef struct thread_data {
    pthread_t thread_id;
    SLIST_ENTRY(thread_data) entries;
} thread_data_t;
// linked list init
SLIST_HEAD(thread_list, thread_data) head;

// mutex init
pthread_mutex_t file_mutex;

int main(int argc, char *argv[]) {
    
    

    SLIST_INIT(&head);

    // Open System Log
    openlog(NULL, 0, LOG_USER); /* Initialize syslog */
    
    // setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create Server Socket
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) { //socket error handling
        syslog(LOG_ERR, "Error creating socket");
        closelog();
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "ERROR setting SO_REUSEADDR");
        close(serverfd);
        closelog();
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Successfully created socket with id: %d", serverfd);


    // Initialize server address structure
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); 

    // Bind socket to address
    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "ERROR on binding");
        close(serverfd);
        closelog();
        exit(EXIT_FAILURE);
    } else {
        syslog(LOG_INFO, "Successfully bound socket to server");
    }

    // check for daemon mode
    if ((argc == 2) && (strcmp(argv[1], "-d") == 0)) {
        pid_t pid = fork();
        if ( pid < 0 ) {
            syslog(LOG_ERR, "Fork Failed.");
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
    } else {
        syslog(LOG_INFO, "Running in normal mode");
    }

    // Listen for connections
    if (listen(serverfd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Error listening on socket.");
        close(serverfd);
        closelog();
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Listening on port %d", PORT);

    pthread_mutex_init(&file_mutex, NULL);
    SLIST_INIT(&head);
    setup_timer();

    while( true ) {

        // Accepting client connections
        int clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &client_len);
        if (clientfd < 0) {
            syslog(LOG_ERR, "Failed to accept connection.");
            break;
        } else {
            syslog(LOG_ERR, "Accepted connection, creating thread");
        }

        // Create thread for new connection
        pthread_t client_id;
        int *client_sockfd_ptr = malloc(sizeof(int));
        *client_sockfd_ptr = clientfd;
        if(pthread_create(&client_id, NULL, handle_connection, client_sockfd_ptr) != 0){
            syslog(LOG_ERR, "Failed to create client thread");
            close(clientfd);
            free(client_sockfd_ptr);
            continue;
        }

        // allocate mem and add thread to linked list
        struct thread_data *tdata = malloc(sizeof(struct thread_data));
        tdata->thread_id = client_id;
        SLIST_INSERT_HEAD(&head, tdata, entries);

    }

    // We clean up once we exit the while loop
    return 0;

}

//Signal Handler, simply sets flag to exit main operating loop
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
            // Cancel and join all threads
        struct thread_data *entry;
        SLIST_FOREACH(entry, &head, entries) {
            pthread_cancel(entry->thread_id);
        }
        SLIST_FOREACH(entry, &head, entries) {
            pthread_join(entry->thread_id, NULL);
        }

        // Clean up linked list
        while (!SLIST_EMPTY(&head)) {
            entry = SLIST_FIRST(&head);
            SLIST_REMOVE_HEAD(&head, entries);
            free(entry);
        }
        // Close server socket
        if (serverfd >= 0) {
            close(serverfd);
        }
        // Delete file
        if (remove(FILE_IO) != 0) {
            syslog(LOG_ERR, "Failed to delete the file %s: %s", FILE_IO, strerror(errno));
        }
        // Destroy the mutex
        pthread_mutex_destroy(&file_mutex); 
        syslog(LOG_INFO, "Sockets terminated");
        syslog(LOG_INFO, "Program acheived a graceful exit!!");
        closelog();
        exit(0);
    }
}

void *handle_connection(void *arg) {
    // save argument then free the data
    int tdata = *(int*)arg;
    free(arg);

    // make array for clients IP
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // this will get the IP addr and port number and save it into client_addr
    getpeername(tdata, (struct sockaddr *)&client_addr, &client_len);
    // Convert IP to a human readable string and save it to client_ip
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    // Log the accepted connection
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // open a file for appending data
    int fp = open(FILE_IO, O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fp){
        syslog(LOG_INFO, "Opened file for writing data");
    } else {
        syslog(LOG_ERR, "Client failed to open file.");
        close (tdata);
        return NULL;
    }

    char buffer[BUFFER_SIZE];

    ssize_t bytes_read;
    pthread_mutex_lock(&file_mutex);
    while ((bytes_read = recv(tdata, buffer, BUFFER_SIZE, 0)) > 0) {
        
        if (write(fp, buffer, bytes_read) != bytes_read) {
            syslog(LOG_ERR, "Failed to write received data to file: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex);
            break;
        } else {
            syslog(LOG_INFO, "Read %ld bytes", bytes_read);
        }
        
        // Check for end of data transfer
        if (buffer[bytes_read - 1] == '\n') {
            break;
        }
    }
    pthread_mutex_unlock(&file_mutex);
    fsync(fp);
    close(fp);
    

    if (bytes_read < 0) {
        syslog(LOG_ERR, "Failed to receive data: %s, recv returned: %zd", strerror(errno), bytes_read);
    }

    fp = open(FILE_IO, O_RDONLY, 0644);
    if (fp < 0) {
        syslog(LOG_ERR, "Failed to open file for reading: %s", strerror(errno));
        close(tdata);
        return NULL;
    }
    pthread_mutex_lock(&file_mutex);
    while ((bytes_read = read(fp, buffer, BUFFER_SIZE)) > 0) {
        if (send(tdata, buffer, bytes_read, 0) < 0) {
            syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
            break;
        }
    }
    pthread_mutex_unlock(&file_mutex);
    close(fp);
    close(tdata);

    
    syslog(LOG_INFO, "Closed connection to %s", client_ip);
    return NULL;

}

void setup_timer() {
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_handler;
    sev.sigev_notify_attributes = NULL;
    // creates a timer using the real time clock, timer_handler
    // will get executed whenever the timer expires
    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1) {
        syslog(LOG_ERR, "Failed to create timer: %s", strerror(errno));
        exit(1);
    }
    // setup periodic timer to go off every 10 sec
    struct itimerspec timer_def;
    timer_def.it_value.tv_sec = 10;
    timer_def.it_value.tv_nsec = 0;
    timer_def.it_interval.tv_sec = 10;
    timer_def.it_interval.tv_nsec = 0;
    if (timer_settime(timer_id, 0, &timer_def, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set timer: %s", strerror(errno));
        exit(1);
    }
}

void timer_handler(union sigval dummyval) {
    (void)dummyval; // Mark dummyval as intentionally unused
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now); // get time in a human readable format
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "timestamp:%Y-%m-%d %H:%M:%S\n", tm_info);

    // lock mutex to access shared file at FILE_IO
    pthread_mutex_lock(&file_mutex);
    int file_fd = open(FILE_IO, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd >= 0) {
        if(write(file_fd, timestamp, strlen(timestamp)) == -1){
            syslog(LOG_ERR, "Error writing timer to file: %s", strerror(errno));
        }
        close(file_fd);
    }
    pthread_mutex_unlock(&file_mutex);
}