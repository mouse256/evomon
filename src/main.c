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

#define CONFIGFILE "/.evo2qeo.conf"


#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include  <signal.h>

#include <qeo/api.h>
#include "serial.h"
#include "honeywelldecode.h"
#include "main.h"

#ifdef USE_TELNET
#include <pthread.h>
#include "telnet.h"
#endif

static qeo_state_writer_t *_temp_writer;
//static qeo_event_writer_t *_raw_writer;
int running = 0;



int charToInt(char n)
{
	if (n >= '0' && n <= '9') {
         return (n-'0');
    }
	if (n >= 'A' && n <= 'F') {
		return (n-'A'+10);
	}
	if (n >= 'a' && n <= 'f') {
		return (n-'a'+10);
	}
	return 0;
}


void processMsg(char * msg)
{
	//printf("MSG: %s\n", msg);
	char * tok = strtok(msg, " ");
	int maxMsgSize = 256;
	unsigned char out[maxMsgSize];
	int i = 0;
	int size = 0;
	printf("\nRAW: ");
	while(tok != NULL) {
		if (strlen(tok) != 2) {
			printf("Invalid sized byte: %s\n", tok);
			return;
		}
		out[size] = charToInt(tok[0]) << 4;
		out[size] += charToInt(tok[1]);
		printf("%02X ", out[size]);

		//read next
		i++;
		size++;
		if (size == maxMsgSize) {
			//normally a message is < 50 bytes. if we exceed this limit we can safely assume it was rubbish data and ignore it
			printf("...\nMessage is too big, can't parse it\n");
			return;
		}
		tok = strtok(NULL, " ");
	}
	//printf("\nparsed %d bytes\n", size);
	//for (i = 0; i < size; i++) {
	//	printf("%02X ", out[i]);
	//}
	printf("\n");

	manchesterDecode(out, size);


	fflush(stdout); //flush stdout

}

int processBuf(char * buf, int size)
{
	int i;
	int start = 0;
	for (i = 0; i < size; ++i) {
		if (buf[i] == '\n') {
			//found endline
			char msg[2048];
			if (i - start >= 2048) {
				//message is too long, we can safely assume it's rubbish and ignore
				printf("\nERROR: got a message that is too long - ignoring\n");
				start = i + 1;
				continue;
			}
			//convert into a null terminated string
			snprintf(msg, i - start, "%.*s", i - start, buf + start);
			processMsg(msg);
			start = i + 1;
		}
	}
	if (start > 0) {
		//found at least a msg
		for (i = 0; i < size - start; ++i) {
			//copy bytes to the start of the buffer
			buf[i] = buf[start + i];
		}
	}
	return size - start;
}

void signalHandler(int signal)
{
	printf("\ngoing to shutdown!\n");
	running = 0; //ctrl-c hit
}

void read_loop()
{
	int buf1Max = 256;
	int buf2Max = 4096;
	char buf[buf1Max];
	char buf2[buf2Max];
	int buf2size = 0;
	running = 1;

	signal (SIGQUIT, signalHandler);
	signal (SIGINT, signalHandler);

	while(running) {
			int n = serial_read(buf, sizeof(buf));

			if (n > 0) {
				if (n > sizeof(buf)) {
					printf("ERROR: n > buf: %d %zd\n", n, sizeof(buf));
					continue;
				}
				//printf("Read %d bytes\n", n);
				//printf("%.*s\n", n, buf);
				//copy into the process buffer
				if (buf2size + n >= buf2Max) {
					fprintf(stderr, "ERROR: buffer misalign: %d -- %d\n", buf2size, n);
				}
				memcpy(buf2 + buf2size, buf, n);
				buf2size += n;

				buf2size = processBuf(buf2, buf2size);
				if (buf2size + buf1Max >= buf2Max) {
					//even after processing there's still a lot left in the buffer
					//not enough to handle another input of buf1
					//so discard all the data, it can't be useful
					printf("\nERROR: Buffer contains too much data, throwing %d bytes away\n", buf2size);
					buf2size = 0;
				}
			}
			usleep(100000); //100ms
		}
}

