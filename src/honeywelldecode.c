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

#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "honeywelldecode.h"
#include "serial.h"



void decode3150(honeywellmsg * msg)
{
	//heat demand
    int j;
	if (msg->payloadsize != 2) {
		printf("Error decoding heat demand payload, size incorrect: %d\n", msg->payloadsize);
		return;
	}
	unsigned char zone = msg->payload[0];
	unsigned char demand = msg->payload[1];
	printf("heat demand: %d: %d\n", zone, demand);
	//find temp struct and send
	for (j = 0; j < tempMsgCount; j++) {
		if (tempMsg[j]->zone == zone) {
			//found temperature zone
			tempMsg[j]->heatDemandZone = demand;

			//send Qeo msg
			qeoSendTempState(tempMsg[j]);
		}
		//TODO: create struct if not yet available
	}

}

void decode30C9(honeywellmsg * msg)
{
	//zone temperature distribution
	int i;
	char tag[] = "ZONE_TEMP";
	//blocks of 3 bytes
	if (msg->payloadsize % 3 != 0) {
		printf("Error decoding zone temperature payload, size incorrect: %d\n", msg->payloadsize);
		return;
	}
	if (msg->id1 !=  msg->id2) {
		printf("%s: ERROR: got 2 different id's, don't know what to do: 0x%x 0x%x\n", tag, msg->id1, msg->id2);
	}
	if (masterId == 0) {
		return; //masterId not yet known
	}

	int j;
	if (msg->id1 == masterId) {
		//msg from main unit
		for (j = 0; j < tempMsgCount; j++) {
			//first clear out current set values. Zones can disappear if the config is altered
			tempMsg[j]->tempMeasuredZone = -1;
		}
		for (i = 0 ; i < msg->payloadsize ; i += 3) {
			unsigned char zone = msg->payload[i];
			unsigned int temp = msg->payload[i + 1] << 8 | msg->payload[i + 2];
			//this is sent for zones that use a zone temperature instead of the internal sensor.
			printf("%s: Zone msg: 0x%x: %d: %d\n", tag, msg->id1, zone, temp);

			for (j = 0; j < tempMsgCount; j++) {
				//first clear out current set values. Zones can disappear if the config is altered
				if (tempMsg[j]->zone == zone) {
					//found temperature zone
					tempMsg[j]->tempMeasuredZone = temp;
					qeoSendTempState(tempMsg[j]);
				}
			}
		}
	}
	else {
		//msg from sensor itself
		if (msg->payloadsize != 3) {
			printf("%s: ERROR: got a sensor temperature msg with a strange payload size: %d\n", tag, msg->payloadsize);
		}
		unsigned char zone = msg->payload[0];
		unsigned int temp = msg->payload[1] << 8 | msg->payload[2];

		printf("%s: Sensor msg: 0x%x: %d\n", tag, msg->id1, temp);
		//in this case the zoneId is always 0 and hence is worthless
		if (zone != 0) {
			printf("%s: ERROR: sensor reading with zone != 0: 0x%x - %d\n", tag, msg->id1, zone);
		}

		//find temp struct and send
		for (j = 0; j < tempMsgCount; j++) {
			if (tempMsg[j]->sensor == msg->id1) {
				//found temperature zone
				tempMsg[j]->tempMeasured = temp;

				//send Qeo msg
				qeoSendTempState(tempMsg[j]);
			}
			//TODO: create struct if not yet available
		}
	}
}


