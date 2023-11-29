/* * * *
* "relayChatClient.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * */

#include "relayChat.h" 
#include <string.h>

struct room;
typedef struct room
{
    char roomName[20];
    char members[MAXUSERS][20];
    int count;
    struct room * next;
}room_t;

void keepalive();
void receiver();
void sender();
void updateRoomMembership(struct irc_packet_list_resp * input);


pthread_mutex_t timestamp_mutex;
time_t lastSent;
time_t lastRecvd;
char username[20] = "";
int sock = 0;
int timeout = 0;
void * buf = NULL;//the buffer used by the receiver to hold incoming packets
char * inputBuf = NULL;//the buffer used by the sender to hold user input


room_t * roomHead = NULL;
pthread_t sendThrd, recvThrd, kaThrd;


void keepalive()
{
    time_t now = time(NULL);
    int connected = 1;
    while(1)
    {
        pthread_mutex_lock(&timestamp_mutex);
        if(now - lastRecvd >= 15)
        {//server is not sending heartbeats, assume connection lost
        //kill sender and receiver threads, set timeout flag and return to main.
            pthread_cancel(sendThrd);
            pthread_cancel(recvThrd);
            timeout = 1;
            return;
            //Attempt to reconnect?

        }
        if(now - lastSent >= 3)
        {//Outgoing heartbeat is stale
            //send heartbeat
        }
        pthread_mutex_unlock(&timestamp_mutex);
        sleep(3);
    }
    return;
}

void receiver()
{
    size_t capacity = sizeof(struct irc_packet_tell_msg) + MAXMSGLENGTH * sizeof(char);
    if(!buf)
    {
        buf = malloc(capacity);
    }
    
    size_t size = 0;
    while(1);
    {

        size = recv(sock, buf, capacity, 0);
        
        pthread_mutex_lock(&timestamp_mutex);
        lastRecvd = time(NULL);
        pthread_mutex_unlock(&timestamp_mutex);
        struct irc_packet_generic * input = buf;
        switch (input->header.opcode)
        {
        case IRC_OPCODE_ERR:
            //disconnect from server and terminate
            pthread_cancel(sendThrd);
            pthread_cancel(kaThrd);
            print("Recieved inalid data, disconnecting from server...\n");
            timeout = 0;
            return;
            break;
        case IRC_OPCODE_HEARTBEAT:
            //no work necessary, timestamp already updated
            break;
        case IRC_OPCODE_LIST_ROOMS_RESP:
            struct irc_packet_list_resp * roomlist = (struct irc_packet_list_resp *)input;
            
            break;
        case IRC_OPCODE_LIST_USERS_RESP:
            updateRoomMembership((struct irc_packet_list_resp *)input);
            break;
        case IRC_OPCODE_TELL_MSG:
        case IRC_OPCODE_TELL_PRIV_MSG:
            struct irc_packet_tell_msg * message = buf;
            int result = validate_string(message->sending_user, 20);
            if(message->header.length > 40 + MAXMSGLENGTH || message->header.length < 40)
            {
                //err invalid length
            }
            if(result != 1)
            {
                //err bad string
            }
            result = validate_string(message->target_name, 20);
            if(result != 1)
            {
                //err bad string
            }
            result = validate_string(message->msg, message->header.length - 40);
            if(result != 1)
            {
                //err bad string
            }
            if(strncmp(username, message->target_name, 20) == 0)
            {
                fprintf(stdout, "%.20s to %.20s: %.*s\n", message->sending_user, message->target_name, MAXMSGLENGTH, message->msg);
            }
            break;
        default:
            //invalid opcode
            struct irc_packet_error output;
            output.error_code = IRC_ERR_ILLEGAL_OPCODE;
            output.header.opcode = IRC_OPCODE_ERR;
            output.header.length = 4;
            send(sock, &output, sizeof(struct irc_packet_error), 0);
            //disconnect from server and terminate

            break;
        }


    }
}

