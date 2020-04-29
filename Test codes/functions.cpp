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
