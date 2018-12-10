/*
  @author: Chris Jativa
  @author: Mark Zgaljic
 
 radio utility
 Copyright (C) 2004  Hans Verkuil  <hverkuil@xs4all.nl>
 
 Enhanced with proper options and new functionality:
 Copyright (C) 2004  Brian Jackson
 
 Added -c option:
 Copyright (C) 2004  Mark Rafn
 
 Added more options and functionality:
 Copyright (C) 2005  Adam Forsyth  <agforsyth@gmail.com>
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>      /* standard C i/o facilities */
#include <stdlib.h>     /* needed for atoi() */
#include <unistd.h>     /* defines STDIN_FILENO, system calls,etc */
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>      /* gethostbyname */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#define __user
#include <linux/videodev2.h>
#include "ivtv.h"
#define RADIO_DEV "/dev/radio0"
#define AUDIO_IN_DEV "/dev/video24"
#define TRUE 1
#define FALSE 0

struct config {
	int just_tune;
	int passthrough;
	int channelchange;
	char prog_name[255];
	struct v4l2_tuner tuner;
	struct v4l2_frequency freq;
	int fh;
	char radio_dev[PATH_MAX];
	char audio_in[PATH_MAX];
	char play_cmd[PATH_MAX + 50];
	char *play_cmd_tmpl;
	int verbose;
	int div;
} cfg;

//BOOLEAN values
#define TRUE 1
#define FALSE 0

//universal global variables
char ioInputBuffer[20];
char station_list[300];
char* msg;  //set via command line arguments.
char* writeMsg = "";  //message to be sent back to the scheduler

//udp specific variables
int SERVER_PORT = 9002;
int CLIENT_PORT = 9001;
int serverSocket;
int bindMe;
int r;
int clientSocket;
struct sockaddr_in receive;  //struct holds ip address and port numbers.
struct sockaddr_in clientsAddress;
socklen_t clientDataLength;
const char* schedulerIP = "137.140.8.82";  //respond back to agile to IP address (local host)


//---------------------Inserted methods----------------------------
void myOpen(){
    if(strcmp(msg, "stdio") == 0)
        return;
    
    else if(strcmp(msg, "udp") == 0){
        //------opening---server code-----------
        serverSocket = socket(AF_INET,SOCK_DGRAM,0);  //creates socket
        memset(receive.sin_zero,'\0',sizeof(receive.sin_zero));  //clears all data inside servaddr struct
        receive.sin_family = AF_INET;
        receive.sin_addr.s_addr = INADDR_ANY;  //receive from any ip address
        receive.sin_port = htons(SERVER_PORT);  //port # to use
        bindMe = bind(serverSocket,(struct sockaddr*)&receive, sizeof(receive));  //binding...getting ready to listen for connections
        clientDataLength = sizeof(clientsAddress);
        
        //-----opening---client code-----------
        clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  //try making datagram socket
    }
}

void myRead(){
    if(strcmp(msg, "stdio") == 0)
    {
        printf("Scan or Tune:\n");
        char *buffer = NULL;
        unsigned long length;
        getline(&buffer, &length, stdin);
        int i;
        for(i = 0; i < 20; i++){
            if(i > length)
                break;
            ioInputBuffer[i] = *(buffer + i);
        }
        return;
        
    }else if(strcmp(msg, "udp") == 0){
        //block, wait for packets
        r = (int) recvfrom(serverSocket,ioInputBuffer,sizeof(ioInputBuffer),0,(struct sockaddr*)&clientsAddress, &clientDataLength);
        ioInputBuffer[r]= 0;
    }
}

void myWrite(){
    if(strcmp(msg, "stdio") == 0){
        printf("%s\n",station_list);
        
    }
    int value = strcmp(msg, "udp");
    if(value == 0){
        struct sockaddr_in send;
        
        send.sin_family = AF_INET;
        send.sin_port = htons(CLIENT_PORT);  //port to send packet to.
        send.sin_addr.s_addr = inet_addr(schedulerIP);
        
        /* Send the message in writeMsg to the server */
        sendto(clientSocket, writeMsg, (strlen(writeMsg)), 0, (struct sockaddr *)&send, sizeof(send));
    }
    
    memset(station_list, 0, sizeof(station_list));  //clears stationList array
    memset(ioInputBuffer, 0,sizeof(ioInputBuffer));  //clears ioInputBuffer
}


