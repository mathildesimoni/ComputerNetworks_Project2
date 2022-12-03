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
#include <pthread.h>
#include <math.h>

#include "packet.h"
#include "common.h"

#define STDIN_FD 0
#define RETRY 120 // millisecond

// initialization of global variables
int next_seqno = 0;      // sequence number for next packet to be sent
int send_base = 0;       // start of the window
int unack_send_base = 0; // send base of an unacked packet
int window_size = 10;    // window size (in packets)
double CWND = 1;            // congestion window size
int ssthresh = 64;
//double whole_number = 0;    // to keep track if 1/CWND congestion avoidance sums to a whole number
int send_max;            // end of the window
int last_seqno = 0;      // seq number of last packet when reach end of file
int timer_running = 0;   // 1 if the timer is currently running
int end_transfer = 0;    // = 1 when all packets for the file have been sent
int last_packet_rcv = 0; // = 1 when receiver received last empty packet and finished running
int dup_ack = 0;         // checking when receiving duplicate acks
int num_dup = 0;         // number of duplicate acks
int slow_start = 1;
int congestion_avoidance = 0;
// time_t t;
// char log_line[256]; // line to store in log file

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer;
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;
FILE *fp;
FILE* log_file;
pthread_mutex_t lock;

struct thread_data
{
    FILE *fp;
    int sockfd;
    struct sockaddr_in serveraddr;
    int serverlen;
};

void send_packets(struct thread_data *data);
void receive_packets(struct thread_data *data);

void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

void resend_packets(int sig)
{ // struct thread_data *data

    if (sig == SIGALRM)
    {
        VLOG(INFO, "Timeout \n");
    }
    else
    {
        VLOG(INFO, "3 duplicate ACKs \n");
    }

    // check if need to pass to congestion avoidance
    // if packet is lost in slow-start phase
    // if (slow_start == 1) {
    //     // packet was lost in slow start phase, go back to start in slow start and update ssthresh
    //     ssthresh = (int)ceil(fmax((double)CWND / (double)2, 2));
    //     CWND = 1;
    // }

    timer_running = 0;

    // all packets for the file have been sent and received
    // only need to receive last empty packet to let know the
    // receiver that the transfert is completed
    if (end_transfer == 1)
    {
        sndpkt = make_packet(0);
        sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
               (const struct sockaddr *)&serveraddr, serverlen);
        start_timer();
        timer_running = 1;
    }
    else
    {

        // only resend the last unacked packet
        int len;
        char buffer[DATA_SIZE];

        printf("resending the last unacked packet\n");

        // move the pointer to part of the file to be retransmitted
        fseek(fp, send_base, SEEK_SET);
        len = fread(buffer, 1, DATA_SIZE, fp);

        sndpkt = make_packet(len);
        memcpy(sndpkt->data, buffer, len);
        sndpkt->hdr.seqno = send_base;

        VLOG(DEBUG, "Sending unacked packet %d to %s",
             send_base, inet_ntoa(serveraddr.sin_addr));

        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                   (const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }
        if (timer_running == 0)
        {
            start_timer();
            timer_running = 1;
        }
        // free(sndpkt);
        // if packet is lost in slow-start or congestion avoidance phase
        // go back to start of slow start for 2 cases
        ssthresh = (int)ceil(fmax((double)CWND / (double)2, 2));
        CWND = 1;
        slow_start = 1;
        congestion_avoidance = 0;
        // printf("In resend, ssthresh is now: %d\n", ssthresh);
        // CWND = 1;
        // whole_number = 0;
    }

    // if (sig == SIGALRM) { //SIGALRM

    // timer_running = 0;
    // VLOG(INFO, "Timeout happend");

    // all packets for the file have been sent and received
    // only need to receive last empty packet to let know the
    // receiver that the transfert is completed
    // if (end_transfer == 1){
    // sndpkt = make_packet(0);
    // sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
    // (const struct sockaddr *)&serveraddr, serverlen);
    // start_timer();
    // timer_running = 1;
    // }
    // else {

    // only resend the last unacked packet
    // int len;
    // char buffer[DATA_SIZE];
    // printf("resending the last unacked packet\n");

    // move the pointer to part of the file to be retransmitted
    // fseek(fp, send_base, SEEK_SET);
    // len = fread(buffer, 1, DATA_SIZE, fp);

    // sndpkt = make_packet(len);
    // memcpy(sndpkt->data, buffer, len);
    // sndpkt->hdr.seqno = send_base;

    // VLOG(DEBUG, "Sending unacked packet %d to %s",
    //   send_base, inet_ntoa(serveraddr.sin_addr));

    // if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
    //  ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
    //  error("sendto");
    // }
    // if (timer_running == 0) {
    // start_timer();
    // timer_running = 1;
    // }
    // free(sndpkt);

    // // Resend all packets range between send_base and send_max
    // int len;
    // char buffer[DATA_SIZE];
    // //starting from oldest unacked packet
    // next_seqno = send_base;
    // int max = send_max;

    // // move the pointer to part of the file to be retransmitted
    // fseek(fp, next_seqno, SEEK_SET);

    // // resend all packets in the window
    // while (next_seqno < max) {
    //  len = fread(buffer, 1, DATA_SIZE, fp);

    //  if (len <= 0) {
    //      break;
    //  }
    //  sndpkt = make_packet(len);
    //  memcpy(sndpkt->data, buffer, len);
    //  sndpkt->hdr.seqno = next_seqno;

    //  VLOG(DEBUG, "Sending unacked packet %d to %s",
    //          next_seqno, inet_ntoa(serveraddr.sin_addr));

    //  if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
    //          ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
    //  error("sendto");
    //  }
    //  if (timer_running == 0) {
    //      start_timer();
    //      timer_running = 1;
    //  }
    //  free(sndpkt);
    //  next_seqno += len;
    // }
    //}
    // }
    // else if (sig == 8) {
    // 3 puplicates
    // timer_running = 0;
    // stop_timer();
    //  VLOG(INFO, "3 puplicate ACKs happened");

    // only resend the last unacked packet
    // int len;
    // char buffer[DATA_SIZE];

    // move the pointer to part of the file to be retransmitted
    // fseek(fp, send_base, SEEK_SET);
    // len = fread(buffer, 1, DATA_SIZE, fp);

    // sndpkt = make_packet(len);
    // memcpy(sndpkt->data, buffer, len);
    // sndpkt->hdr.seqno = send_base;

    // VLOG(DEBUG, "Sending unacked packet %d to %s",
    //        send_base, inet_ntoa(serveraddr.sin_addr));

    // if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
    //     ( const struct sockaddr *)&serveraddr, serverlen) < 0) {
    //  error("sendto");
    // }
    //  if (timer_running == 0) {
    //   start_timer();
    //  timer_running = 1;
    // }
    // free(sndpkt);
    //  }
    // free(sndpkt);
}

