#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
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
#include <map>
#include <assert.h>

#define PORT 8080
#define BUFFER_LEN 1024

using namespace std;

typedef struct client_info{
  char ipAddr[25];
  int  port;
}client_info;

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

int recv_file(int serv_sockfd, const char *path){
	FILE *fp = fopen(path, "w");
	// read the filesize first
	int fsize = -1;
  int res = read(serv_sockfd, &fsize, sizeof(int));
	if(res == 0 || res ==-1){
    return -1;
  }

	char read_buf[BUFFER_LEN];
	for (int i = 0; i < fsize / BUFFER_LEN; i++){
		int read_bytes = read(serv_sockfd, read_buf, BUFFER_LEN); // TODO: assert read_bytes as BUFFER_LEN
    if(read_bytes == 0 || read_bytes == -1){
      return -1;
    }
		fwrite(read_buf, read_bytes, 1, fp);
	}
	if (fsize % BUFFER_LEN != 0){
		int leftbytes = fsize - ftell(fp);
		int read_bytes = read(serv_sockfd, read_buf, leftbytes); // TODO: assert read_bytes as leftbytes
    if(read_bytes == 0 || read_bytes == -1){
      return -1;
    }
    fwrite(read_buf, read_bytes, 1, fp);
	}
	fclose(fp);
  return 1;
}

int send_file(const char *path, int client_fd){
	char buffer[BUFFER_LEN];
	FILE *fp = fopen(path, "r");
	fseek(fp, 0, SEEK_END);
	int filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	int res = write(client_fd, &filesize, sizeof(int));
  if(res==0||res==-1)
    return -1;
  for (int i = 0; i < filesize / BUFFER_LEN; i++){
		fread(buffer, BUFFER_LEN, 1, fp);
		res = write(client_fd, buffer, BUFFER_LEN);
    if(res==0||res==-1)
      return -1;
    // printf("%d\n", BUFFER_LEN);
	}
	if (filesize % BUFFER_LEN != 0){
		int leftbytes = filesize - ftell(fp);
		fread(buffer, leftbytes, 1, fp);
		res = write(client_fd, buffer, leftbytes);
    if(res==0||res==-1)
      return -1;
    // printf("%d\n", leftbytes);
	}
  fclose(fp);
  return 1;
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

int replicate_state(int clientSocket, map<int, client_info>* next_servers){
    //tab begin
    int file_nos = 0;
    char temp[25];
    read(clientSocket, temp, 5);
    if(strcmp(temp,"file") == 0){
        printf("This client has been elected as a backup server!\n");
        if(mkdir("so_files",0777)==-1){
            printf("The so_files directory could not be created!.\n");
        }
        read(clientSocket, &file_nos, sizeof(int));
        for(int i=0;i<file_nos;i++){
            recv_file(clientSocket, ("./so_files/"+to_string(i)+".so").c_str() );
        }
        printf("State replication completed succesfully!.\n");
        read(clientSocket, temp, 5); //this reads for the nstb command
    }

    assert(strcmp(temp, "nstb")==0);
    printf("Accepting backupserver details from current server.\n");
    int client_nos = 0;
    read(clientSocket, &client_nos, sizeof(int));
    client_info c;
    int order;
    for(int i = 0; i<client_nos; i++){
        client_info c;
        int order;
        int strlength;
        read(clientSocket, &order, sizeof(int));
        read(clientSocket, &(c.port), sizeof(int));
        read(clientSocket, &strlength, sizeof(int));
        read(clientSocket, c.ipAddr, strlength+1);
        c.ipAddr[strlength] = '\0';
        next_servers->insert(pair<int, client_info> (order, c));
    }
    printf("Backups Server table updated.\n");
    printf("Backup server details are as follows....\n");
    for(auto x : *next_servers){
        printf("Backup server %d : %s:%d\n", x.first, x.second.ipAddr, x.second.port);
    }
}

int main(){
	setbuf(stdout, NULL);
	int clientSocket, ret;
	struct sockaddr_in serverAddr;
	struct sockaddr_in clientAddr;
	char buffer[BUFFER_LEN];
	map<int, client_info> next_servers;

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
    replicate_state(clientSocket, &next_servers);
	while (1){
		printf("Waiting for tasks from server....\n");
		int t = read(clientSocket, buffer, 5); // command size
    if(t==-1 || t==0){ //it actually return 0
      printf("Connection to server lost!.\n");
      exit(1);
      // system("./server_final_1");
    }
		if (strcmp(buffer, "ping") == 0){
			int res = write(clientSocket, "pong", 5); //TODO: FAILURE_POINT
      // this will also write back 5 bytes, read 5 bytes to consume this
      if(res==0||res==-1){
        printf("Connection to server lost.\n");
        exit(1);
      }
    }
		else if (strcmp(buffer, "file") == 0){
      int res;
      printf("Receiving file\n");
			res = recv_file(clientSocket, "client_node_recvfile.so"); //TODO: FAILURE_POINT
      if(res==0||res==-1){
        printf("Connection to server lost.\n");
        exit(1);
      }
      printf("Received file\n");
			exec_file("./client_node_recvfile.so", "client_recvfile_op.txt");
			printf("Executed code\n");
			printf("Sending Output Back to the server\n");
			res = send_file("./client_recvfile_op.txt",clientSocket); //TODO: FAILURE_POINT
      if(res==0||res==-1){
        printf("Connection to server lost.\n");
        exit(1);
      }
      res = write(clientSocket, "complete", 9); //TODO: FAILURE_POINT
      if(res==0||res==-1){
        printf("Connection to server lost.\n");
        exit(1);
      }
    }
		else if (strcmp(buffer, "exit") == 0){
			close(clientSocket);
			break;
		}
	}
	return 0;
}
