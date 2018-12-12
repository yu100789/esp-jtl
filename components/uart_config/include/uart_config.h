#ifndef __UART_CONFIG_H__
#define __UART_CONFIG_H__

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "globaldefines.h"
#include "sdkconfig.h"
#include "soc/uart_struct.h"
#include "wifi_mqtt_config.h"
#include <stdio.h>
#define TXD (GPIO_NUM_4)
#define RXD (GPIO_NUM_5)
#define BUF_SIZE (1024)

void uart_jtl_config(void);
QueueHandle_t getUartQueueHandle(void);
uint8_t crc_high_first(uint8_t* ptr, int len);
#endif