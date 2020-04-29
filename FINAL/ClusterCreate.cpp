/*
This is the final code written for ClusterCreate, a framework for task distribution in Android devices
This file contains all the functions that is used in setting up server and client
and also handline server and client fault tolerance
*/
// #include "ClusterCreate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h> //opt
#include <netinet/in.h> //opt
#include <dlfcn.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include <dirent.h>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#define BUFFER_LEN 1024
#define BACKUP_SERVERS 1
#define UDP_PORT 7000

using namespace std;

struct client_info{
    char ipAddr[25];
    int  port;
    int  sock_desc;
    int busy;
    pthread_cond_t cond1;
    pthread_mutex_t lock;
};

struct params_connection_handler{
    int sd; //socket descriptor required for connecting to client. Don't delete this (atleast as of now)
    int key;
    map<int, client_info> *client_table;
    map<int, string> *work_table;
    vector<string> *pending_files;
    vector<string> *completed_files;
    map<int, client_info> *next_servers;
    int replicate;
};

struct params_server_work{
    int port;
    map<int, client_info> *client_table;
    map<int, string> *work_table;
    vector<string> *pending_files;
    vector<string> *completed_files;
    set<unsigned int> *priority_table;
    map<int, client_info> *next_servers;
};

typedef struct client_info client_info;
typedef struct params_connection_handler params_connection_handler;
typedef struct params_server_work params_server_work;
typedef struct sockaddr_in sockaddr_in;

//*************FUNCTION DEFINITIONS******************//

void get_pending_files(vector<string> *pending_files){
    DIR *directory;
    struct dirent *current;
    directory = opendir("../so_files/");
    if(directory){
        while((current = readdir(directory))!=NULL){
            string s(current->d_name);
            if(s.length()>3 && (s.compare(s.length()-3, 3, ".so") == 0) )
                pending_files->push_back(current->d_name);
        }
    }
    else{
        printf("The .so file directory doesn't exist!\n");
    }
}


void recv_file(string pathstr ,client_info& client){
    char* path = (char*)malloc(sizeof(char)*pathstr.length());
    strcpy(path, pathstr.c_str());
	FILE *fp = fopen(path, "w");
	int fsize = -1;
	read(client.sock_desc, &fsize, sizeof(int));
	char read_buf[BUFFER_LEN];
	for (int i = 0; i < fsize / BUFFER_LEN; i++){
	    int read_bytes = read(client.sock_desc, read_buf, BUFFER_LEN);
	    fwrite(read_buf, read_bytes, 1, fp);
	}
	if (fsize % BUFFER_LEN != 0){
		int leftbytes = fsize - ftell(fp);
		int read_bytes = read(client.sock_desc, read_buf, leftbytes);
		fwrite(read_buf, read_bytes, 1, fp);
	}
	fclose(fp);
}


void send_file(string pathstr, client_info& client){
    char* path = (char*)malloc(sizeof(char)*pathstr.length());
    strcpy(path, pathstr.c_str());
    char buffer[BUFFER_LEN];

    FILE *fp = fopen(path, "r");

    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    write(client.sock_desc, &filesize, sizeof(int));

    for (int i = 0; i < filesize / BUFFER_LEN; i++){
        fread(buffer, BUFFER_LEN, 1, fp);
        write(client.sock_desc, buffer, BUFFER_LEN);
    }
    if (filesize%BUFFER_LEN != 0){
        int leftbytes = filesize - ftell(fp);
        fread(buffer, leftbytes, 1, fp);
        write(client.sock_desc, buffer, leftbytes);
    }
    fclose(fp);
}


char* concatenate(char* string1, char* string2){
    //tab begin
    int i,j;
    char* result = (char*)malloc(sizeof(char)*(strlen(string1)+strlen(string2)));
    for(i=0;i<strlen(string1);i++){
        result[i] = string1[i];
    }
    for(j=0;j<strlen(string2);j++){
        result[i+j] = string2[j];
    }
    return result;
}


client_info* CreateClient(char* ipAddr, int port, int sock_desc, int busy){
    //tab begin
    client_info *c = (client_info*)malloc(sizeof(client_info));
    strcpy(c->ipAddr, ipAddr);
    c->port = port;
    c->sock_desc = sock_desc;
    c->busy = busy;
    c->cond1 = PTHREAD_COND_INITIALIZER;
    c->lock = PTHREAD_MUTEX_INITIALIZER;
    //c->rank = rank;
    return c;
}


void print_client_details(map<int, client_info> m){
    //tab begin
    printf("Client Table :\n");
    for(auto x: m){
        printf("Client %d : %s:%d \n", x.first, x.second.ipAddr, x.second.port);
    }
}


