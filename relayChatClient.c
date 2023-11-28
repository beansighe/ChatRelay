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
}room;

void keepalive();
void receiver();
void sender();
void updateRoomMembership(struct irc_packet_list_resp * input);


pthread_mutex_t timestamp_mutex;
time_t lastSent;
time_t lastRecvd;
pthread_mutex_t conMTX;
int connected = 1;
char username[20] = "";
int sock = 0;
room * roomHead = NULL;


void keepalive()
{
    time_t now = time(NULL);
    int connected = 1;
    while(1)
    {
        pthread_mutex_lock(&timestamp_mutex);
        if(now - lastRecvd >= 15)
        {//server is not sending heartbeats, assume connection lost
            //Attempt to reconnect?
            pthread_mutex_lock(&conMTX);


            pthread_mutex_unlock(&conMTX);
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
    void * buf = malloc(sizeof(struct irc_packet_tell_msg) + MAXMSGLENGTH * sizeof(char));
    pthread_mutex_lock(&conMTX);
    while(connected);
    {
        pthread_mutex_unlock(&conMTX);

        //recv(sock, );
        
        pthread_mutex_lock(&timestamp_mutex);
        lastRecvd = time(NULL);
        pthread_mutex_unlock(&timestamp_mutex);
        struct irc_packet_generic * input = buf;
        switch (input->header.opcode)
        {
        case IRC_OPCODE_ERR:
            //disconnect from server and terminate
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
            send(sock, &output, sizeof(struct irc_packet_error), 0);
            //disconnect from server and terminate

            break;
        }


        pthread_mutex_lock(&conMTX);
    }
}

void sender()
{

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





    //nonsense tests start here
    struct irc_packet_send_msg * test = malloc( sizeof(struct irc_packet_send_msg) + 27 * sizeof(char));

    strcpy(test->msg, "abcdefghijklmnopqrstuwxyz");

    printf("%s\n", test->msg);

    fprintf(stdout, "%lu\n", sizeof(char));
    printf("%.7s\n", "0123456789");

    exit(0);
}