void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int))
{
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000; // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    timer.it_value.tv_sec = delay / 1000; // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

int main(int argc, char **argv)
{
    int portno; // len;
    char *hostname;

    send_max = send_base + (int) floor(CWND) * DATA_SIZE;
    // printf("send_max in main func: %d\n", send_max);
    if (slow_start == 1) {printf("congestion phase: slow_start\n");}
    if (congestion_avoidance == 1) {printf("congestion phase: congestion_avoidance\n");}

    /* check command line arguments */
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    char filename[256];
    sprintf(filename, "../data_send/%s", argv[3]);
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* initialize server server details */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0)
    {
        fprintf(stderr, "ERROR, invalid host %s\n", hostname);
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

    pthread_join(t_1, NULL);
    pthread_join(t_2, NULL);

    return 0;
}

// function used by the sending thread
void send_packets(struct thread_data *data)
{

    int len;
    char buffer[DATA_SIZE];

    // create local variables to make the code more readable
    FILE *fp = data->fp;
    int sockfd = data->sockfd;
    int serverlen = data->serverlen;
    struct sockaddr_in serveraddr = data->serveraddr;

    init_timer(RETRY, resend_packets);

    while (1)
    {   
        if (next_seqno < send_max)
        {
            // we are in the window, a new packet can be sent
            fseek(fp, next_seqno, SEEK_SET);
            len = fread(buffer, 1, DATA_SIZE, fp);
            if (len <= 0)
            {
                last_seqno = next_seqno;
                VLOG(INFO, "End Of File has been reached");
                break;
            }
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = next_seqno;

            VLOG(DEBUG, "Sending packet %d to %s",
                 next_seqno, inet_ntoa(serveraddr.sin_addr));

            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                       (const struct sockaddr *)&serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
            if (timer_running == 0)
            {
                start_timer();
                timer_running = 1;
            }
            free(sndpkt);
            next_seqno += len;
        }
    }

    while (send_base < last_seqno)
    {
        // wait all packets are ACKed
    }

    // need to send last empty packet to notify the receiver that
    // the transfert is completed
    end_transfer = 1;
    stop_timer();
    sndpkt = make_packet(0);
    sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
           (const struct sockaddr *)&serveraddr, serverlen);
    start_timer();
    timer_running = 1;
    while (last_packet_rcv == 0)
    {
        // make sure the received received the last empty packet
    }
    exit(0);
}

