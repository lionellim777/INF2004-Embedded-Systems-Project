/* Emits one representative payload per MQTT topic from the REAL encoders, as "topic hex" lines. */
#include "binary_protocol.h"
#include "motion_record.h"
#include "fault_flags.h"
#include <stdio.h>
static void emit(const char *topic, const uint8_t *b, size_t n){printf("%s ",topic);for(size_t i=0;i<n;i++)printf("%02x",b[i]);puts("");}
int main(void){
    uint8_t b[32];
    motion_record_t m={.captured_ms=123456,.left_counts=14000,.right_counts=13950,.left_mm_s=120,.right_mm_s=118,
        .kind=7,.phase=1,.fault=0,.stop_reason=0,.valid=true,.stop_latched=false};
    if(motion_record_encode(&m,b,MOTION_RECORD_BYTES))emit("motion",b,MOTION_RECORD_BYTES);
    uint16_t raw[3]={310,2480,190};
    if(line_record_encode(123456,raw,2,true,b))emit("line",b,LINE_RECORD_BYTES);
    if(barcode_record_encode(123456,'A',true,false,b))emit("barcode",b,BARCODE_RECORD_BYTES);
    if(status_record_encode(123456,0x12345678u,31,0,4,b))emit("status",b,STATUS_RECORD_BYTES);
    terrain_reading_t t={.ms=123456,.roll_decideg=-37,.humps=2,.last_height_dmm=96,.highest_height_dmm=104,
        .motion_state=2,.tilt_alerts=0,.valid=true,.calibrated=true,.tilt_active=false};
    if(terrain_record_encode(&t,b))emit("terrain",b,TERRAIN_RECORD_BYTES);
    if(hump_event_record_encode(123456,2,104,0,b))emit("hump_event",b,HUMP_EVENT_BYTES);
    obstacle_reading_t o={.ms=123456,.range_mm=310,.phase=6,.servo_deg=90,.detected=1,.completed=0,.active=true,.range_valid=true};
    if(obstacle_record_encode(&o,b))emit("obstacle",b,OBSTACLE_RECORD_BYTES);
    if(track_status_record_encode("CENTER_LINE_NOT_REACQUIRED",FAULT_IMU,FAULT_IMU|FAULT_WIFI_LOSS,b))emit("track_status",b,TRACK_STATUS_RECORD_BYTES);
    return 0;}
