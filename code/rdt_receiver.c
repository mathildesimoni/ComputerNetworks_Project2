#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"
#define buffsize 100


tcp_packet *recvpkt;
tcp_packet *sndpkt;
tcp_packet *lostpkt_buffer[buffsize]; //random buffer size for now

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    char dup_buff[MSS_SIZE];
    struct timeval tp;
    int exp_seqno = 0; // expected sequence number
    int nb_out_order_packets = 0;
    int i_buf;
    int check_packet_buffered;
    int count;

    int i = 0; //iterator 
    while(i < buffsize){
            lostpkt_buffer[i] = make_packet(MSS_SIZE);
            lostpkt_buffer[i]->hdr.data_size = 0; // Assign all created packets to have a data_size of 0 for empty packets
            lostpkt_buffer[i]->hdr.seqno = -1; // Assign all packet hdr.seqno's to -1 for exmpty packets
            i++;
    }

    // check command line arguments 
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    char filename[256];
    sprintf(filename, "../data_recv/%s", argv[2]);
    fp  = fopen(filename, "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        if ( recvpkt->hdr.data_size == 0) {
            VLOG(INFO, "End Of File has been reached");
            fclose(fp);
            
            // notify the sender that it knows the transfer is completed
            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            sndpkt->hdr.ctr_flags = ACK;
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }
            break;
        }
        
        // sendto: ACK back to the client 
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "Packet received: %lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);
        
        printf("Expected seq was: %d \n", exp_seqno);
        
        // send ACK back to the client only if in-order packet
        if (recvpkt->hdr.seqno != exp_seqno) {
            printf("Out of order packet! \n\n");

            // check that the out of order packet isn't already in the buffer
            i_buf = 0;
            check_packet_buffered = 0;
            while (i_buf < buffsize){
                if (lostpkt_buffer[i_buf]->hdr.seqno == recvpkt->hdr.seqno) {
                    check_packet_buffered = 1;
                    break;
                }
                i_buf ++;
            }

            // if packet already buffered, no need to buffer it again
            // if packet is not buffered yet, add it to the buffer
            if (check_packet_buffered == 0) {
                i_buf = 0;
                while (i_buf < buffsize) {
                    if (lostpkt_buffer[i_buf]->hdr.seqno == -1) {
                        // can store the out of order packet here
                        memcpy(lostpkt_buffer[i_buf], recvpkt, MSS_SIZE);
                        break;
                    }
                    i_buf ++;
                 }   
            }
            // if the buffer is full, the out of order packet will NOT be stored

            // send an ACK  with expected sequence number 
            // expected sequence number is not the seq no of the received packet here
            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = exp_seqno;
            sndpkt->hdr.ctr_flags = ACK;
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }

            // buffer the packet and send a duplicate ack 
            // i = 0;
            // while(i<buffsize){
            //     if(lostpkt_buffer[i]->hdr.seqno == recvpkt->hdr.seqno){
            //         memcpy(lostpkt_buffer[i], recvpkt, MSS_SIZE);
            //         sndpkt = make_packet(0);
            //         sndpkt->hdr.ackno = lostpkt_buffer[i]->hdr.seqno + lostpkt_buffer[i]->hdr.data_size;
            //         sndpkt->hdr.ctr_flags = ACK;
            //         if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
            //             (struct sockaddr *) &clientaddr, clientlen) < 0) {
            //             error("ERROR in sendto");
            //         }
            //     }
            //     i++;
            // }
        }
        else {
            printf("\n");
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);

            // update expected sequence number
            exp_seqno += recvpkt->hdr.data_size;
            
            // check if out of order packets with seqno > recvpkt.seqno were buffered
            // if yes, write them to the file, remove from buffer and update the ackno before sending ACK
            count = 0;
            while (1) {
                if (count == buffsize) {
                    // no out of order packet following
                    break;
                }
                count = 0;
                i_buf = 0;
                while (i_buf < buffsize){
                    if (lostpkt_buffer[i_buf]->hdr.seqno == exp_seqno) {
                        // write packet to the file
                        fseek(fp, exp_seqno, SEEK_SET);
                        fwrite(lostpkt_buffer[i_buf]->data, 1, lostpkt_buffer[i_buf]->hdr.data_size, fp);

                        // increase expected sequence number
                        exp_seqno += lostpkt_buffer[i_buf]->hdr.data_size;

                        // remove the packet from the buffer
                        lostpkt_buffer[i_buf] = make_packet(MSS_SIZE);
                        lostpkt_buffer[i_buf]->hdr.data_size = 0;
                        lostpkt_buffer[i_buf]->hdr.seqno = -1;
                        break;
                    }
                    count ++;
                    i_buf ++;
                }
            }

            sndpkt = make_packet(0);
            sndpkt->hdr.ackno = exp_seqno;
            // sndpkt->hdr.ackno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            sndpkt->hdr.ctr_flags = ACK;
            if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                    (struct sockaddr *) &clientaddr, clientlen) < 0) {
                error("ERROR in sendto");
            }
            // exp_seqno += recvpkt->hdr.data_size;
        }
    }
    return 0;
}
