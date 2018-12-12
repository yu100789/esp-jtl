#include "app_main.h"

static const char* tag = "app";

void app_main()
{
    uart_jtl_config();
    wifi_init();
    vTaskDelete(NULL);
}