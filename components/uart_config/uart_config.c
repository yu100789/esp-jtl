#include "uart_config.h"

static const char* tag = "uart";

void uart_jtl_config(void)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = { .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD, RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    xQueues.xQueue_jtl_send = xQueueCreate(5, BUF_SIZE);
    xQueues.xQueue_jtl_recv = xQueueCreate(5, BUF_SIZE);
    xTaskCreate(jtl_recv_task, "jtl_recv_task", 1024 * 8, NULL, 4, NULL);
    xTaskCreate(jtl_send_task, "jtl_send_task", 1024 * 8, NULL, 5, NULL);
}
QueueHandle_t getUartQueueHandle(void) { return xQueues.xQueue_jtl_send; }
uint8_t crc_high_first(uint8_t* ptr, int len)
{
    uint8_t i;
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *ptr++;
        for (i = 8; i > 0; --i) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x13;
            else
                crc = (crc << 1);
        }
    }
    return (crc);
}