#include <stdio.h>
#include <netinet/in.h> // for sockaddr_in struct
#include <sys/socket.h>
#include <unistd.h> // for close
#include <dirent.h> // for opendir, closedir
#include <errno.h>
#include <string.h> // for memset
#include <arpa/inet.h> // for inet_addr
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h> //for open file modes.

//==============================================================================
//================ Some general info that we can delete later ==================

// The peer acts both as a client and as a server

// Initially, a peer can be configured to have a list of known partners

// The user asks the (locally installed) peer to get a file -> after file is
// successfully transferred, it is stored in a directory chosen by the user 

// The local peer contacts the other peers concurrently 


// Protocol errors (file does not exist, wrong command) should be handled 
// appropriately

// We should do health checks every x seconds

//==============================================================================


// Change this value later, maybe relate it somehow to the number of threads
// for a program
#define MAX_PEERS 10
#define PORT 5005
#define BUF_LEN 1024

// Global variable for peer-to-peer conection
int connection_vector[5];


//----- set_addr is used to initialize a sockaddr_in struct
//
//----- Parameters
//----- 	addr: sockaddr_in struct to be initialized
//-----		inaddr: IP address
void set_addr(struct sockaddr_in *addr, uint32_t inaddr, short sin_port) {
	memset((void*) addr, 0, sizeof(*addr));

	// Set the family to be the IPv4 internet protocols
	addr->sin_family = AF_INET;

	addr->sin_addr.s_addr = htonl(inaddr);
	addr->sin_port = htons(sin_port);
}


//----- when_acting_as_a_server is used to facilitate other peers with a list of
//----- available files and send them to a peer as requested
void when_acting_as_a_server() {
	int sockfd, connfd,size,file_desc;
	int n_read;
	struct sockaddr_in local_addr, remote_addr;
	socklen_t remote_len;
	char file_name[BUF_LEN+5];
	char buf[BUF_LEN+5]; // delete this later maybe

	printf("Server is starting...\n"); // delete this later
	
	// TCP/IP protocol
	// For TCP: SOCK_STREAM socket
	sockfd = socket(PF_INET, SOCK_STREAM, 0);

	set_addr(&local_addr, INADDR_ANY, PORT);

	if( -1 == bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr))) {
		fprintf(stderr, "Something went wrong when binding.\n");
		return;
	}

	printf("[server] accepting connection\n");

	// Peer accepting connections
	if(-1 == listen(sockfd, 5)) {
		fprintf(stderr, "Something went wrong when listening for connections");
	}


	printf("[server] accepting specific connection\n");

	// Accept connection
	connfd = accept(sockfd, (struct sockaddr *)&remote_addr, &remote_len);
	
	if(-1 == connfd) {
		fprintf(stderr, "Something went wrong when accepting a connection.\n");
		return;
	}

	printf("[server] reading\n");

	// read()
	n_read = read(connfd,&size,4); //reading the file name size
	if ( n_read < 0 || size > 512 ){
		fprintf(stderr, "[Server] Something went wrong when reading.\n");
		return;
	}
	n_read = read(connfd, file_name, size); //reading name of wanted file

	if(-1 == n_read) {
		fprintf(stderr, "[Server] Something went wrong when reading.\n");
		return;
	}

	printf("[server] The peer told me to: %s %d\n", file_name,size);

	file_desc = open(file_name,O_RDONLY) ;
    if ( file_desc == -1 )
    {
		fprintf(stderr, "[Server] Something went wrong when opening file.\n");
		return;
    }

	//calculating the dimension of the file
	size = 0 ;
	while ( (n_read = read(file_desc,buf,BUF_LEN)) > 0 ) 
	{
		size = size+n_read;
	}

	if ( close(file_desc) < 0 )
	{
		fprintf(stderr, "[Server] Something went wrong when closing file.\n");
		return;
	}

	printf("[Server] size calculated %d \n",size) ;
	if ( write(connfd,&size,4) < 0 ) //writing size
	{
		fprintf(stderr, "[Server] Something went wrong when sending the file size.\n");
		return;
	}

	file_desc = open(file_name,O_RDONLY) ;
    if ( file_desc == -1 )
    {
		fprintf(stderr, "[Server] Something went wrong when opening file.\n");
		return;
    }
	//writing the file
	while ( (n_read = read(file_desc,buf,BUF_LEN)) > 0 )
	{
		if ( write(connfd,buf,n_read) < 0 )
		{
			fprintf(stderr, "[Server] Something went sending the file.\n");
			return;
		}
	}

	if ( close(file_desc) < 0 )
	{
		fprintf(stderr, "[Server] Something went wrong when closing file.\n");
		return;
	}

	// write()

	// read()

	if (-1 == close(sockfd)) {
		fprintf(stderr, "The server socket could not close properly");
		return;
	}
}

