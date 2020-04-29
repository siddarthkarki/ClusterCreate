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
		next_servers.insert(pair<int, client_info> (order, c));
	}
	printf("Backups Server table updated.\n");
	printf("Backup server details are as follows....\n");
	for(auto x : next_servers){
		printf("Backup server %d : %s:%d\n", x.first, x.second.ipAddr, x.second.port);
	}
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
