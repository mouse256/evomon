/**************************************************************
 ********          THIS IS A GENERATED FILE         ***********
 **************************************************************/

#ifndef QDM_EVOHOMETEMPERATURE_H_
#define QDM_EVOHOMETEMPERATURE_H_

#include <qeo/types.h>


typedef struct {
    /**
     * [Key]
     */
    int32_t sensor;
    int16_t zone;
    int16_t tempSet;
    int16_t tempSetZone;
    int16_t tempMeasured;
    int16_t tempMeasuredZone;
    int16_t heatDemandZone;
    char * zoneName;
    char * sensorName;
} org_muizenhol_evohome_item_Temperature_t;
extern const DDS_TypeSupport_meta org_muizenhol_evohome_item_Temperature_type[];


#endif /* QDM_EVOHOMETEMPERATURE_H_ */

