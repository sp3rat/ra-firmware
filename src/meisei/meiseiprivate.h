
#ifndef __MEISEIPRIVATE_H
#define __MEISEIPRIVATE_H


#include <math.h>
#include <stdlib.h>
#if !defined(M_PI)
#  define M_PI 3.14159265358979323846
#endif
#include <string.h>
#include <stdio.h>

#include "lpclib.h"
#include "meisei.h"
#include "gps.h"


typedef uint8_t MEISEI_RawData[48];


typedef union {
    MEISEI_RawData rawData;
    uint64_t fields[6];
} MEISEI_Packet;



typedef struct {
    ECEF_Coordinate observerECEF;
    LLA_Coordinate observerLLA;
} MEISEI_CookedGps;


/* Data that needs to be stored for every instance. */
typedef struct _MEISEI_InstanceData {
    struct _MEISEI_InstanceData *next;
    uint32_t id;
    float rxFrequencyMHz;
    float rxOffset;

    uint32_t lastUpdated;

    uint16_t frameCounter;

    /* Raw data before processing */
    MEISEI_Packet configPacketEven;             /* Even frame number */
    MEISEI_Packet configPacketOdd;              /* Odd frame number */
    MEISEI_Packet gpsPacketEven;                /* Even frame number */
    MEISEI_Packet gpsPacketOdd;                 /* Odd frame number */
    uint16_t frameCounterEven;

    MEISEI_CookedGps gps;
} MEISEI_InstanceData;



LPCLIB_Result _MEISEI_processConfigFrame (
        MEISEI_Packet *packet,
        MEISEI_InstanceData **instancePointer,
        float rxFrequencyHz);
LPCLIB_Result _MEISEI_processGpsFrame (
        MEISEI_InstanceData *instance);
uint16_t _MEISEI_getPayloadHalfWord (const uint64_t *fields, int index);

/* Iterate through instances */
bool _MEISEI_iterateInstance (MEISEI_InstanceData **instance);

/* Remove an instance from the chain */
void _MEISEI_deleteInstance (MEISEI_InstanceData *instance);

#endif