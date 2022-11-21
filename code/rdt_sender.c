#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include<pthread.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //millisecond

// initialization of global variables
int next_seqno=0; // sequence number for next packet to be sent
int send_base=0; // start of the window
int unack_send_base=0; //send base of an unacked packet 
int window_size = 10; // window size (in packets)
int send_max; // end of the window
int last_seqno = 0; // seq number of last packet when reach end of file
int timer_running = 0; // 1 if the timer is currently running

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;  
FILE *fp;   
pthread_mutex_t lock; 

struct thread_data {
   FILE *fp;
   int  sockfd;
   struct sockaddr_in serveraddr;
   int serverlen;
};

void send_packets(struct thread_data *data);
void receive_packets(struct thread_data *data);

void start_timer() {
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void resend_packets(int sig, struct thread_data *data) {
    if (sig == SIGALRM) {
        
        timer_running = 0;
        // Resend all packets range between send_base and send_max
        int len;
        char buffer[DATA_SIZE];
        VLOG(INFO, "Timeout happend");
        //starting from oldest unacked packet
        next_seqno = send_base;
        int max = send_max;
  
        // move the pointer to part of the file to be retransmitted
        fseek(fp, next_seqno, SEEK_SET);
        
        // resend all packets in the window
        while (next_seqno < max) { 
            len = fread(buffer, 1, DATA_SIZE, fp);
            
            if (len <= 0) {
                break;
            }
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = next_seqno;

            VLOG(DEBUG, "Sending unacked packet %d to %s", 
                    next_seqno, inet_ntoa(serveraddr.sin_addr));

            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                    ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
            error("sendto");
            }
            if (timer_running == 0) {
                start_timer();
                timer_running = 1;
            }
            free(sndpkt);
            next_seqno += len;
        }
    }
}

void stop_timer() {
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int)) {
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;  
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

int main (int argc, char **argv) {
    int portno; // len;
    char *hostname;
    send_max = send_base + window_size * DATA_SIZE;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    char filename[256];
    sprintf(filename, "../data_send/%s", argv[3]);
    fp = fopen(filename, "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");


    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    struct thread_data data;
    data.fp = fp;
    data.sockfd = sockfd;
    data.serveraddr = serveraddr;
    data.serverlen = serverlen;

    // start threading process
    // split the program in 2 threads
    // first thread t_1 sends data
    // second thread t_2 receives ACK messages

    pthread_t t_1, t_2;
    pthread_create(&t_1, NULL, send_packets, &data);
    pthread_create(&t_2, NULL, receive_packets, &data);

    pthread_join(t_1,NULL);
    pthread_join(t_2,NULL);

    return 0;

}

// function used by the sending thread
void send_packets(struct thread_data *data){

    int len;
    char buffer[DATA_SIZE];
    
    // create local variables to make the code more readable
    FILE *fp = data->fp;
    int sockfd = data->sockfd;
    int serverlen = data->serverlen;
    struct sockaddr_in serveraddr = data->serveraddr;

    init_timer(RETRY, resend_packets);
    
    while (1) {
        if (next_seqno < send_max){
            // we are in the window, a new packet can be sent
            len = fread(buffer, 1, DATA_SIZE, fp);
            if (len <= 0) {
                last_seqno = next_seqno;
                VLOG(INFO, "End Of File has been reached");
                break;
                
                //printf("sender send base: %d \n", send_base);
                // sndpkt = make_packet(0);
                // sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                       // (const struct sockaddr *)&serveraddr, serverlen);
                // exit(0);
                // break;
            }
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = next_seqno;

            VLOG(DEBUG, "Sending packet %d to %s", 
                    next_seqno, inet_ntoa(serveraddr.sin_addr));

            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                        ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
                error("sendto");
            }
            if (timer_running == 0) {
                start_timer();
                timer_running = 1;
            }
            free(sndpkt);
            next_seqno += len;
        }
    }
    
    //printf("sender send base: %d \n", send_base);
    //printf("sender last_seqno: %d \n", last_seqno);
    
    while (send_base < last_seqno) {
    	//printf("sender send base: %d \n", send_base);
    	//printf("sender last_seqno: %d \n", last_seqno);
    }
    
    sndpkt = make_packet(0);
    sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0, 
    	(const struct sockaddr *)&serveraddr, serverlen);
    exit(0);
}

// function used by the receiving thread
void receive_packets(struct thread_data *data){

    int len;
    char buffer[DATA_SIZE];

    // create local variables to make the code more readable
    FILE *fp = data->fp;
    int sockfd = data->sockfd;
    int serverlen = data->serverlen;
    struct sockaddr_in serveraddr = data->serveraddr;

    while (1) {
        if(recvfrom(sockfd, buffer, MSS_SIZE, 0, 
            (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0) {
            error("recvfrom");
        }
        recvpkt = (tcp_packet *)buffer;
        
        if (recvpkt->hdr.ackno > send_base) {
            stop_timer(); //stop timer, will restart when sender sends the next packet
            pthread_mutex_lock(&lock);
            send_base = recvpkt->hdr.ackno;
            send_max += DATA_SIZE;
            pthread_mutex_unlock(&lock);
  
            //if there are still unacked packets, restart timer
            if (next_seqno != send_base) { 
                start_timer();
            }
        }
    }
}