float printTemp(int temp)
{
	if (temp == -1) {
		return 0;
	}
	return ((float) temp) / 100;
}


void logToFile()
{
	FILE * fp;

	fp = fopen("state.out", "w");
	if (fp == NULL) {
		fprintf(stderr, "WARN: can't open state.out for writing\n");
		return;
	}
	fprintf(fp, "zone;sensor;zoneName;sensorName;tempSet;tempSetZone;tempMeasured;tempMeasuredZone;heatDemandZone\n");
	int i;
	for (i = 0; i < tempMsgCount; i++) {
		fprintf(fp, "%d;0x%x;%s;%s;%.02f;%.02f;%.02f;%.02f;%d\n",
			tempMsg[i]->zone,
			tempMsg[i]->sensor,
			tempMsg[i]->zoneName,
			tempMsg[i]->sensorName,
			printTemp(tempMsg[i]->tempSet),
			printTemp(tempMsg[i]->tempSetZone),
			printTemp(tempMsg[i]->tempMeasured),
			printTemp(tempMsg[i]->tempMeasuredZone),
            tempMsg[i]->heatDemandZone
		);
	}
	fclose(fp);
}

void qeoSendTempState(org_muizenhol_evohome_item_Temperature_t * msg)
{
	printf("QEO_TEMP_STATE: publish 0x%x - %d (%s) - %d - %d - %d\n", msg->sensor, msg->zone, msg->zoneName, msg->tempSetZone, msg->tempSet, msg->tempMeasured);
	qeo_state_writer_write(_temp_writer, msg);
	logToFile();
}

