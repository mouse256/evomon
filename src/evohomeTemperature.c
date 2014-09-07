/**************************************************************
 ********          THIS IS A GENERATED FILE         ***********
 **************************************************************/

#include "evohomeTemperature.h"

const DDS_TypeSupport_meta org_muizenhol_evohome_item_Temperature_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "org.muizenhol.evohome.item.Temperature", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 9, .size = sizeof(org_muizenhol_evohome_item_Temperature_t) },  
    { .tc = CDR_TYPECODE_LONG, .name = "sensor", .flags = TSMFLAG_KEY, .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, sensor) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "zone", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, zone) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "tempSet", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, tempSet) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "tempSetZone", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, tempSetZone) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "tempMeasured", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, tempMeasured) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "tempMeasuredZone", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, tempMeasuredZone) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "heatDemandZone", .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, heatDemandZone) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "zoneName", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, zoneName), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "sensorName", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(org_muizenhol_evohome_item_Temperature_t, sensorName), .size = 0 },  
};
