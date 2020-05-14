This is the code to ClusterCreate written by Siddarth Karki, Anagha M Anil Kumar and Karan Panjabi.

This readme will help you in the set up of the tool to run un Linux machines, the code to get it running on Android is situated in the other folder written by Karan, which has a seperate Readme.

Please follow the follwing instructions for the setup:

1. Go to ./src/
2. There will be 4 files ClusterCreate.cpp, ClusterCreate.h, server.cpp, client.cpp
3. You can compile both the server and client code by
    g++ ClusterCreate.cpp server.cpp -o server -lpthread -ldl
    g++ ClusterCreate.cpp client.cpp -o client -lpthread -ldl
4. Create one more folder else where in your directory tree, not in the src folder or it's parent folder
5. Copy ClusterCreate.cpp client.cpp and the client object file generated after compiling and place it in this newly created server.
6. Client and Server can be run in their respective loactions terminals by ./client and ./server respectively.
7. The similar instructions need to be followed regardless of client and server code being in the same system or on seperate physical systems.