void decode2309(honeywellmsg * msg)
{
	char tag[] = "ZONE_SETPOINT";
	//zone setpoint setting
	int i;
	//blocks of 3 bytes
	if (msg->payloadsize % 3 != 0) {
		printf("Error decoding zone setpoint payload, size incorrect: %d\n", msg->payloadsize);
		return;
	}
	for (i = 0 ; i < msg->payloadsize ; i += 3) {
		unsigned char zone = msg->payload[i];
		unsigned int temp = msg->payload[i + 1] << 8 | msg->payload[i + 2];
		printf("%s: Setting: %d: %d\n", tag, zone, temp);


		if (masterId == 0x0) {
			//masterId not yet set
			if (msg->id1 == msg->id2) {
				//2 identical setpoint ids, this means this is the master id
				masterId = msg->id1;
				printf("***********************************************\n");
				printf("%s: MasterId: Ox%x\n", tag,  masterId);
				printf("***********************************************\n");
			}
			else {
				printf("%s: MasterId not yet known\n", tag);
				continue; //masterId not yet known, unable to continue parsing
			}
		}

		//msg->id1: valve id
		//msg->id2: set by id
		if (msg->id1 == msg->id2) {
			//setpoint from main unit
			//in this case we have to rely on the zone id
			org_muizenhol_evohome_item_Temperature_t * tempItem = NULL;
			int j;
			for (j = 0; j < tempMsgCount; j++) {
				tempItem = tempMsg[j];
				if (tempItem->zone == zone) {
					//found temperature zone
					tempItem = tempMsg[j];

					if (tempItem->tempSetZone == temp) {
						printf("%s: zone %d, sensor 0x%x, set temperature reconfirmed by main unit to %d\n", tag, zone, tempItem->sensor, temp);
					}
					else {
						printf("%s: zone %d, sensor 0x%x, set temperature changed by main unit from %d to %d\n", tag, zone, tempItem->sensor, tempItem->tempSetZone, temp);

						tempItem->tempSetZone = temp;
						//send Qeo msg
						qeoSendTempState(tempItem);
					}
				}
			}
		}
		else {
			//setpoint from local valve
			if (msg->id2 != masterId) {
				printf("%s: ERROR: id2 != masterId: 0x%x != 0x%x\n", tag, msg->id2, masterId);
				//don't know what to do in this case, ignore
			}
			else {
				//check if there is already a temperature message for these ids
				org_muizenhol_evohome_item_Temperature_t * tempItem = NULL;
				for (i = 0; i < tempMsgCount; i++) {
					if (tempMsg[i]->sensor == msg->id1) {
						//found temperature msg
						tempItem = tempMsg[i];
						if (tempMsg[i]->zone != zone) {
							printf("%s: WARNING: zone changed from %d to %d for sensor 0x%x\n", tag, tempItem->zone, zone, msg->id1);
							tempItem->zone = zone;
						}
						if (tempItem->tempSet == temp) {
							printf("%s: zone %d, sensor 0x%x, set temperature reconfirmed by sensor to %d\n", tag, zone, tempItem->sensor, temp);
						}
						else {
							printf("%s: zone %d, sensor 0x%x, set temperature changed by sensor from %d to %d\n", tag, zone, tempItem->sensor, tempItem->tempSet, temp);
							tempItem->tempSet = temp;
							//send Qeo msg
							qeoSendTempState(tempItem);
						}


					}
				}

				if (tempItem == NULL) {
					//new temperature message needs to be created
					printf("%s: zone %d, sensor 0x%x, set temperature set by sensor to %d\n", tag, zone, msg->id1, temp);
					tempMsg = (org_muizenhol_evohome_item_Temperature_t **) realloc(tempMsg, (tempMsgCount + 1) * sizeof(org_muizenhol_evohome_item_Temperature_t *));
					tempItem = malloc(sizeof(org_muizenhol_evohome_item_Temperature_t));
					tempMsg[tempMsgCount] = tempItem;

					tempItem->zone = zone;
					tempItem->sensor = msg->id1;
					tempItem->tempSet = temp;
					tempItem->tempMeasured = -1; //not yet known
					tempItem->tempSetZone = -1; //not yet known
					tempItem->tempMeasuredZone = -1; //not yet known
					tempItem->zoneName = malloc(1); //empty default, only a nullbyte
					tempItem->zoneName[0] = '\0';
					tempItem->sensorName = malloc(1); //empty default, only a nullbyte
					tempItem->sensorName[0] = '\0';

					//see if the zone name is set in the conf file
					for (i = 0; i < conf_zone_count; ++i) {
						if (conf_zones[i]->zone == zone) {
							tempItem->zoneName = realloc(tempItem->zoneName, strlen(conf_zones[i]->name) + 1);
							strcpy(tempItem->zoneName, conf_zones[i]->name);
						}
					}

					//see if the sensor name is set in the conf file
					for (i = 0; i < conf_sensor_count; ++i) {
						if (conf_sensors[i]->sensor == msg->id1) {
							tempItem->sensorName = realloc(tempItem->sensorName, strlen(conf_sensors[i]->name) + 1);
							strcpy(tempItem->sensorName, conf_sensors[i]->name);
						}
					}

					tempMsgCount++;

					//send Qeo msg
					qeoSendTempState(tempItem);
				}



			}
		}
	}

}