void testRun()
{
	//		RAW 33 55 53 a9 6a a9 a6 96 a9 96 6a a9 a6 96 a9 96 6a a5 aa 5a 69 aa a5 aa aa aa 96 9a a5 56 59 35
	//		DEC 18 12 61 68 12 61 68 30 c9 03 00 06 43 ed
	//		CHECKSUM - OK
	//		ID1=126168 ID2=126168 TS=00 COMMAND=30c9, LENGTH=3
//	char test1[] = "53 a9 6a a9 a6 96 a9 96 6a a9 a6 96 a9 96 6a a5 aa 5a 69 aa a5 aa aa aa 96 9a a5 56 59 35";
//	processMsg(test1);
//
//	char test2[] = "53 A9 6A A9 A6 9A 65 6A A6 AA 9A 9A 96 55 65 A6 A5 AA 69 AA A5 AA AA AA 95 69 56 55 AA 35";
//	processMsg(test2);
//
//	char test3[] = "53 A9 6A AA 9A 9A 96 55 65 AA 9A 9A 96 55 65 A6 A5 AA 69 AA 5A AA AA AA 99 A9 9A AA A9 AA A5 6A 9A AA A6 AA 99 A9 9A AA A5 AA A5 A6 AA 9A 9A 35";
//	processMsg(test3);
//
//	char test4[] = "53 A9 6A A9 A6 96 A9 96 6A AA 9A 9A 96 55 65 A5 A9 99 AA AA A6 AA A9 AA 33 53 15 12 00 80 03 88 04 06 B1 00 00 52 02 10 00 20 04 90 51 0C 84 00 22 04 21 AB 41 40 43 20 04 8F 1E A3 0D C1 20 02 14 F6 08 81 06 20 82 04 90 18 0C A0 D6 49 01 10 E1 00 0C 86 70 50 E3 40 00 30 16 82 43 C0 22 03 50 21 1B D5 10 90 A1 14 C0 21 00 08 27 A5 60 59 60 02 8A 88 3A 10 20 00 04 64 18 8A 28 13 00 58 DD 02 00 00 84 00 00 00 00 00 00 00 00 00 00 00 59 00 00 00 00 00 00 00 D8 07 00 00 00 00 00 00 50 F9 FF FE 07 00 00 00 56 11 40 00 00 00 00 00 18 75 2C 05 D9 07 00 00 60 F9 FF FE 07 00 00 00 35 14 50 A2 CC 08 20 02 00 D9 04 70 04 64 62 A0 16 84 3A 0C 28 C0 00 03 01 40 0D 00 84 8C 44 44 08 84 00 0A 45 80 88 08 09 28 11 10 07 82 20 70 7A 10 18 08 0A 81 10 C0 4D 81 60 92 48 9B BB 80 22 91 93 04 0C 01 88 00 E0 02 19 82 51 01 00 18 93 C2 B1 02 D5 80 24 90 C8 4D 46 00 E8 10 08 10 28 02 44 02 0E 00 16 44 17 48 D2 37 00 02 04 8A 29 40 58 8C 0C C4 01 88 08 07 90 80 0C 22 15 A2 86 45 3D 80 56 40 00 89 84 0A 08 A6 40 A9 F2 45 53 01 04 80 A2 09 00 08 00 30 86 90 30 12 00 02 02 00 9D 44 13 08 00 01 08 00 08 89 80 11 16 81 31 00 10 C2 5C 00 94 A8 40 B9 01 2E 01 80 15 80 A2 49 19 C2 82 C2 AF 43 00 90 2C 06 CE 8A 90 60 10 06 00 34 9B D0 08 10 02 80 C1 25 F0 02 80 00 00 01 00 20 C0 74 02 00 40 E1 C1 22 18 42 5C 02 E9 85 2C 98 64 62 A8 30 00 00 9A 0A 00 03 80 80 00 D2 90 1E 10 49 02 64 CA 70 22 49 04 58 40 8E 40 40 07 01 00 10 C8 02 98 00 41 12 41 38 00 81 01 12 78 C0 00 40 10 1E 00 83 68 A0 10 90 80 20 60 36 08 03 2D 30 09 24 30 12 31 34 02 82 08 01 D0 13 20 90 80 88 90 01 31 80 08 00 80 1C 06 00 00 00 44 8A 0B 62 12 C0 C4 92 80 20 30 46 85 00 07 32 B8 85 A0 02 54 18 11 F8 C2 64 24 81 C0 09 01 44 00 81 08 80 98 4C 01 20 12 E4 09 28 06 26 D9 4A DA 09 83 40 F4 30 00 88 02 00 41 82 2A 23 00 60 C1 30 00 3C 40 42 16 48 46 8C 10 00 80 50 21 90 07 61 01 90 02 0F 7C 97 21 51 41 98 A0 00 B8 8E 10 14 C0 06 01 80 60 49 07 00 99 04 9E DE 82 00 84 40 40 50 08 10 AB 5D 02 40 00 C0 A0 80 88 02 E8 18 09 E4 10 80 00 80 13 1C 04 29 21 41 50 00 09 19 08 10 80 52 10 00 02 11 A2 01 3C 20 02 02 21 80 84 20 00 3C C0 10 84 04 13 04 A8 18 3D D0 A0 30 00 24 70 21 80 10 30 00 02 20 00 10 80 0C 50 82 90 1A 01 86 0A 88 89 81 35\n";
//	processBuf(test4, strlen(test4));

	//unsigned char test5[] = {0X18, 0X04, 0X46, 0XFB, 0X04, 0X46, 0XFB, 0X23, 0X09, 0X0C, 0X00, 0X05,
	//		0X14, 0X01, 0X03, 0X84, 0X02, 0X05, 0X14, 0X03, 0X03, 0X20};
	//addChecksum(test5, 22);

	//printf("\n");
	unsigned char test6[] = {0X18, 0X04, 0X46, 0XFB, 0X04, 0X46, 0XFB, 0X23, 0X09, 0X03, 0X00, 0X05,
				0X14};
		addChecksum(test6, 13);

//	unsigned char test6[] = {0X18, 0X12, 0X61, 0X68, 0X12, 0X61, 0X68, 0X30, 0XC9, 0X03, 0X00, 0X06, 0X43};
//addChecksum(test6, 13);

	//char test5[] = "53 A9 6A \n";
	//processBuf(test5, strlen(test5));

	//event_writer_close(_raw_writer);*/
	//exit(0);
}