void sender()
{
    if(!inputBuf)
    {
        inputBuf = malloc(MAXINPUTLENGTH * sizeof(char));
    }
    
    while (1)
    {
        //read a line from the user
        memset(inputBuf, '\0',  MAXINPUTLENGTH * sizeof(char));//Clear out the input buffer so it can satisfy the string expectations
        fgets(inputBuf, MAXINPUTLENGTH - 1, stdin);
        //parse the line to identify a command
        if(inputBuf[0] == '\\')
        {
            switch (inputBuf[1])
            {
            case 'r'://send message to only one room
                //usage \r roomname msgbody
            case 'u'://send message to only one user
                //usage \u username msgbody
                strtok(inputBuf, " ");
                char * rname = strtok(NULL, " ");
                char * msgbody = strtok(NULL, " ");
                int msglength = strlen(msgbody) + 1;
                if(msglength <= 1)
                {
                    if(inputBuf[1] == 'r')
                    {
                        print("usage \\r username msgbody\n");
                    }
                    else
                    {
                        print("usage \\u username msgbody\n");
                    }
                }
                else
                {
                    struct irc_packet_send_msg  * roommsg = malloc(sizeof(struct irc_packet_send_msg) + msglength);
                    if(inputBuf[1] == 'r')
                    {
                        roommsg->header.opcode = IRC_OPCODE_SEND_MSG;
                    }
                    else
                    {
                        roommsg->header.opcode = IRC_OPCODE_SEND_PRIV_MSG;

                    }
                    roommsg->header.length = 20 + msglength;
                    memset(roommsg->target_name, '\0', 20);
                    memset(roommsg->msg, '\0', msglength);
                    strncpy(roommsg->target_name, rname, msgbody - rname - 1);
                    send(sock, roommsg, sizeof(struct irc_packet_send_msg) + msglength, 0);
                    free(roommsg);
                }
                break;
            case 'j'://join (or create) a room
                //usage \j roomname
                if(strlen(inputBuf) <= 3)
                {
                    print("usage \\j roomname\n");
                }
                else
                {
                    //prepare message to send
                    struct irc_packet_join joinmsg;
                    joinmsg.header.opcode = IRC_OPCODE_JOIN_ROOM;
                    joinmsg.header.length = 20;
                    memset(joinmsg.room_name, '\0', 20);
                    strncpy(joinmsg.room_name, inputBuf + 3, 20);

                    //first check we are not already in the room

                    //add room name to list of rooms

                    //now let the server know we want to join the room
                    send(sock, &joinmsg, sizeof(struct irc_packet_join), 0);

                }
                break;
            case 'l'://list rooms
                //usage \l
                struct irc_packet_list_rooms outgoing;
                outgoing.header.opcode = IRC_OPCODE_LIST_ROOMS;
                outgoing.header.length = 0;
                send(sock, &outgoing, sizeof(struct irc_packet_list_rooms), 0);
                break;
            case 'e'://exit a room
                //usage \e roomname
                break;
            case 'q'://quit the server
                //usage \q
                break;
            default:
                //unrecognized command
                break;
            }
        }
        else
        {//forward message to all joined rooms

        }

        //construct a packet struct according to the command and send it


    }
    
}

void updateRoomMembership(struct irc_packet_list_resp * input)
{
    int inputCount = input->header.length / 20 - 1;
    room * current = roomHead;
    while(current && strncmp(current->roomName, input->identifier, 20) != 0)
    {
        current = current->next;
    }
    if(current)//matching room found
    {
        int myIndex = 0;
        int serverIndex = 0;
        int endSRC[10];
        int i = 0;
        while(serverIndex < inputCount && myIndex < current->count && i < 10)
        {
            int result = strncmp(current->members[myIndex], input->item_names[serverIndex], 20);
            
            if(result == 0)//name matches
            {
                ++serverIndex;
                ++myIndex;
                endSRC[i] = serverIndex;
            }
            else if(result < 0)
            {
                //the user at current->members[myIndex] has left the room
                fprintf(stdout, "%.20s left %.20s\n", current->members[myIndex], input->identifier);
                ++myIndex;
            }
            else
            {
                //new user has joined
                endSRC[i] = serverIndex;
                ++serverIndex;
                ++i;
                fprintf(stdout, "%.20s joined %.20s\n", input->item_names[serverIndex], input->identifier);
            }
        }
        while(serverIndex < inputCount && i < 10)
        {
            //new user has joined
            fprintf(stdout, "%.20s joined %.20s\n", input->item_names[serverIndex], input->identifier);
            endSRC[i] = serverIndex;
            ++serverIndex;
            ++i;
        }

        for(int j = 0; j < 10; ++j)//update the membership record of the room
        {
            memset(current->members[j], '\0', 20 * sizeof(char));
            strncpy(current->members[j], input->item_names[endSRC[j]], 20);
        }
        current->count = inputCount;

    }


    //else we are not a member of the room
}

int main(int argc, char *argv[]) {

    lastRecvd = time(NULL);
    lastSent = time(NULL);
    memset(username, '\0', 20 * sizeof(char));

    //init behavior
    //Get username from user

    //get server IP from user
    //open socket and connect to server

    //spin off children (keepalive, receiver, sender)
    pthread_create(&kaThrd, NULL, keepalive, NULL);
    pthread_create(&sendThrd, NULL, sender, NULL);
    pthread_create(&recvThrd, NULL, receiver, NULL);

    do
    {
        if(timeout)
        {
            //server disconnected due to timeout
            fprint(stdout, "The server is unresponsive. Do you want to attempt to reconnect? (y/n)\n");
            
        }
        pthread_join(kaThrd, NULL);
        pthread_join(sendThrd, NULL);
        pthread_join(recvThrd, NULL);
    } while (timeout);
    if(inputBuf)
    {
        free(inputBuf);
    }
    if(buf)
    {
        free(buf);
    }
    





    //nonsense tests start here
    struct irc_packet_send_msg * test = malloc( sizeof(struct irc_packet_send_msg) + 27 * sizeof(char));

    strcpy(test->msg, "abcdefghijklmnopqrstuwxyz");

    printf("%s\n", test->msg);

    fprintf(stdout, "%lu\n", sizeof(char));
    printf("%.7s\n", "0123456789");

    int count = 5;

    while(count >= 0)
    {  
        printf("%s\n", "snore");
        sleep(10);
        --count;
    }

    exit(0);
}
