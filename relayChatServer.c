/* * * *
* "relayChatServer.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * */

#include "relayChat.h" 

#define BACKLOG 10


void *manage_client(void *data);

int main(void) {
    //create db


	int ret, listen_fd, connect_fd; //catch on ret, listen on listen, new connect on connect
    struct addrinfo *server_info, *valid_sock;
	struct addrinfo pre_info;
    struct sockaddr_storage connecter_addr;
    socklen_t sin_size; //for identifying whether incoming connection is ipv4 or ipv6 
    char s[INET6_ADDRSTRLEN];
   // struct sigaction sig_action;

   //thread stuff
   pthread_t thread;
   int thread_ret = 1;



    //find and bind socket
	memset(&pre_info, 0, sizeof pre_info);
	pre_info.ai_family = AF_UNSPEC;
	pre_info.ai_socktype = SOCK_STREAM;
	pre_info.ai_flags = AI_PASSIVE;

	// error check
	if ((ret = getaddrinfo(NULL, PORT, &pre_info, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;						//how to communicate error back
	}

	for (valid_sock = server_info; valid_sock != NULL; valid_sock = valid_sock->ai_next) {
		if ((listen_fd = socket(valid_sock->ai_family, valid_sock->ai_socktype,
				valid_sock->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		// something about setsockopt so sockets can be reused??

		if (bind(listen_fd, valid_sock->ai_addr, valid_sock->ai_addrlen) == -1) {
			close(listen_fd);
			perror("server: bind");
			continue;
		}

		break;
	}

    // free result holder 
    freeaddrinfo(server_info);

    if (valid_sock == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    //reap dead processes?? why would we need to do this

    //listening

    printf("server: waiting for connections. . .\n");

    //accept() loop
    while(1) {
        sin_size = sizeof connecter_addr;
        connect_fd = accept(listen_fd, (struct sockaddr *)&connecter_addr, &sin_size);
        if (connect_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(connecter_addr.ss_family,
            get_ip_addr((struct sockaddr *)&connecter_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        thread_ret = pthread_create(&thread, NULL, manage_client, (void*)connect_fd);
    }


    exit(0);
}

/*
storeNewUser(char un, uint_? ip_addr) {
    //add un and ip_addr to user list;
}

acceptLogin(char un, uint_? ip_addr) {
    //isThisUserOnline() ?, if not call storeNewUser()
    //else ????
}

isThisUserOnline(un) {
    //is un in db?
}

join_room(char un, char room_name) {
    // if !(room_name)
        // create
    //find user_data un
    //update_roster(*room, user_data)
    //send_updated_roster(*room)
}

void send_updated_roster()

void unpack_packet() {
    //if . . . sort by type of packet to requisite series of actions
}


OBJECTS???
typedef struct user_data {
    char username;
    uint_? ip_addr;
}

typedef struct chat_room {
    char room_name[name_size];
    struct user_data *members[room_size];
    int curr_size;
}

ROOM MANAGEMENT FUNCTIONS
int update_roster (struct chat_room *this) {
        // ??
        //update and return this->curr_size?
    };

void send_updated_roster (struct chat_room *this) {
        //for all users in this->members:
            //send new roster to ip_addr
    };

void send_room_msg (struct chat_room *this, struct irc_packet_tell_msg *to_send)
        //for all users in this->members:
            // send to_send to ip_addr
    };
*/


void *manage_client(void *data) {
    int socket_fd = (int) data;

    if (send(socket_fd, "Hello, world!", 13, 0) == -1)
        perror("send");
    close(socket_fd);
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}