void replay(const char * file)
{
	printf("Replay file %s\n", file);
	FILE * fp;
	char line[2048];
	char msg[2048];

	fp = fopen(file, "r");
	if (fp == NULL) {
		fprintf(stderr, "ERROR: can't open %s\n", file);
		exit(1);
	}

    while (fgets (line, sizeof(line), fp ) != NULL) { // read a line

    	if (strstr(line, "RAW:") != NULL) {
    		//found RAW line
    		memcpy(msg, line + 5, strlen(line) - 7);
    		msg[strlen(line) - 7] = '\0';
    		processMsg(msg);
			sleep(1);
//    		usleep(100000);
    		//printf("R: --%s-- \n",  msg);
    	}

    }

	fclose(fp);
	//sleep(100);
}


void readConf()
{
	FILE * fp;
	char line[256];
    char localfilename[80];
	int confId = 0;
	int const confIdZone = 1;
	int const confIdSensor = 2;
	int const confIdMasterId = 3;
	int i = 0;
	conf_zones = NULL;
	conf_zone_count = 0;
	conf_sensor_count = 0;
	masterId = 0; //default value

    sprintf(localfilename, "%s%s", getenv("HOME"), CONFIGFILE);
	fp = fopen(localfilename, "r");
	if (fp == NULL) {
        //create dummy configfile
        printf("Creating empty configfile in %s\n", localfilename);
        fp = fopen(localfilename, "w");
        if (fp == NULL) {
		    fprintf(stderr, "ERROR: can't create %s\n", localfilename);
		    exit(1);
        }
        fprintf(fp, "#This configfile is used by evo2qeo to give friendly names to zone and sensor ids. It's optional but makes the data much more user friendly to read. To get the id's just run the program with an empty configfile and you'll see them automatically appear.\n\n");
        fprintf(fp, "#Zone naming\n#Zone:0:Living\n\n");
        fprintf(fp, "#sensor id naming\n#Sensor:0xffffff:Living sensor 1\nSensor:0xffffff:Living sensor 2\n\n");
        fprintf(fp, "#masterId: Id of the evohome unit\n#Optional, but speeds up initial data. Otherwise it will be auto-detected if enough messages have arrived\n#MasterId:0xffffff\n\n");
        fclose(fp);

	    fp = fopen(localfilename, "r");
	    if (fp == NULL) {
		    fprintf(stderr, "ERROR: can't open %s\n", localfilename);
		    exit(1);
        }
	}
	int id;
	char name[256];


    while (fgets (line, sizeof(line), fp ) != NULL) { // read a line

    	//first strip off newlines and everything after a #
		for (i = 0; i < strlen(line); ++i) {
			if (line[i] == '\n' || line[i] == '\r' || line[i] == '#') {
				line[i] = '\0';
				break;
			}
		}
		//strip off trailing spaces
		for (i = (strlen(line) - 1); i >= 0; --i) {
			if (line[i] == '\t' || line[i] == ' ') {
				line[i] = '\0';
			}
			else {
				break;
			}
		}
		if (strlen(line) == 0) {
			continue;
		}

    	//printf("Conf Line: %s\n", line);
    	//process line
    	char * tok = strtok(line, ":");
    	i = 0;
    	confId = 0; //reset
    	while(tok != NULL) {
    		i++;
    		//printf("%d: %s\n", i, tok);
    		if (i == 1) {
    			if (strcmp(tok, "Zone") == 0) {
    				confId = confIdZone;
    			}
    			else if (strcmp(tok, "Sensor") == 0) {
					confId = confIdSensor;
				}
    			else if (strcmp(tok, "MasterId") == 0) {
					confId = confIdMasterId;
				}
    			else {
    				fprintf(stderr, "can't parse conf file line: %s\n", tok);
    				exit(1);
    			}
    		}
    		else {
    			if (confId == 0) {
    				fprintf(stderr, "confId not set\n");
					exit(1);
    			}
				switch (i) {
					case 2:
						if(confId == confIdSensor || confId == confIdMasterId) {
							id = strtol(tok, NULL, 16);
						}
						else {
							id = atoi(tok);
						}
						break;
					case 3:
						strcpy(name, tok);
						break;
					default:
						fprintf(stderr, "error in conf file parsing\n");
						exit(1);
				}

    		}

    		tok = strtok(NULL, ":");
    	}

    	//line processed
    	if (confId == confIdZone) {
    		conf_zone_count++;
    		conf_zones = realloc(conf_zones, conf_zone_count * sizeof(*conf_zones));
    		conf_zone_t * conf_zone = malloc(sizeof(conf_zone_t));
    		conf_zone->zone = id;
    		strcpy(conf_zone->name, name);
    		conf_zones[conf_zone_count - 1] = conf_zone;
    	}
    	else if (confId == confIdSensor) {
			conf_sensor_count++;
			conf_sensors = realloc(conf_sensors, conf_sensor_count * sizeof(*conf_sensors));
			conf_sensor_t * conf_sensor = malloc(sizeof(conf_sensor_t));
			conf_sensor->sensor = id;
			strcpy(conf_sensor->name, name);
			conf_sensors[conf_sensor_count - 1] = conf_sensor;
		}
    	else if (confId == confIdMasterId) {
    		masterId = id;
    		printf("Using MasterId 0x%x\n", masterId);
    	}
    	else {
    		fprintf(stderr, "Error in conf file parsing\n");
    		exit(1);
    	}
    }

	fclose(fp);
}