// function used by the receiving thread
void receive_packets(struct thread_data *data)
{
    // opening file CWND.csv to store CWND evolution
    log_file = fopen("../analysis/CWND.csv", "wb");
    if (log_file == NULL)
    {
        error("opening log_file");
    }

    // variables to get current time
    struct timeval t;
    char log_line[256]; // line to store in log file
    int milli;
    int micro;
    char tmp_buffer[80];
    char curTime[84];

    int len;
    char buffer[DATA_SIZE];

    // create local variables to make the code more readable
    FILE *fp = data->fp;
    int sockfd = data->sockfd;
    int serverlen = data->serverlen;
    struct sockaddr_in serveraddr = data->serveraddr;

    while (1)
    {
        // get current time and output to log_file
        gettimeofday(&t, NULL);
        // milli = t.tv_usec / 1000;
        micro = t.tv_usec;
        strftime(tmp_buffer, 80, "%H:%M:%S", localtime(&t.tv_sec));
        // curTime[84] = "";
        curTime[83] = "";
        sprintf(curTime, "%s:%d", tmp_buffer, micro);
        sprintf(log_line, "%s,%0.2f\n", curTime, CWND);
        fwrite(log_line, sizeof(log_line), 1, log_file);

        printf("BEFORE receiving packet CWND: %0.2f\n", CWND);
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                     (struct sockaddr *)&serveraddr, (socklen_t *)&serverlen) < 0)
        {
            error("recvfrom");
        }
        recvpkt = (tcp_packet *)buffer;

        if (dup_ack != recvpkt->hdr.ackno)
        {
            dup_ack = recvpkt->hdr.ackno;
            num_dup = 1;
        }
        else
        { // dup_ack == recvpkt->hdr.ackno
            // increase number of duplicate acks if receive ack for same packet
            num_dup += 1;
        }

        // if receive 3 duplicate acks
        if (num_dup == 3)
        {
            dup_ack = 0;
            num_dup = 0;
            printf("3 duplicate acks received, packet lost.\n");
            // printf("CONGESTION AVOIDANCE \n");
            // printf("current window size in lost pakcet: %f\n", CWND);
            // CWND += (double) 1 / (double) CWND;
            // send_max = send_base + (int) floor(CWND) * DATA_SIZE;

            // // whole_number += (double) 1 / (double) CWND;
            // // if ((int) floor(whole_number) == 1)
            // // {
            // //     CWND++;
            // //     whole_number = 0;
            // //     send_max = send_base + CWND * DATA_SIZE;
            // // }

            stop_timer();
            resend_packets(2); // 2 is random here, no particular meaning
        }

        if (end_transfer == 1)
        {
            // last empty packet was received, received stoped running
            last_packet_rcv = 1;
        }
        else
        {
            if (recvpkt->hdr.ackno > send_base)
            {
                stop_timer(); // stop timer
                pthread_mutex_lock(&lock);
                // move window (keeping same packet)
                send_base = recvpkt->hdr.ackno;
                // send_max += DATA_SIZE;
                // if (slow_start == 1) {
                //     CWND ++;
                //     send_max = send_base + (int) floor(CWND) * DATA_SIZE;

                // }
                if (CWND < ssthresh)
                {
                    // slow start case
                    //printf("current window size: %f\n", CWND);
                    CWND++; 
                    send_max = send_base + (int) floor(CWND) * DATA_SIZE;
                    //printf("SLOW START \n");
                    slow_start = 1;
                    congestion_avoidance = 0;
                }
                else // (CWND > ssthresh)
                {
                    //printf("CONGESTION AVOIDANCE \n");
                    //printf("current window size in congestion avoidance: %f\n", CWND);
                    // whole_number += (double) 1 / (double) CWND;
                    // printf("whole number : %f\n", whole_number);
                    // if ((int) floor(whole_number) == 1)
                    // {
                    //     printf("whole number reached 1\n");
                    //     CWND++;
                    //     whole_number = 0;
                    //     send_max = send_base + CWND * DATA_SIZE;
                    // }
                    CWND += (double) 1 / (double) CWND;
                    send_max = send_base + (int) floor(CWND) * DATA_SIZE;
                    slow_start = 0;
                    congestion_avoidance = 1;
                }
                pthread_mutex_unlock(&lock);

                // if there are still unacked packets, restart timer
                if (next_seqno != send_base)
                {
                    start_timer();
                }
            }
        }
        printf("AFTER receiving packet CWND: %0.2f\n", CWND);
        if (slow_start == 1) {printf("congestion phase: slow_start\n");}
        if (congestion_avoidance == 1) {printf("congestion phase: congestion_avoidance\n");}
        printf("ssthresh: %d \n", ssthresh);
    }
    fclose(log_file);
}