//----- when_acting_as_a_client is used for the functionality of the local peer
//----- to transfer requested files from other peers to the user's computer
void when_acting_as_a_client(char *file_name,uint32_t peer_address) {
	int sockfd,size,file_desc,n_read;
	struct sockaddr_in remote_addr; 
	peer_address = 2130706433; // this should be changed to an array of peer addresses
	char buf[BUF_LEN+5] ; // delete this later maybe

	printf("Client is starting...\n"); // delete this later

	// TCP/IP protocol
	// For TCP: SOCK_STREAM socket
	sockfd = socket(PF_INET, SOCK_STREAM, 0);

	// for now, connect just to one peer for testing purposes
	// later, connect to more peers 

	// create a new thread (or process) for every peer that is going to be contacted

	// Set the remote address for one of the peers
	set_addr(&remote_addr, peer_address, PORT);

	printf("[client] connecting to server\n");

	// Connect to one of the peers
	if (-1 == connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr))) {
		fprintf(stderr, "Could not connect\n");
		return;
	} 

	printf("[client] writing to server\n");

	// write()
	size = strlen(file_name);
	if(0 > write(sockfd, &size, 4)) {
		fprintf(stderr, "[Client] Something went wrong when writing.\n");
		return;
	}

	if(0 > write(sockfd, file_name, size)) {
		fprintf(stderr, "[Client] Something went wrong when writing.\n");
		return;
	}

	// read()
	
	if ( read(sockfd,&size,4) < 0 ) //reading the file dimension
	{
		fprintf(stderr, "[Client] Something went wrong when reading file size.\n");
		return;
	}

	//creating the file
	printf("[Client] Printint the size after reading %d\n",size) ;
	file_desc = open("Pulamea.txt",O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ) ;
    if ( file_desc < 0 )
    {
		fprintf(stderr, "[Client] Something went wrong when creating the new file.\n");
        return ;
    }

	for ( ; size > 0 ; size -= 1024 )
	{
		if ( (n_read = read(sockfd,buf,BUF_LEN)) < 0 )
		{
			fprintf(stderr, "[Client] Something went wrong when reading the file.\n");
			return;
		}
		if ( write(file_desc,buf,n_read) < 0 )
		{
			fprintf(stderr, "[Client] Something went wrong when writing in the new file.\n");
			return;
		}
	}

	if ( close(file_desc) < 0 )
    {
		fprintf(stderr, "[Client] Something went wrong when closing the file.\n");
        return ;
    }

	if (-1 == close(sockfd)) {
		fprintf(stderr, "The client socket could not close properly");
		return;
	}
}


//----- For now I just created two processes to see that the server and client
//----- work properly. Later this function should just start the peer 
void start_peer(char *path, char *file_name) {
	pid_t pid;

	printf("%s\n", "Peer is starting...");

	if((pid = fork()) < 0) {
		fprintf(stderr, "Error when creating a new process");
		return;
	}
	
	// if parent process
	if(pid == 0) {
		when_acting_as_a_server();
	} else {
		sleep(3);
		when_acting_as_a_client(file_name,0);
	}
}

//----- is_valid_path checks if a given path exists in the system
//
//----- Returns 1 if the path exists and 0 otherwise
int is_valid_path(char *path) {
	DIR* dir;

	if(NULL == (dir = opendir(path))) {
		return 0;
	} else {
		// close the opened dir
		if(-1 == closedir(dir)) {
			fprintf(stderr, "Could not close the directory %s\n", path);
		}

		return 1;
	}
}

//----- Try to connect with a peer
//
//----- Successful connection changes the coressponding vector element with the IP
void *establish_connection_with_peer(void *vargs){
    static int index= 0;
	int sockfd;
	struct sockaddr_in remote_addr; 
	uint32_t peer_address = *(int*) vargs;

	// TCP/IP protocol
	// For TCP: SOCK_STREAM socket
	sockfd = socket(PF_INET, SOCK_STREAM, 0);

	// Set the remote address for the given peer
	set_addr(&remote_addr, peer_address, PORT);

	// Connect to given peer
	if ( connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1) {
		fprintf(stderr, "Could not connect\n");
		return vargs;
	}

	// If connection was successful change the value in the connection vector
	connection_vector[index++]= peer_address;

	// Close the socket
	if (-1 == close(sockfd)) {
		fprintf(stderr, "The client socket could not close properly");
		exit(-1);
	}

	// Exit the thread
	pthread_exit(&peer_address);
}

//----- Create threads for each peer to establish a connection
//
//----- Return a peer address with whom the connection was successful
//----- or a -1 otherwhise
int create_threads(int peers[]){
	pthread_t thread_id[5];
    void *ret[5];
	int nb_of_peers= 0;

	for(int i=0 ; i<3 ; i++ ){
        if( peers[i] != 0 )
            nb_of_peers++;
    }

	// Create 3 threads for each peer
	for( int i=0 ; i<nb_of_peers ; i++ ){
		if( pthread_create(&thread_id[i], NULL, &establish_connection_with_peer, &peers[i]) != 0 ){
			perror("pthread_create");
            return 1;
		}
	}

	// Wait for the threads to finish execution
    for(int i=0 ; i<nb_of_peers ; i++){
        if(pthread_join(thread_id[i], &ret[i]) != 0){
            perror("pthread_join");
            return 2;
        }
    }

	for(int i=0 ; i<nb_of_peers ; i++ ){
		// Search for a peer with whom the connection was successful
		// and return the peer_ip
		if( connection_vector[i] != 0 )
			return connection_vector[i];
	}

	printf("No peers available at the moment! Try again later!\n");
	return -1;
}

int main(int argc, char **argv) {
	// Write the CLI logic here
	// There can be other input arguments so just add them as needed

	// The input arguments should be a file name, and a path to a directory in 
	// which the file should be saved after the transfer is done
	// argv[1]: path
	// argv[2]: file name

	// Check if the user inputted the correct number of arguments
	if(3 != argc) {
		printf("Incorrect number of arguments given\n");
		printf("Usage: ./prog path_to_directory file_name\n");
		return -1;
	}

	// Check if the path given by the user is a valid one
	if(!(is_valid_path(argv[1]))) {
		printf("The given directory is not valid\n");
		return -1;
	}

	start_peer(argv[1], argv[2]);

	return 0;
}