void clean_conf()
{
	int i;
	for (i = 0; i < conf_zone_count; ++i) {
		free(conf_zones[i]);
	}
	free(conf_zones);
	for (i = 0; i < conf_sensor_count; ++i) {
		free(conf_sensors[i]);
	}
	free(conf_sensors);
}

int main(int argc, const char **argv)
{
	//setbuf(stdout, NULL); //for easy debugging - valgrind does not like this
	printf("Starting...\n");
	readConf();

#ifdef USE_TELNET
	pthread_t * thread = malloc(sizeof(pthread_t));
	int res = pthread_create(thread, NULL, (void *) &start_telnet, NULL);
	if (res != 0) {
		fprintf(stderr, "Error starting telnet thread\n");
		exit(1);
	}
#endif

#ifndef TESTRUN
    char buf[128];
    const char * serialport = NULL;

	if (argc == 2) {
		serialport = argv[1];
	}
#endif

    qeo_factory_t *qeo;
    qeo = qeo_factory_create();

    _temp_writer = qeo_factory_create_state_writer(qeo, org_muizenhol_evohome_item_Temperature_type, NULL, 0);
    //_raw_writer = ...

	honeywelldecode_init();
#ifdef TESTRUN
	if (argc == 2) {
		replay(argv[1]);
	}
	else {
		testRun();
	}
#else

	open_serial(serialport);
	//initial read, clean some data
	serial_read(buf, sizeof(buf));


	//init
	serial_init_read();

	//read some data to avoid rubbish at the start
	serial_read(buf, sizeof(buf));
	sleep(1);
	serial_read(buf, sizeof(buf));

	printf("starting read loop\n");
	read_loop();
#endif


	sleep(1000); //give it some time to settle
	printf("Cleaning...\n");
//	state_writer_remove(_temp_writer, &msg1);
//	sleep(1);
	qeo_state_writer_close(_temp_writer);
	//qeo_event_writer_close(_raw_writer);
    qeo_factory_close(qeo);
	honeywelldecode_destroy();
	clean_conf();
	sleep(1); //give it some time to settle
	return 0;
}

