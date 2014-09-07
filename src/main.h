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

#ifndef EVO2QEO_MAIN_H
#define EVO2QEO_MAIN_H

#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <qeo/api.h>
#include "evohomeTemperature.h"

typedef struct {
	int zone;
	char name[128];
} conf_zone_t;
conf_zone_t ** conf_zones;
int conf_zone_count;

typedef struct {
	int sensor;
	char name[128];
} conf_sensor_t;
conf_sensor_t ** conf_sensors;
int conf_sensor_count;

/*
typedef struct {
    char *message;
} chat_msg_t;

static const qeo_typedef_t _tsm_chat_msg[] =
{
  { .tc = QEO_TYPECODE_STRUCT, .name = "org.muizenhol.evohome.item.RawMessage2", .size = sizeof(chat_msg_t), .nelem = 1 },
  { .tc = QEO_TYPECODE_STRING, .name = "msg", .size = 0, .offset = offsetof(chat_msg_t, message) }
};
*/

void qeoSendTempState(org_muizenhol_evohome_item_Temperature_t * msg);
float printTemp(int temp);
void processMsg(char * msg);

#endif

