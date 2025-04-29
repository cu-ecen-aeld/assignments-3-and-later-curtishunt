#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>  
#include <stdlib.h>
#include <unistd.h>   
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>

volatile sig_atomic_t exit_flag = 0;

int my_socket = -1;
int count_exits = 0;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        exit_flag = 1;
		remove("/var/tmp/aesdsocketdata");
        printf("Caught exit flag!!!!!");
		count_exits += 1;
		syslog(LOG_INFO, "exited %u times", count_exits);
    }
}

int main(int argc, char *argv[]) {

	remove("/var/tmp/aesdsocketdata");

	// check for daemon
	int daemon_mode = 0;
	if (argc == 2 && strcmp(argv[1], "-d") == 0) {
		daemon_mode = 1;
	}

	//////////////////////
	//// create socket ///
	//////////////////////
	my_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket == -1) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	printf("Socket created!!!!!!!!!!!!!!\n");
	// bind socket to port 9000
	struct sockaddr_in server_socket;
	memset(&server_socket, 0, sizeof(server_socket));
	server_socket.sin_family = AF_INET;
	server_socket.sin_addr.s_addr = htonl(INADDR_ANY);
	server_socket.sin_port = htons(9000);
	
	int opt = 1;
	if (setsockopt(my_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt failed");
		close(my_socket);
		exit(EXIT_FAILURE);
	}
	
	printf("attempting to bind to port 9000\n");
	if (bind(my_socket, (struct sockaddr *)&server_socket, sizeof(server_socket)) == -1) {
		perror("failed to bind to port 9000");
		close(my_socket);
		exit(EXIT_FAILURE);
	}

	printf("Socket bound to port 9000!!!!!!!!!!!!!!\n");

	if (daemon_mode) {
		pid_t pid = fork();
		if (pid < 0) {
			perror("fork failed");
			close(my_socket);
			exit(EXIT_FAILURE);
		}
	
		if (pid > 0) {
			// Parent exits
			exit(EXIT_SUCCESS);
		}
	
		// Child continues: become session leader
		if (setsid() < 0) {
			perror("setsid failed");
			close(my_socket);
			exit(EXIT_FAILURE);
		}
	
		// Optional: fork again to fully detach (not required for your assignment)
	
		// Change working directory to root
		if (chdir("/") < 0) {
			perror("chdir failed");
			close(my_socket);
			exit(EXIT_FAILURE);
		}
	
		// Close stdin, stdout, stderr
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	//signals that will trigger an exit
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	// start listening on port
	if (listen(my_socket, 10) == -1) {
    		perror("Failed to start listening on port 9000");
    		exit(EXIT_FAILURE);
	}
	
	printf("started listening on port 9000!!!!!\n");
	
	// accept client
	struct sockaddr_in client;
	int client_socket;
	socklen_t client_len = sizeof(client);
	char ip_address[16];
	ssize_t bytes_sent;

	// storage variable
	char *data = malloc(1); 
    data[0] = '\0'; 
    size_t data_len = 0;
	size_t add_len;


	// accepts client sockets until an exit signal is received
	while(exit_flag == 0){

		client_socket = accept(my_socket, (struct sockaddr *)&client, &client_len);
		if (client_socket == -1) {
			if (exit_flag) {
				remove("/var/tmp/aesdsocketdata");
				free(data);
				break;
			}
		    perror("failed to accept a client");
			remove("/var/tmp/aesdsocketdata"); 
			free(data);
		    close(my_socket);
		    exit(EXIT_FAILURE);
		}
		if (inet_ntop(AF_INET, &client.sin_addr, ip_address, sizeof(ip_address)) == NULL) {
    			perror("inet_ntop failed");
		} else {
    			syslog(LOG_INFO, "Accepted connection from %s", ip_address);
		}
		printf("accepted a client!!! connection is %s\n", ip_address);
		// log connection with syslog
		openlog("aesd_socket", LOG_PID, LOG_USER);
		syslog(LOG_INFO, "Accepted connection from %s\n", ip_address);	
		
		ssize_t bytes_received;
		char input_buff[1024];

		// open file
		FILE *output_file = fopen("/var/tmp/aesdsocketdata", "a+");
		if (output_file == NULL) {
			remove("/var/tmp/aesdsocketdata"); 
			free(data);
		    perror("failed to open file");
		    close(client_socket);
		    close(my_socket);
		    exit(EXIT_FAILURE);
		}
		// Receive data
		// runs until no more data from socket
		while(1) {
		    memset(input_buff, 0, 1024);
		    bytes_received = recv(client_socket, input_buff, 1024, 0);
			if(exit_flag){
				
				break;
			}
			printf("received %s from socket, number of bytes received was: %zd\n", input_buff, bytes_received);
		    if (bytes_received == -1) {
				remove("/var/tmp/aesdsocketdata"); 
				free(data);
				perror("failed receving data from client");
				close(client_socket);
				close(my_socket);
				fclose(output_file);
				exit(EXIT_FAILURE);
		    } else if (bytes_received == 0) {
				break;
		    }
		    // Check for /n character
		    else if (strchr(input_buff, '\n') != NULL) {
				// save last byte of transfer
				add_len = strlen(input_buff);
				char *temp = realloc(data, data_len + add_len + 1);
				if (!temp) {
					remove("/var/tmp/aesdsocketdata"); 
					free(data);
					perror("realloc");
					fclose(output_file);
					exit(EXIT_FAILURE);
				}
				data = temp;
				strcat(data, input_buff);
				data_len += add_len;
				
				//write data to file
				fwrite(input_buff, sizeof(char), bytes_received, output_file);

				break;
		    }
			else {
				// save data
				add_len = strlen(input_buff);
				char *temp = realloc(data, data_len + add_len + 1);
				if (!temp) {
					remove("/var/tmp/aesdsocketdata"); 
					free(data);
					perror("realloc");
					fclose(output_file);
					return 1;
				}
				data = temp;
				strcat(data, input_buff);
				data_len += add_len;
				fwrite(input_buff, sizeof(char), bytes_received, output_file);
			}
		}

		// close file
		fclose(output_file);
		
		// send data back to client
		bytes_sent = send(client_socket, data, strlen(data), 0);
		printf("sent %zd bytes to client socket\n", bytes_sent); 
		if (bytes_sent == -1) {
			remove("/var/tmp/aesdsocketdata"); 
			free(data);	
			perror("failed to send");
			fclose(output_file);
			close(client_socket);
			close(my_socket);
			exit(EXIT_FAILURE);
		}
		printf("sent data to a socket!!!!!!!!\n");
		//close client socket
		if(close(client_socket)==-1){
			remove("/var/tmp/aesdsocketdata"); 
			free(data);
			perror("failed to close client socket");
			close(my_socket);
			exit(EXIT_FAILURE);	
		}
		printf("Closed client socket!!!!!!!!!\n");
		// log disconnection with syslog
		syslog(LOG_INFO, "Closing connection with client: %s", ip_address);

	}
	
	//delete file
	remove("/var/tmp/aesdsocketdata"); 
	free(data);
	// Close the server socket
	close(my_socket);
	printf("Closed Server Socket!!!!!!!!!!\n");
	// close syslog
	closelog();
	
	return 0;
}