void myClose(){
    if(strcmp(msg, "stdio") == 0)
        return;
    else if(strcmp(msg, "udp") == 0){
        
        /* Deallocating all open sockets */
        close(serverSocket);
        close(clientSocket);
    }
    else
        return;
}

char getMessage()
{
    if(ioInputBuffer[0] == 's')
        return 's';
    else if(ioInputBuffer[0] == 's')
        return 't';
    else
        return ' ';
}
//-------------End of inserted methods---------------------


void print_usage(void)
{
    fprintf(stderr, "Usage: %s <options>\n", cfg.prog_name);
    fprintf(stderr, "Possible options are:\n");
    fprintf(stderr, "    -d <device>    Radio control device (default: %s)\n", RADIO_DEV);
    fprintf(stderr, "    -s             Scan for channels\n");
    fprintf(stderr, "    -a             Scan for frequencies\n");
    fprintf(stderr, "                   Use if Scan for channels is missing stuff\n");
    fprintf(stderr, "                   If e.g. 90.9, 91.1, 91.3 all exist use this\n");
    fprintf(stderr, "    -f <freq>      Tune to a specific frequency\n");
    fprintf(stderr, "    -j             Just tune (don't try to play the audio)\n");
    fprintf(stderr, "                   You'll have to play the audio yourself (see README.radio)\n");
    fprintf(stderr, "    -P             Use passthrough mode of card\n");
    fprintf(stderr, "    -g             Enable channel changing for passthrough mode\n");
    fprintf(stderr, "    -v             Verbose\n");
    fprintf(stderr, "    -h             Display this help message\n");
    fprintf(stderr, "    -i <device>    PCM audio capture device (default: %s)\n",AUDIO_IN_DEV);
    fprintf(stderr, "    -c <command>   Command to play audio.  This will be processed by\n");
    fprintf(stderr, "                   the shell, after substituting %%s with the audio device.\n");
    fprintf(stderr, "                   Default: \"%s\"\n",cfg.play_cmd_tmpl);
}

int scan_channels(int allfreqs)
{
    int i;
    station_list[0] = '\0';
    cfg.fh = open(cfg.radio_dev, O_RDONLY);
    if (cfg.fh == -1) {
        perror("radio");
        fprintf(stderr, "cannot open %s\n", cfg.radio_dev);
        exit(1);
    }
    
    cfg.tuner.index = 0;
    if (ioctl(cfg.fh, VIDIOC_G_TUNER, &cfg.tuner) == -1) {
        fprintf(stderr, "ioctl: Failed to set tuner (%s)\n",
                strerror(errno));
        exit(1);
    }
    cfg.div = (cfg.tuner.capability & V4L2_TUNER_CAP_LOW) ? 1000 : 1;
    int isafreq = 0;
    int freqindex = 0;
    int freqstrength;
    int rangehigh;
    int rangelow;
    double freqs[5][2];
    double printedfreq;
    
    if (allfreqs == 1) {
        rangelow = cfg.tuner.rangelow;
        rangehigh = cfg.tuner.rangehigh;
    } else {
        rangelow = 1392 * cfg.div;
        rangehigh = 1728 * cfg.div;
    }
    for (i = rangelow; i < rangehigh; i += cfg.div) {
        cfg.freq.tuner = 0;
        cfg.freq.type = V4L2_TUNER_RADIO;
        cfg.freq.frequency = i;
        if (ioctl(cfg.fh, VIDIOC_S_FREQUENCY, &cfg.freq) == -1) {
            fprintf(stderr, "ioctl: Failed to set freq (%s)\n",
                    strerror(errno));
            exit(1);
        }
        if (ioctl(cfg.fh, VIDIOC_G_TUNER, &cfg.tuner) == -1) {
            fprintf(stderr, "ioctl: Failed to set tuner (%s)\n",
                    strerror(errno));
            exit(1);
        }
        //print long freq info if wanted
        printedfreq = cfg.freq.frequency;
        freqstrength = cfg.tuner.signal;
        if (cfg.verbose) {
            printf("%3.2f, %4.0f: %d \n", printedfreq / (16.0 * cfg.div), printedfreq, freqstrength);
        }
        if (cfg.tuner.signal > 16384) {
            isafreq++;
            freqindex = isafreq - 1;
            //if there are multiple stations in a row, this may detect the 1st and not the rest.
            if (isafreq < 6) {
                freqs[freqindex][0] = cfg.freq.frequency;
                freqs[freqindex][1] = cfg.tuner.signal;
            }
        } else {
            if (isafreq > 0) {
                switch(isafreq) {
                    case 1:
                        printedfreq = freqs[0][0];
                        freqstrength = freqs[0][1];
                        break;
                    case 2:
                        printedfreq = (freqs[0][0] + freqs[1][0]) / 2;
                        freqstrength = (freqs[0][1] + freqs[1][1]) / 2;
                        break;
                    case 3:
                        printedfreq = freqs[1][0];
                        freqstrength = freqs[1][1];
                        break;
                    case 4:
                        printedfreq = (freqs[1][0] + freqs[2][0]) / 2;
                        freqstrength = (freqs[1][1] + freqs[2][1]) / 2;
                        break;
                    default:
                        printedfreq = freqs[2][0];
                        freqstrength = freqs[2][1];
                        break;
                }
                printedfreq /= cfg.div;
                if (printedfreq > 1392 && printedfreq < 1728) {
                    if ((((int)printedfreq) % 16) == 12) {
                        sprintf(station_list, "%s\n%3.1f FM", station_list, (printedfreq / 16.0) - 0.01);
                    } else if((((int)printedfreq) % 16) == 4) {
                        sprintf(station_list, " %3.1f FM\n", (printedfreq / 16.0) + 0.01);
                    } else {
                        sprintf(station_list, " %3.1f FM\n", (printedfreq / 16.0));
                    }
                }
            }
            isafreq = 0;
        }
    }
    close(cfg.fh);
    return 0;
}

