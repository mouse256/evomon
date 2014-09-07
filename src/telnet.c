/*
 * EvoMon monitoring daemon
 * Copyright (C) 2014 Tom Billiet

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <sys/socket.h>       //  socket definitions
#include <sys/types.h>        //  socket types
#include <arpa/inet.h>        //  inet (3) funtions
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "telnet.h"
#include "honeywelldecode.h"

#define PORT 3123
#define MAX_LINE 1024

/*  Read a line from a socket  */

ssize_t Readline(int sockd, void *vptr, size_t maxlen) {
    ssize_t n, rc;
    char    c, *buffer;

    buffer = vptr;

    for ( n = 1; n < maxlen; n++ ) {

        if ( (rc = read(sockd, &c, 1)) == 1 ) {
            *buffer++ = c;
            if ( c == '\n' )
                break;
        }
        else if ( rc == 0 ) {
            if ( n == 1 )
                return 0;
            else
                break;
        }
        else {
            if ( errno == EINTR )
                continue;
            return -1;
        }
    }

    *buffer = 0;
    return n;
}




void start_telnet(void * args)
{
	printf("Starting telnet...\n");

    int       list_s;                //  listening socket
    int       conn_s;                //  connection socket
    //short int port;                  //  port number
    struct    sockaddr_in servaddr;  //  socket address structure
    char      buffer[MAX_LINE];      //  character buffer
    //char     *endptr;                //  for strtol()

    //  Create the listening socket
    if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
        exit(1);
    }


    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(PORT);

    int yes = 1;
    if (setsockopt(list_s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    /*  Bind our socket addresss to the
        listening socket, and call listen()  */
    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error calling bind()\n");
        exit(-1);
    }

    if ( listen(list_s, 1024) < 0 ) {
        fprintf(stderr, "ECHOSERV: Error calling listen()\n");
        exit(-1);
    }


    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {

        /*  Wait for a connection, then accept() it  */

        if ( (conn_s = accept(list_s, NULL, NULL) ) < 0 ) {
            fprintf(stderr, "ECHOSERV: Error calling accept()\n");
            exit(-1);
        }

        strcpy(buffer, "zone;sensor;zoneName;sensorName;tempSet;tempSetZone;tempMeasured;tempMeasuredZone\n");
        write(conn_s, buffer, strlen(buffer));
        int i;
        for (i = 0; i < tempMsgCount; i++) {
        	sprintf(buffer, "%d;0x%x;%s;%s;%.02f;%.02f;%.02f;%.02f\n",
        			tempMsg[i]->zone,
        			tempMsg[i]->sensor,
        			tempMsg[i]->zone_name,
					tempMsg[i]->sensor_name,
        			printTemp(tempMsg[i]->temp_set),
        			printTemp(tempMsg[i]->temp_set_zone),
					printTemp(tempMsg[i]->temp_measured),
					printTemp(tempMsg[i]->temp_measured_zone)
        			);
        	write(conn_s, buffer, strlen(buffer));
        }

        /*  Retrieve an input line from the connected socket
            then simply write it back to the same socket.     */

//        Readline(conn_s, buffer, MAX_LINE-1);
//        printf("TELNET: %s\n", buffer);
//        char msg[] = "BLABLA\n";
//        write(conn_s, msg, strlen(msg));
//        write(conn_s, msg, strlen(msg));
//        write(conn_s, msg, strlen(msg));
//        //Writeline(conn_s, buffer, strlen(buffer));


        /*  Close the connected socket  */

        if ( close(conn_s) < 0 ) {
            fprintf(stderr, "ECHOSERV: Error calling close()\n");
            exit(-1);
        }
    }



	pthread_exit(0);
}