void decodePayload(honeywellmsg * msg)
{

	switch (msg->command)
	{
	case 0x30C9:
		//zone temperature distribution
		decode30C9(msg);
		break;
	case 0x3150:
		//heat demand
		decode3150(msg);
		break;
	case 0x2309:
		//zone setpoint setting
		decode2309(msg);
	break;
    case 0x1060: //unknown, but frequent
    case 0x1100: //unknown
    case 0x0008: //unknown
    case 0x0009: //unknown
    case 0x1F09: //unknown
    case 0x3B00: //unknown
    	printf("Can't decode payload, unparsable command: 0x%x\n", msg->command);
		break;
	default:
		printf("Can't decode payload, unknown command: 0x%x\n", msg->command);
		break;
	}
}

void decodeHoneywell(unsigned char * in, int inSize, int dirty)
{
	//decode header
    int id1_present=0;
    int id2_present=0;
    int ts_present=0;
    int i = 0; //progress indicator in the decoding array
    int next = 0;

    next = 1;
    if (inSize < next) {
    	printf("ERROR: message is too small to be valid\n");
    	return;
    }

    switch(in[0]) {
      case 0x18: //two addresses, no timestamp
        id1_present=1;
        id2_present=1;
        break;

      case 0x16: //one address and one timestamp
        id1_present=1;
        ts_present=1;
        break;

      case 0x14: //one address, no timestamp
        id1_present=1;
        break;

      default:
    	  printf("ERROR: can't parse header 0x%02x\n", in[0]);
    	  return;
    }
    i += next; //first byte (header) decoded

    honeywellmsg msg;
    //default values
    msg.id1 = 0;
    msg.id1set = 0;
    msg.id2 = 0;
    msg.id2set = 0;
    msg.timestamp = 0;
    msg.timestampset = 0;
    msg.command = 0;
    msg.payloadsize = 0;



    if(id1_present) {
    	next = 3;
		if (inSize < i + next) {
			printf("ERROR: message is too small to be valid\n");
			return;
		}
        msg.id1  = in[i] << 16 | in[i + 1] << 8 | in[i + 2];
        i += next; // 3 bytes decoded
        printf("ID1: 0x%x\n", msg.id1);
    }
    if(id2_present) {
    	next = 3;
		if (inSize < i + next) {
			printf("ERROR: message is too small to be valid\n");
			return;
		}
		msg.id2  = in[i] << 16 | in[i + 1] << 8 | in[i + 2];
		i += next; // 3 bytes decoded
		printf("ID2: 0x%x\n", msg.id2);
	}

    if (ts_present)
    {
    	next = 1;
		if (inSize < i + next) {
			printf("ERROR: message is too small to be valid\n");
			return;
		}
    	msg.timestamp = in[i];
    	i+= next; //1byte
    	printf("Timestamp: 0x%x\n", msg.timestamp);
    }

    next = 2;
	if (inSize < i + next) {
		printf("ERROR: message is too small to be valid\n");
		return;
	}
    msg.command = in[i] << 8 | in[i + 1];
    printf("Command: 0x%x\n", msg.command);
    i += next; //2bytes

    next = 1;
	if (inSize < i + next) {
		printf("ERROR: message is too small to be valid\n");
		return;
	}
    msg.payloadsize = in[i];
    i += next; // 1 byte
    printf("Payload size: 0x%x\n", msg.payloadsize);

    next = msg.payloadsize;
	if (inSize < i + next) {
		printf("ERROR: message is too small to be valid\n");
		return;
	}
    memcpy(msg.payload, in + i, msg.payloadsize);
    i += next;

    next = 1;
	if (inSize < i + next) {
		printf("ERROR: message is too small to be valid\n");
		return;
	}
    i += next; //checksum

    if (i != inSize) {
    	printf("Message has invalid size\n");
    	return;
    }

    if (!dirty) {
    	//only continue if manchester decoding was ok
    	decodePayload(&msg);
    }

}

unsigned char toManchester(unsigned char val)
{
   switch(val)
	{
	  case 0x00: return 0xAA;
	  case 0x01: return 0xA9;
	  case 0x02: return 0xA6;
	  case 0x03: return 0xA5;
	  case 0x04: return 0x9A;
	  case 0x05: return 0x99;
	  case 0x06: return 0x96;
	  case 0x07: return 0x95;
	  case 0x08: return 0x6A;
	  case 0x09: return 0x69;
	  case 0x0A: return 0x66;
	  case 0x0B: return 0x65;
	  case 0x0C: return 0x5A;
	  case 0x0D: return 0x59;
	  case 0x0E: return 0x56;
	  case 0x0F: return 0x55;
	  default:
		  printf("Manchester encode error: can't handle 0x%02x\n", val);
		  exit(1);
	}
}