static void cleanup(int signal)
{
    printf("Cleaning up\n");
    if (cfg.passthrough) {
        cfg.passthrough = 0;
        ioctl(cfg.fh, IVTV_IOC_PASSTHROUGH, &cfg.passthrough);
    }
    close(cfg.fh);
    exit(0);
}

int main(int argc, const char * argv[])
{
    if(strcmp(argv[1], "stdio") == 0)
        msg = "stdio";
    else if(strcmp(argv[1], "udp") == 0)
        msg = "udp";
    printf("Running via: %s", msg);
    
    //int opt;
    strcpy(cfg.prog_name, argv[0]);
    strcpy(cfg.radio_dev, RADIO_DEV);
    strcpy(cfg.audio_in, AUDIO_IN_DEV);
    cfg.play_cmd_tmpl = "aplay -f dat < %s";
    cfg.just_tune = TRUE;
    cfg.channelchange = TRUE;
    cfg.passthrough = FALSE;
    cfg.verbose = 0;
    
    /*
    if (argc < 2) {
        print_usage();
        exit(1);
    }
    */
    
    /*	while ((opt = getopt(argc, argv, "vghjPsad:f:i:c:")) != -1) {
     switch (opt) {  //delete this switch statement
     case 'j':
     cfg.just_tune = TRUE;
     break;
     case 'P':
     printf("Setting passthrough mode\n");
     cfg.passthrough = TRUE;
     break;
     case 'f':
     cfg.freq.frequency = (__u32)(atof(optarg) * 16000 + .5);  //optarg is the number of the frequency without 't'
     break;
     case 'd':
     strcpy(cfg.radio_dev, optarg);
     break;
     case 'i':
     strcpy(cfg.audio_in, optarg);
     break;
     case 'c':
     cfg.play_cmd_tmpl = optarg;
     break;
     case 'g':
     cfg.channelchange = TRUE;
     break;
     case 's':
     scan_channels(0);
     exit(0);
     case 'a':
     scan_channels(1);
     exit(0);
     case 'v':
     cfg.verbose = 1;
     break;
     case 'h':
     print_usage();
     exit(0);
     default:
     fprintf(stderr, "Unknown option\n");
     print_usage();
     exit(1);
     }
     }
     */
    cfg.fh = open(cfg.radio_dev, O_RDWR);
    if (cfg.fh == -1) {
        perror("radio");
        fprintf(stderr, "cannot open %s\n", cfg.radio_dev);
        exit(1);
    }
    ioctl(cfg.fh, VIDIOC_G_TUNER, &cfg.tuner);
    cfg.div = (cfg.tuner.capability & V4L2_TUNER_CAP_LOW) ? 1000 : 1;
    if (cfg.div == 1)
        cfg.freq.frequency /= 1000;
    
    cfg.freq.tuner = 0;
    cfg.freq.type = V4L2_TUNER_RADIO;
    if (cfg.freq.frequency) {
        printf("set to freq %3.2f\n", cfg.freq.frequency / (16.0 * cfg.div));
        ioctl(cfg.fh, VIDIOC_S_FREQUENCY, &cfg.freq);
        if (cfg.verbose) {
            printf("signal strength: %d\n", cfg.tuner.signal);
        }
    }
    
    if (cfg.passthrough) {
        if (ioctl(cfg.fh, IVTV_IOC_PASSTHROUGH, &cfg.passthrough) < 0) {
            fprintf(stderr, "passthrough ioctl failed\n");
            exit(1);
        }
        cfg.just_tune = TRUE;
    }
    
    signal(SIGPIPE, cleanup);
    signal(SIGHUP, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGUSR1, cleanup);
    signal(SIGINT, cleanup);
    cfg.just_tune = TRUE;
    
    /* if they want us to just tune, do that and wait, otherwise we play the output with aplay */
    if (cfg.just_tune) {
        //allow in-program channel changing for passthru mode / just tune mode
        if(cfg.channelchange) {
            printf("FM Agent about to enter while loop.");
            myOpen();
            
/*loop ->*/ while (TRUE){
    
                myRead();
    
                char whatToDo = getMessage();  //reads first char from ioInputBuffer (global)
                if(whatToDo == 's'){  //scan
                    scan_channels(1);
                    writeMsg = station_list;
                }
                else if(whatToDo == 't'){  //tune
                    char *buffer = NULL;
                    unsigned long length;
                    getline(&buffer, &length, stdin);
                    int i;
                    for(i = 0; i < 20; i++){   //extracts frequency # from string. ie: '100.3' from 't 100.3'
                        if(i > length)
                            break;
                        ioInputBuffer[i] = *(2+ buffer + i);
                    }
                    
                    //now set to to this frequency...replaced optarg with newfreqs.
                    cfg.freq.tuner = 0;
                    cfg.freq.type = V4L2_TUNER_RADIO;
                    cfg.freq.frequency = (__u32)(atof(ioInputBuffer) * 16000 + .5);  //from switch statement above
                    ioctl(cfg.fh, VIDIOC_S_FREQUENCY, &cfg.freq);
                    writeMsg = "ok";
                }else
                    goto skipWrite;  //place all this in a switch statement if time permits.

                myWrite();
    
                skipWrite:  ;
            }//end of while(TRUE) loop
            
            myClose();
            
        } else
            pause();
    }//end of if(cfg.just_tune)
    else  //else we don't 'just tune'...this should never happen!
    {
        struct v4l2_capability radio_cap;
        struct v4l2_capability audio_cap;
        int fd;
        
        ioctl(cfg.fh, VIDIOC_QUERYCAP, &radio_cap);
        fd = open(cfg.audio_in, O_RDONLY);
        audio_cap.bus_info[0] = 0;
        if (fd != -1) {
            ioctl(fd, VIDIOC_QUERYCAP, &audio_cap);
            close(fd);
        }
        if (strcmp((char *)radio_cap.bus_info, (char *)audio_cap.bus_info)) {
            fprintf(stderr, "%s belongs to a different ivtv driver then %s.\n", cfg.audio_in, cfg.radio_dev);
            fprintf(stderr, "Run ivtv-detect to discover the correct radio/PCM out combination.\n");
            exit(1);
        }
        
        snprintf(cfg.play_cmd, sizeof(cfg.play_cmd), cfg.play_cmd_tmpl, cfg.audio_in);
        printf("Running: %s\n", cfg.play_cmd);
        if ((system(cfg.play_cmd)) == -1) {
            fprintf(stderr, "Failed to play audio with aplay\n");
            fprintf(stderr, "Make sure you have it installed\n");
            exit(1);
        }
    }
    
    cleanup(0);
    return 0;
}