void *connection_handler(void *socket_desc){
    //tab begin
   //= (map<int, client_info> *)malloc(sizeof(map<int, client_info>));
    params_connection_handler *p = (params_connection_handler*)socket_desc;
    map<int, client_info> *client_table = p->client_table;
    map<int, string> *work_table = p->work_table;
    vector<string> *pending_files = p->pending_files;
    vector<string> *completed_files = p->completed_files;
    map<int, client_info> *next_servers = p->next_servers;
    int sock = p->sd;
    int key = p->key;
    int replicate = p->replicate;
    int read_size;
    char *message , client_message[2000];

   //this is the part where yo replicate the next_servers table and also the so files(if this client has replicate var set to true)
    if(replicate){
        write(sock, "file", 5);
        vector<string> files;
        get_pending_files(&files);
        int file_nos = files.size();
        write(sock, &file_nos, sizeof(int));
        for(auto x: files){
            send_file((string("../so_files/")+x), (*client_table)[key]);
        }
    }
    //send next server list
    write(sock,"nstb",5);
    int ns_size = next_servers->size();
    write(sock, &ns_size, sizeof(int));
    for(auto x: *next_servers){
        int strlength = strlen(x.second.ipAddr);
        // cout << x.second.ipAddr << endl;
        write(sock, &x.first, sizeof(int));
        write(sock, &x.second.port, sizeof(int));
        write(sock, &strlength, sizeof(int));
        write(sock, &x.second.ipAddr, strlength);
    }

    while(1){
        if(work_table->find(key)==work_table->end()){
            printf("No work assigned to client %d as of yet, going to sleep!\n", key);
            pthread_cond_wait(&((*client_table)[key].cond1), &((*client_table)[key].lock));
        }
        //this part is continued after the client recieves a signal from another thread when a  work is assigned.
        pthread_mutex_unlock(&((*client_table)[key].lock));
        printf("Client %d has been woken up!\n", key);
        //sleep(1);
        write(sock , "ping" , 5);
        read_size = recv(sock , client_message , 2000 , 0);
        if(read_size==0 || read_size== -1){
            printf("Client Disconnected\n");
            pending_files->push_back((*work_table)[key]);
            work_table->erase(key);
            client_table->erase(key);
            return NULL;
        }
        write(sock , "file" , 5);
        send_file((string("../so_files/")+(*work_table)[key]), (*client_table)[key] );
        recv_file( (string("../op_files/")+"op"+ (*work_table)[key].substr(0, (*work_table)[key].length()-3)+".txt"), (*client_table)[key]);
        read_size = recv(sock , client_message , 2000 , 0);
        if((strcmp(client_message,"complete")!=0 || read_size==0 || read_size==-1)){ //the files hasn't been sent properly back to server
            printf("Output file not collected properly\n");
            pending_files->push_back((*work_table)[key]);
            work_table->erase(key);
            if(read_size==0 || read_size==-1){
                client_table->erase(key);
                return NULL;
            }
        }
        else{
            printf("Output file collected succesfully from Client %d!\n", key);
            completed_files->push_back((*work_table)[key]);
            (*client_table)[key].busy = 0;
            work_table->erase(key);
        }
    }
 }


void udp_update_broadcast(int flag, map<int, client_info> *client_table, vector<sockaddr_in> *next_servers){
    if(flag==1){
        int serverfd = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in server_addr, client_addr;
        if(serverfd<0){
            printf("Server for UDP broadcast couldn't be created.\n");
            return ;
        }
        memset(&server_addr,0,sizeof(server_addr));
        memset(&client_addr,0,sizeof(client_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(UDP_PORT);
        bind(serverfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        for(auto x: *(client_table)){
            client_addr.sin_family = AF_INET;
            client_addr.sin_port = htons(x.second.port);
            inet_aton(x.second.ipAddr, &client_addr.sin_addr);
            sendto(serverfd, (void*)next_servers, sizeof(next_servers), MSG_CONFIRM , (const struct sockaddr *)&client_addr, sizeof(client_addr));
        }
        close(serverfd);
   }
 }


void* start_server(void *params){
    //tab begin
    params_server_work *p = (params_server_work*)params;
    map<int, client_info> *client_table = p->client_table;
    map<int, string> *work_table = p->work_table;
    vector<string> *pending_files = p->pending_files;
    vector<string> *completed_files = p->completed_files;
    set<unsigned int> *priority_table = p->priority_table;
    map<int, client_info> *next_servers = p->next_servers;
    int port = p->port;
    int socket_desc , new_socket , c , *new_sock,i;
 	struct sockaddr_in server , client;
 	char *message;

 	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
 	if(socket_desc == -1){
 	      printf("Sorry. The Socket could not be created!\n");
 	}

 	server.sin_family = AF_INET;
 	server.sin_addr.s_addr = INADDR_ANY;
 	server.sin_port = htons(port);

    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
   		printf("Socket Binding Failed!\n");
 	}
    printf("Sever Socket has been binded to the port : %d\n",port);

 	listen(socket_desc , 3);

 	puts("Waiting for incoming connections...\n");
 	c = sizeof(struct sockaddr_in);
    i = 0;

 	while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){

 	    printf("Connection Accepted from: %s:%d (Client %d)\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), i);
        unsigned int spec;  //= (int*)malloc(sizeof(int));
        read(new_socket, &spec, sizeof(int));
        spec = spec<<7;
        spec = spec | i;
        pthread_t sniffer_thread;
        params_connection_handler *pc = (params_connection_handler*)malloc(sizeof(params_connection_handler));
        pc->key = i;
        pc->sd = new_socket;
        pc->client_table = client_table;
        pc->work_table = work_table;
        pc->pending_files  = pending_files;
        pc->completed_files = completed_files;
        pc->next_servers = next_servers;
        pc->replicate = 0;
        client_info *c = CreateClient(inet_ntoa(client.sin_addr), ntohs(client.sin_port), new_socket, 0);
        client_table->insert(pair<int, client_info> (i, *c));
        priority_table->insert(spec);
        print_client_details(*client_table);

        if(next_servers->size() < BACKUP_SERVERS){
            int size = next_servers->size(); //basically acts like the order of server take over!
            next_servers->insert(pair<int, client_info> (size,*c));
            pc->replicate = 1;
        }

 		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) pc) < 0){
 		         perror("could not create thread");
 		}
        i++;
    }
 	if (new_socket < 0){
 	      perror("accept failed");
 	}
}


