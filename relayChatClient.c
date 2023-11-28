/* * * *
* "relayChatClient.c"
* RelayChat by Micah Lorenz and Tierney McBride
* * * * */

#include "relayChat.h" 
#include <string.h>

void keepalive();
void receiver();


pthread_mutex_t timestamp_mutex;
time_t lastSent;
time_t lastRecvd;
pthread_mutex_t conMTX;
int connected = 1;
char username[20] = "";


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
            /* code */
            break;
        case IRC_OPCODE_HEARTBEAT:
            //no work necessary, timestamp already updated
            break;
        case IRC_OPCODE_LIST_ROOMS_RESP:

            break;
        case IRC_OPCODE_LIST_USERS_RESP:
            break;
        case IRC_OPCODE_TELL_MSG:
            break;
        case IRC_OPCODE_TELL_PRIV_MSG:
            break;
        default:
            //invalid opcode

            break;
        }


        pthread_mutex_lock(&conMTX);
    }
}

int main(int argc, char *argv[]) {

    lastRecvd = time(NULL);
    lastSent = time(NULL);

    struct irc_packet_send_msg * test = malloc( sizeof(struct irc_packet_send_msg) + 27 * sizeof(char));

    strcpy(test->msg, "abcdefghijklmnopqrstuwxyz");

    printf("%s\n", test->msg);




    exit(0);
}