void prepareToWrite(unsigned char * in, int inSize)
{
	//each byte gets 3 chars
	//3chars start
	//2chars stop
	//nullbyte
	char msg[inSize * 3 + 6];
	int i;
	memcpy(msg, "53 ", 4);
	for(i = 0; i < inSize; i++) {
		char buf[3];
		sprintf(buf, "%02X ", in[i]);
		memcpy(msg + (i * 3) + 3, buf, 3);
	}
	memcpy(msg + (inSize * 3) + 3, "35\0", 3);
	printf("Msg to write: %s\n", msg);

	serial_write(msg, inSize * 3 + 6);
}

void manchesterEncode(unsigned char * in, int inSize)
{
	unsigned char out[inSize * 2];
	int i;
	for (i = 0; i < inSize; i++) {
		out[i * 2] = toManchester(in[i] >> 4);
		out[i * 2 + 1] = toManchester(in[i] & 0x0F);
	}

//	printf("Encoded: ");
//	for (i = 0; i < inSize * 2; i++) {
//		printf("%02X ", out[i]);
//	}
//	printf("\n");

	prepareToWrite(out, inSize * 2);
	//void serial_write(char * msg, int size)


	//manchesterDecode(out, inSize * 2);
}

void addChecksum(unsigned char * in, int inSize)
{
	unsigned char out[inSize + 1];
	int i;
	unsigned char check = 0x0;
	for (i = 0; i < inSize; i++) {
		out[i] = in[i];
		check += in[i];
	}
	out[inSize] = 0xFF - check + 1;
	printf("Checksum: 0x%02X\n", out[inSize]);
	manchesterEncode(out, inSize + 1);
}

void manchesterDecode(unsigned char * in, int inSize)
{
	//manchester decode
	int i;
	unsigned char out[inSize / 2];
	int size = 0;
	int j = 0;
	int dirty = 0;

	for (i = 0; i < inSize; i++) {

		unsigned char val;
	    switch(in[i])
	    {
	      case 0xAA: val=0x00; break;
	      case 0xA9: val=0x01; break;
	      case 0xA6: val=0x02; break;
	      case 0xA5: val=0x03; break;
	      case 0x9A: val=0x04; break;
	      case 0x99: val=0x05; break;
	      case 0x96: val=0x06; break;
	      case 0x95: val=0x07; break;
	      case 0x6A: val=0x08; break;
	      case 0x69: val=0x09; break;
	      case 0x66: val=0x0A; break;
	      case 0x65: val=0x0B; break;
	      case 0x5A: val=0x0C; break;
	      case 0x59: val=0x0D; break;
	      case 0x56: val=0x0E; break;
	      case 0x55: val=0x0F; break;
	      case 0x53:
	    	  if (i != 0) {
	    		  printf("Start breaking at wrong position\n");
	    		  return;
	    	  }
	    	  continue;
	      case 0x35:
	    	  if (i != inSize - 1) {
				  printf("Stop breaking at wrong position\n");
				  return;
			  }
			  continue;
	      default:
	    	  printf("Manchester decode error: can't handle 0x%02x\n", in[i]);
	    	  //return;
	    	  dirty = 1; //continue decoding, but mark as dirty. This may be useful for debugging
	    	  val = 0;
	    	  break;
	    }
	    j++;
	    if (j % 2 != 0) {
	    	//even
	    	out[size] = val << 4;
	    }
	    else {
	    	//odd
			out[size] += val;
			size++;
	    }
	}
	//printf("decoded %d bytes into %d bytes\n", j, size);
	printf("Decoded: ");
	unsigned char check = 0x0;
	for (i = 0; i < size; i++) {
		printf("%02X ", out[i]);
		check += out[i];
	}
	printf("\n");
	if (j % 2 != 0) {
		printf("ERROR: got non-even number of bytes\n");
		return;
	}

	if (check != 0x0) {
		printf("ERROR: checksum mismatch\n");
		dirty = 1; //mark as dirty
	}
	decodeHoneywell(out, size, dirty);
}

void honeywelldecode_init()
{
	tempMsg = NULL;
	tempMsgCount = 0;
}

void honeywelldecode_destroy()
{
	int i;
	for (i = 0; i < tempMsgCount; i++) {
		free(tempMsg[i]->zoneName);
		free(tempMsg[i]->sensorName);
		free(tempMsg[i]);
	}
	free(tempMsg);
}