void* distribute_work(void* params){
    //tab begin
    params_server_work *p = (params_server_work*)params;
    map<int, client_info> *client_table = p->client_table;
    map<int, string> *work_table = p->work_table;
    vector<string> *pending_files = p->pending_files;
    vector<string> *completed_files = p->completed_files;
    set<unsigned int> *priority_table = p->priority_table;
    set<unsigned int>::reverse_iterator rit;
    int file_count = pending_files->size();
    start:
    while( completed_files->size() < file_count ){ // till all the results of the given so files aren't accumilated
        for(auto x: *pending_files){ // iterate through the pending files list
            for(rit = priority_table->rbegin(); rit!=priority_table->rend(); rit++){ //get available client for the pending file by iterating through the client table and checking if a given client is busy
                int key = (*rit)&127; //last seven digits in binary rep is 1
                if(!((*client_table)[key].busy) && x.length() > 0){ //if client is free assign pending file to it
                    work_table->insert(make_pair(key, x));
                    printf("Work has been assigned to client %d : %s\n", key, x.c_str());
                    (*client_table)[key].busy = 1; //mark client as busy
                    remove(pending_files->begin(), pending_files->end(), x);
                    pthread_cond_signal(&((*client_table)[key].cond1));
                    goto start; // you are breaking because you hvae found a client to assign the work to and don't want to souble assign to other clients on the clirnt_table!
                }
            }
        }
    }
    printf("All the give tasks in the so_files folder has been executed successfully!\n");
}

void runserver(int port){
    setbuf(stdout, NULL);
    map<int, client_info> CLIENT_TABLE;
    vector<string> PENDING_FILES;
    vector<string> COMPLETED_FILES;
    map<int, string> WORK_TABLE;
    set<unsigned int> PRIORITY_TABLE;
    map<int, client_info> NEXT_SERVERS;

    params_server_work *params = (params_server_work *)malloc(sizeof(params_server_work));
    params->port = port;
    params->client_table = &CLIENT_TABLE;
    params->work_table = &WORK_TABLE;
    params->pending_files = &PENDING_FILES;
    params->completed_files = &COMPLETED_FILES;
    params->priority_table = &PRIORITY_TABLE;
    params->next_servers = &NEXT_SERVERS;
    get_pending_files(&PENDING_FILES);

    pthread_t server_thread;
    pthread_t distribute_work_thread;

    pthread_create(&server_thread, NULL, start_server, (void*)params);

    sleep(500);

    pthread_create(&distribute_work_thread, NULL, distribute_work, (void*)params);

    pthread_join(distribute_work_thread, NULL);
    pthread_join(server_thread, NULL);

}


int recv_file_cs(int serv_sockfd, const char *path){
	FILE *fp = fopen(path, "w");
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

int send_file_cs(const char *path, int client_fd){
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
	}
	if (filesize % BUFFER_LEN != 0){
		int leftbytes = filesize - ftell(fp);
		fread(buffer, leftbytes, 1, fp);
		res = write(client_fd, buffer, leftbytes);
        if(res==0||res==-1)
            return -1;
    }
    fclose(fp);
    return 1;
}

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


int exec_file(const char *path, const char *output_path){
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
            recv_file_cs(clientSocket, ("./so_files/"+to_string(i)+".so").c_str() );
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
    return 1;
}

void runclient(char* addr, int port){
    //tab begin
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
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(addr);

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
        }
		if(strcmp(buffer, "ping") == 0){
			int res = write(clientSocket, "pong", 5); //TODO: FAILURE_POINT
            if(res==0||res==-1){
                printf("Connection to server lost.\n");
                exit(1);
            }
        }
		else if (strcmp(buffer, "file") == 0){
            int res;
            printf("Receiving file\n");
			res = recv_file_cs(clientSocket, "client_node_recvfile.so"); //TODO: FAILURE_POINT
            if(res==0||res==-1){
                printf("Connection to server lost.\n");
                exit(1);
            }
            printf("Received file\n");
			exec_file("./client_node_recvfile.so", "client_recvfile_op.txt");
			printf("Executed code\n");
			printf("Sending Output Back to the server\n");
			res = send_file_cs("./client_recvfile_op.txt",clientSocket); //TODO: FAILURE_POINT
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
}
