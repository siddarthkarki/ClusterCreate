#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <dlfcn.h>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

#define PORT 8080
#define BUFFER_LEN 1024

using namespace std;

int return_specs(){
	const char *command = "lscpu | grep MHz > output.txt";
	system(command);
	fstream file;
	string word;
  char *spec = (char*)malloc(sizeof(char)*10);
  file.open("output.txt");
  int c = 0;
  while (file >> word){
		c++;
    if(c==3){
        	// displaying content
        	strcpy(spec,word.c_str());
		}
	}
	cout << spec << endl;
	return atoi(spec);
}
void recv_file(int serv_sockfd, const char *path){
	FILE *fp = fopen(path, "w");
	// read the filesize first
	int fsize = -1;
	read(serv_sockfd, &fsize, sizeof(int));

	char read_buf[BUFFER_LEN];
	for (int i = 0; i < fsize / BUFFER_LEN; i++){
		int read_bytes = read(serv_sockfd, read_buf, BUFFER_LEN); // TODO: assert read_bytes as BUFFER_LEN
		fwrite(read_buf, read_bytes, 1, fp);
	}
	if (fsize % BUFFER_LEN != 0){
		int leftbytes = fsize - ftell(fp);
		int read_bytes = read(serv_sockfd, read_buf, leftbytes); // TODO: assert read_bytes as leftbytes
		fwrite(read_buf, read_bytes, 1, fp);
	}

	fclose(fp);
}

void send_file(const char *path, int client_fd){
	char buffer[BUFFER_LEN];
	FILE *fp = fopen(path, "r");
	fseek(fp, 0, SEEK_END);
	int filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	write(client_fd, &filesize, sizeof(int));
	for (int i = 0; i < filesize / BUFFER_LEN; i++){
		fread(buffer, BUFFER_LEN, 1, fp);
		write(client_fd, buffer, BUFFER_LEN);
		// printf("%d\n", BUFFER_LEN);
	}
	if (filesize % BUFFER_LEN != 0){
		int leftbytes = filesize - ftell(fp);
		fread(buffer, leftbytes, 1, fp);
		write(client_fd, buffer, leftbytes);
		// printf("%d\n", leftbytes);
	}
}

// path needs to have a / to be absolute or relative\
// TODO: add cmd line args
int exec_file(const char *path, const char *output_path){
	// TODO: decide to work on separate thread or exec as separate process
	char *errstring;
	// for now it'll exec on the same thread
	void *client_code = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	errstring = dlerror();
	if(errstring != NULL)
		printf("Err? : %s\n", errstring);
	int (*client_mainfunc)() = (int (*)(void)) dlsym(client_code, "main");
	// create a tmp file to store output, redirect stdout to file
	int op_fd = open(output_path, O_CREAT | O_TRUNC | O_WRONLY, 777);
	int stdout_fd = dup(1);
	int stderr_fd = dup(2);
	dup2(op_fd, 1);
	dup2(op_fd, 2);
    int retval = client_mainfunc();
    dlclose(client_code);
	close(op_fd);
	dup2(stdout_fd, 1);
	close(stdout_fd);
	dup2(stderr_fd, 2);
	close(stderr_fd);
	return retval;
}

int main(){
	setbuf(stdout, NULL);
	int clientSocket, ret;
	struct sockaddr_in serverAddr;
	struct sockaddr_in clientAddr;
	char buffer[BUFFER_LEN];

	printf("This is the ClusterCreate Client programme.\n");
	printf("Setting up connection with the server...\n");
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket < 0){
		printf("[-]Error in connection.\n");
		exit(1);
	}
	printf("[+]Client Socket is created.\n");
	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret = connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	if (ret < 0){
		printf("[-]Error in connection.\n");
		exit(1);
	}
	printf("[+]Connected to Server.\n");

	int spec;
	spec = return_specs();
	write(clientSocket, &spec, sizeof(int));
	// cout << "specs: " << spec << endl;// send it to the server here!
	//new fault tolerance to be implmented where
	//TODO : create a map<int, client_info> data structure with all the unecessary fields intialiazed with -1 (check if this will affect the code in anyways)

	/*TODO : check if the recieved string is "file"
					 if yes, then get ready to recieve files by accepting the number of files!
					 			create a SO_FILES directory
								recieve the files in the for loop for file nos of time in the SO_FILES directory
					recieve number of clients in next_server Table
					run a loop
						accept the client priority
						accept the PORT
						accept the IpADDR (25 characters)
	*/

	while (1){
		printf("Waiting for tasks from server....\n");
		read(clientSocket, buffer, 5); // command size
		if (strcmp(buffer, "ping") == 0){
			write(clientSocket, "pong", 5); // this will also write back 5 bytes, read 5 bytes to consume this
		}
		else if (strcmp(buffer, "file") == 0){
			printf("Receiving file\n");
			recv_file(clientSocket, "client_node_recvfile.so");
			printf("Received file\n");
			exec_file("./client_node_recvfile.so", "client_recvfile_op.txt");
			printf("Executed code\n");
			printf("Sending Output Back to the server\n");
			send_file("./client_recvfile_op.txt",clientSocket);
			write(clientSocket, "complete", 9);
		}
		else if (strcmp(buffer, "exit") == 0){
			close(clientSocket);
			break;
		}
	}
	return 0;
}
