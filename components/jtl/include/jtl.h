#ifndef __JTL_H__
#define __JTL_H__

#include "uart_config.h"
#include "wifi_mqtt_config.h"

typedef struct xQueue {
    QueueHandle_t xQueue_jtl_send;
    QueueHandle_t xQueue_jtl_recv;
} xQueue_t;

typedef struct jtl_cmda1 {
    uint8_t user_temp;
    uint8_t now_temp;
    float outputwater;
    float intputwater;
    uint8_t errorcode;
    uint8_t heater_state1;
    uint8_t heater_state2;
} jtl_cmda1_t;

typedef struct jtl_cmda2 {
    float water_yield;
    uint16_t water_volume;
    uint16_t fan_rpm;
} jtl_cmda2_t;

xQueue_t xQueues;

void jtl_recv_task(void* parm);
void jtl_send_task(void* parm);
void mqtt_send_cmd(jtl_cmda1_t jtl_a1_data, jtl_cmda2_t jtl_a2_data, uint8_t cmd_type);
void judgement(int data_len, const char* event_data, int topic_len);
bool set_heaterprior(void);
uint8_t get_heaterprior(uint8_t state);
#endif