
#include "lpclib.h"
#include "app.h"
#include "meisei.h"
#include "meiseiprivate.h"


#define MEISEI_MAX_SONDES         4


/* Points to list of DFM instance structures */
static MEISEI_InstanceData *instanceList;


/* Get a new instance data structure for a new sonde */
static MEISEI_InstanceData *_MEISEI_getInstanceDataStructure (float frequencyMHz)
{
    MEISEI_InstanceData *p;
    MEISEI_InstanceData *instance;

    /* Check if we already have the calibration data. Count the number of sondes
     * while traversing the list.
     */
    int numSondes = 0;
    p = instanceList;
    while (p) {
        if (p->rxFrequencyMHz == frequencyMHz) {
            /* Found it! */
            return p;
        }

        ++numSondes;
        p = p->next;
    }

    /* If we have reached the maximum number of sondes that we want to track in parallel,
     * do a garbage collection now: Identify the least recently used entry and reuse it.
     */
    if (numSondes >= MEISEI_MAX_SONDES) {
        uint32_t oldest = (uint32_t)-1;

        p = instanceList;
        instance = instanceList;
        while (p) {
            if (p->lastUpdated < oldest) {
                oldest = p->lastUpdated;
                instance = p;
            }

            p = p->next;
        }
    }
    else {
        /* We need a new calibration structure */
        instance = (MEISEI_InstanceData *)calloc(1, sizeof(MEISEI_InstanceData));
    }

    if (instance) {
        /* Prepare structure */
        instance->id = SONDE_getNewID(sonde);
        instance->rxFrequencyMHz = frequencyMHz;
instance->model = MEISEI_MODEL_IMS100; //TODO
        instance->metro.temperature = NAN;
        instance->metro.cpuTemperature = NAN;
        instance->gps.observerLLA.lat = NAN;
        instance->gps.observerLLA.lon = NAN;
        instance->gps.observerLLA.alt = NAN;

        /* Insert into list */
        p = instanceList;
        if (!p) {
            instanceList = instance;
        }
        else {
            while (p) {
                if (!p->next) {
                    p->next = instance;
                    break;
                }

                p = p->next;
            }
        }
    }

    return instance;
}



/* Process the config/calib block. */
LPCLIB_Result _MEISEI_processConfigFrame (
        MEISEI_Packet *packet,
        MEISEI_InstanceData **instancePointer,
        float rxFrequencyHz)
{
    LPCLIB_Result result = LPCLIB_SUCCESS;
    float f;

    /* Valid pointer to take the output value required */
    if (!instancePointer) {
        return LPCLIB_ILLEGAL_PARAMETER;
    }

    /* Allocate new calib space if new sonde! */
    MEISEI_InstanceData *instance = _MEISEI_getInstanceDataStructure(rxFrequencyHz / 1e6f);
    *instancePointer = instance;

    if (!instance) {
        return LPCLIB_ERROR;
    }

    /* Set time marker to be able to identify old records */
    instance->lastUpdated = os_time;

    /* Read frame number to determine where to store the packet */
    instance->frameCounter = _MEISEI_getPayloadHalfWord(packet->fields, 0);
    if ((instance->frameCounter % 2) == 0) {
        instance->configPacketEven = *packet;
    }
    else {
        instance->configPacketOdd = *packet;
    }

    /* Store calibration data */
    uint16_t fragment = instance->frameCounter % 64;
    uint32_t u32configRaw = 
        (_MEISEI_getPayloadHalfWord(packet->fields, 3) << 16)
        | _MEISEI_getPayloadHalfWord(packet->fields, 2);
    instance->config[fragment] = *((float *)&u32configRaw);
    instance->configValidFlags |= (1ull << fragment);

    /* Cook some other values */
    instance->rxFrequencyMHz = rxFrequencyHz / 1e6f;
    f = 0;
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_SONDE1)) {
        f = instance->config[0]; //TODO
    }
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_SONDE2)) {
        f = instance->config[16]; //TODO
    }
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_SONDE3)) {
        f = instance->config[32]; //TODO
    }
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_SONDE4)) {
        f = instance->config[48]; //TODO
    }
    if (f != 0) {
        instance->serialSonde = lrintf(f);
    }
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_SENSOR_BOOM)) {
        instance->serialSensorBoom = lrintf(instance->config[4]); //TODO
    }
    if (_MEISEI_checkValidCalibration(instance, CALIB_SERIAL_PCB)) {
        instance->serialPcb = lrintf(instance->config[2]); //TODO
    }

    return result;
}


/* Check if the calibration block contains valid data for a given purpose */
bool _MEISEI_checkValidCalibration(MEISEI_InstanceData *instance, uint64_t purpose)
{
    if (!instance) {
        return false;
    }

    return (instance->configValidFlags & purpose) == purpose;
}


/* Iterate through instances */
bool _MEISEI_iterateInstance (MEISEI_InstanceData **instance)
{
    bool result = false;

    if (instance) {
        if (*instance == NULL) {
            if (instanceList) {
                *instance = instanceList;
                result = true;
            }
        }
        else {
            *instance = (*instance)->next;
            if (*instance) {
                result = true;
            }
        }
    }

    return result;
}



/* Remove an instance from the chain */
void _MEISEI_deleteInstance (MEISEI_InstanceData *instance)
{
    if ((instance == NULL) || (instanceList == NULL)) {
        /* Nothing to do */
        return;
    }

    MEISEI_InstanceData **parent = &instanceList;
    MEISEI_InstanceData *p = NULL;
    while (_MEISEI_iterateInstance(&p)) {
        if (p == instance) {                /* Found */
            *parent = p->next;              /* Remove from chain */
            free(instance);                 /* Free allocated memory */
            break;
        }

        parent = &p->next;
    }
}



