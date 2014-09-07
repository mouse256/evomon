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

#ifndef HONEYWELLDECODE_H
#define HONEYWELLDECODE_H

#include "main.h"
#include "evohomeTemperature.h"

org_muizenhol_evohome_item_Temperature_t ** tempMsg; //list of temperature messages
int tempMsgCount;

typedef struct
{
	unsigned int id1;
    unsigned char id1set;
	unsigned int id2;
	unsigned char id2set;
	unsigned char timestamp;
	unsigned char timestampset;
	unsigned int command;
	unsigned char payloadsize;
	unsigned char payload[256];

} honeywellmsg;

void manchesterDecode(unsigned char * in, int inSize);
void manchesterEncode(unsigned char * in, int inSize);
void addChecksum(unsigned char * in, int inSize);
void honeywelldecode_init();
void honeywelldecode_destroy();

int masterId;

#endif
