#include "jtl.h"

static const char* tag = "JTL";
static jtl_cmda1_t jtl_a1_data;
static jtl_cmda2_t jtl_a2_data;
static char *gotip, mac_buf[6];
static uint8_t cmd_b8[16] = { 0x55, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xB8, 0xAA, 0xAA, 0xAA, 0xAA, 0x80, 0x00, 0xAA, 0xAA };
static uint8_t prior_heater = 0;
static int json_temp = 0;
static TaskHandle_t control_countHandle = NULL;
static void control_count(void* parm);

void mqtt_send_cmd(jtl_cmda1_t jtl_a1_data, jtl_cmda2_t jtl_a2_data, uint8_t cmd_type)
{
    if (get_wifiState() == WIFI_MQTT_CONNECTED) {
        char* json = (char*)calloc(300, sizeof(char));
        tcpip_adapter_ip_info_t ipInfo;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
        gotip = ip4addr_ntoa(&ipInfo.ip);
        if (cmd_type == 0xB1) {
            sprintf(json,
                "{\"source_ip\":\"%s\",\"auth_token\":\"%s\",\"module_serial\":\"%s\",\"user_temperature\":\"%d\","
                "\"input_temperature\":\"%.1f\","
                "\"output_temperature\":\"%.1f\",\"error_code\":\"%x\",\"status\":\"%x\"}",
                gotip, auth_token_key, get_macAddress(), jtl_a1_data.user_temp, jtl_a1_data.intputwater, jtl_a1_data.outputwater,
                jtl_a1_data.errorcode, jtl_a1_data.heater_state1);
            esp_mqtt_client_publish(getClientHandle(), pub_topic_temperature, json, strlen(json), 0, 1);
            ESP_LOGI(tag, "PUBLISHED : %s", json);
            if (set_heaterprior()) {
                sprintf(json, "{\"user\":\"1\",\"temperature\":\"%d\"}", jtl_a1_data.user_temp);
                esp_mqtt_client_publish(getClientHandle(), rules_topic_temperature, json, strlen(json), 0, 0);
                ESP_LOGI(tag, "PUBLISHED : %s", json);
            } else {
                ESP_LOGE(tag, "NO PRIOR");
            }
        } else if (cmd_type == 0xB2) {
            sprintf(json,
                "{\"source_ip\":\"%s\",\"auth_token\":\"%s\",\"module_serial\":\"%02x%02x%02x%02x%02x%02x\",\"water_yield\":\"%.1f\",\"water_"
                "volume\":\"%d\",\"fan_rpm\":"
                "\"%d\"}",
                gotip, auth_token_key, mac_buf[0], mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5], jtl_a2_data.water_yield,
                jtl_a2_data.water_volume, jtl_a2_data.fan_rpm);
            // sprintf("%d",jtl_a2_data.user_temp);
            esp_mqtt_client_publish(getClientHandle(), pub_topic_water, json, strlen(json), 0, 1);
            ESP_LOGI(tag, "PUBLISHED : %s", json);
        }
        free(json);
    } else {
        ESP_LOGE(tag, "WIFI NOT CONNECT");
    }
}
bool set_heaterprior(void)
{
    if (prior_heater == 0) {
        // esp_mqtt_client_publish(
        //     getClientHandle(), control_state, "{\"control\":\"1\",\"survive\":\"600\"}", strlen("{\"control\":\"1\",\"survive\":\"600\"}"), 0, 0);
        // prior_heater = 1;
        // if (control_countHandle == NULL)
        //     xTaskCreate(control_count, "control_count", 1024 * 3, (void*)600, 0, &control_countHandle);
        return true;
    } else if (prior_heater == 1) {
        return true;
    }
    return false;
}
uint8_t get_heaterprior(uint8_t state)
{
    if (prior_heater == 0) {
        state = state | 0x00;
    } else if (prior_heater == 1) {
        state = state | 0x10;
    } else if (prior_heater == 2) {
        state = state | 0x08;
    } else {
        ESP_LOGE(tag, "prior error %d", prior_heater);
    }
    return state;
}
static void control_count(void* parm)
{
    ESP_LOGI(tag, "CONTROL CONUNT TASK START, CONTENT:\n  %d SECONDS LEFT", (int)parm);
    int survive = (int)parm;
    int i = 0;
    if (survive >= 0) {
        i = survive;
    } else {
        ESP_LOGE(tag, "CONTROL CONUNT TASK ERROR %d", i);
        goto FAIL;
    }
    while (i >= 0) {
        ESP_LOGI(tag, "CONTROL SURVIVE: %ds REMAIN", i--);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(tag, "CONTROL SURVIVE OVER");
    prior_heater = 0;
    esp_mqtt_client_publish(
        getClientHandle(), control_state, "{\"control\":\"0\",\"survive\":\"0\"}", strlen("{\"control\":\"0\",\"survive\":\"0\"}"), 0, 1);
    xQueueSend(getUartQueueHandle(), (void*)cmd_b8, pdMS_TO_TICKS(0));
FAIL:
    control_countHandle = NULL;
    vTaskDelete(NULL);
}
void judgement(int data_len, const char* event_data, int topic_len)
{
    cJSON* json_data = cJSON_Parse(event_data);
    if (topic_len == strlen(control_state)) {
        ESP_LOGI(tag, "current prior:%d", prior_heater);
        int control = atoi(cJSON_GetObjectItem(json_data, "control")->valuestring);
        int survive = atoi(cJSON_GetObjectItem(json_data, "survive")->valuestring);
        if (control != 0) {
            if (control_countHandle == NULL) {
                prior_heater = control;
                ESP_LOGI(tag, "prior change to:%d", prior_heater);
                xTaskCreate(control_count, "control_count", 1024 * 3, (void*)survive, 0, &control_countHandle);
            } else {
                ESP_LOGI(tag, "CONRTROL COUNT TASK HAD BEEN CREATED , Remove old task");
                vTaskDelete(control_countHandle);
                control_countHandle = NULL;
                xTaskCreate(control_count, "control_count", 1024 * 3, (void*)survive, 0, &control_countHandle);
            }
        } else if (control == 0 && survive == 0) {
            if (control_countHandle != NULL) {
                vTaskDelete(control_countHandle);
                control_countHandle = NULL;
                prior_heater = 0;
                ESP_LOGI(tag, "prior change to:%d", prior_heater);
            }
        }
    } else if (topic_len == strlen(rules_topic_temperature)) {
        int user = atoi(cJSON_GetObjectItem(json_data, "user")->valuestring);
        json_temp = atoi(cJSON_GetObjectItem(json_data, "temperature")->valuestring);
        cmd_b8[8] = (uint8_t)json_temp;
        ESP_LOGI(tag, "user: %d", user);
        if (user == 2) {
            ESP_LOGI(tag, "current prior:%d", prior_heater);

            xQueueSend(getUartQueueHandle(), (void*)cmd_b8, pdMS_TO_TICKS(0));
        } else {
            ESP_LOGI(tag, "user hasn't been auth");
        }
    } else {
        static uint8_t cmd_b9[16] = { 0x55, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xB9, 0xAA, 0xAA, 0xAA, 0xAA, 0x80, 0x00, 0xAA, 0xAA };
        unsigned long water = strtoul(event_data, NULL, 10);
        cmd_b9[8] = water >> 8;
        cmd_b9[9] = water;
        if (prior_heater != 1) {
            ESP_LOGI(tag, "current prior:%d", prior_heater);
            cmd_b9[12] |= 0x04;

            xQueueSend(getUartQueueHandle(), (void*)cmd_b9, pdMS_TO_TICKS(0));
        } else {
            ESP_LOGI(tag, "no prior,cancel cmd sending");
        }
    }
    cJSON_Delete(json_data);
    ESP_LOGI(tag, "free memory : %d Bytes", esp_get_free_heap_size());
}

void jtl_send_task(void* parm)
{
    uint8_t* data = (uint8_t*)malloc(BUF_SIZE);
    while (1) {
        if (xQueueReceive(xQueues.xQueue_jtl_send, data, portMAX_DELAY)) {
            ESP_LOGI(tag, "GET CMD :");
            ESP_LOG_BUFFER_HEX(tag, data, 16);
            if (data[7] == 0xA1 || data[7] == 0xA2) {
                data[7] += 0x10;
                data[12] = get_heaterprior(data[12]);
                data[14] = crc_high_first(data, 14);
                uart_write_bytes(UART_NUM_1, (const char*)data, 16);
                ESP_LOGI(tag, "send cmdACK");
                ESP_LOG_BUFFER_HEX(tag, data, 16);
                uint8_t* buf = (uint8_t*)calloc(BUF_SIZE + 1, sizeof(uint8_t));
                memcpy(buf, data, 17);
                int i = 0;
                while (i < 3) {
                    if (xQueueReceive(xQueues.xQueue_jtl_recv, data, pdMS_TO_TICKS(500))) {
                        if (data[15] == 0xAA) {
                            if (data[14] == crc_high_first(data, 14)) {
                                if (data[7] == 0xC1) {
                                    if (buf[7] == 0xB1) {
                                        jtl_a1_data.user_temp = buf[8];
                                        jtl_a1_data.now_temp = jtl_a1_data.user_temp;
                                        jtl_a1_data.outputwater = buf[9];
                                        jtl_a1_data.outputwater /= 2;
                                        jtl_a1_data.intputwater = buf[10];
                                        jtl_a1_data.intputwater /= 2;
                                        jtl_a1_data.errorcode = buf[11];
                                        jtl_a1_data.heater_state1 = buf[12];
                                        jtl_a1_data.heater_state2 = buf[13];
                                        mqtt_send_cmd(jtl_a1_data, jtl_a2_data, buf[7]);
                                    } else if (buf[7] == 0xB2) {
                                        jtl_a2_data.water_yield = buf[8];
                                        jtl_a2_data.water_yield /= 10;
                                        jtl_a2_data.water_volume = buf[9] << 8 | buf[10];
                                        jtl_a2_data.fan_rpm = buf[11] << 8 | buf[12];
                                        mqtt_send_cmd(jtl_a1_data, jtl_a2_data, buf[7]);
                                    }
                                    ESP_LOGI(tag, "A1 or A2 CMD COMPLETE");
                                    break;
                                } else {
                                    ESP_LOGE(tag, "CMD ERROR");
                                }
                            } else {
                                ESP_LOGE(tag, "CRC ERROR 2 : %x", (char)crc_high_first(data, 14));
                            }
                        } else {
                            ESP_LOGE(tag, "NOT A CMD");
                        }
                    } else {
                        uart_write_bytes(UART_NUM_1, (const char*)buf, 16);
                        ESP_LOGI(tag, "resend cmd");
                        ESP_LOG_BUFFER_HEX(tag, buf, 16);
                        i++;
                    }
                }
                free(buf);
            } else if (data[7] == 0xB8 || data[7] == 0xB9) {
                data[12] = get_heaterprior(data[12]);
                data[14] = crc_high_first(data, 14);
                uart_write_bytes(UART_NUM_1, (const char*)data, 16);
                ESP_LOGI(tag, "send cmdACK");
                ESP_LOG_BUFFER_HEX(tag, data, 16);
                uint8_t* buf = (uint8_t*)calloc(BUF_SIZE + 1, sizeof(uint8_t));
                memcpy(buf, data, 17);
                int i = 0;
                while (i < 3) {
                    if (xQueueReceive(xQueues.xQueue_jtl_recv, data, pdMS_TO_TICKS(500))) {
                        if (data[15] == 0xAA) {
                            if (data[14] == crc_high_first(data, 14)) {
                                if (data[7] == 0xA8 || data[7] == 0xA9) {
                                    if (data[7] == 0xA8) {
                                        data[7] = 0xC2;
                                        data[14] = crc_high_first(data, 14);
                                        uart_write_bytes(UART_NUM_1, (const char*)data, 16);
                                        ESP_LOGI(tag, "B8 CMD COMPLETE");
                                        break;
                                    } else if (data[7] == 0xA9) {
                                        data[7] = 0xC2;
                                        data[14] = crc_high_first(data, 14);
                                        uart_write_bytes(UART_NUM_1, (const char*)data, 16);
                                        ESP_LOGI(tag, "B9 CMD COMPLETE");
                                        break;
                                    }
                                } else {
                                    ESP_LOGE(tag, "CMD ERROR");
                                }
                            } else {
                                ESP_LOGE(tag, "CRC ERROR 3 : %x", (char)crc_high_first(data, 14));
                            }
                        } else {
                            ESP_LOGE(tag, "NOT A CMD");
                        }
                    } else {
                        uart_write_bytes(UART_NUM_1, (const char*)buf, 16);
                        ESP_LOGI(tag, "resend cmd");
                        ESP_LOG_BUFFER_HEX(tag, buf, 16);
                        i++;
                    }
                }
                free(buf);
            }
            ESP_LOGI(tag, "free memory : %d Bytes", esp_get_free_heap_size());
        }
    }
}
void jtl_recv_task(void* parm)
{
    // Configure a temporary buffer for the incoming data
    uint8_t* data = (uint8_t*)malloc(BUF_SIZE + 1);
    while (1) {
        const int len = uart_read_bytes(UART_NUM_1, (uint8_t*)data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if (len > 0) {
            data[len] = 0;
            // ESP_LOGI(tag, "JTL_UART Read %d bytes: '%s'", len, data);
            ESP_LOGI(tag, "JTL_UART Read %d bytes", len);
            if (data[15] == 0xAA) {
                if (data[14] == crc_high_first(data, 14)) {
                    if (data[7] == 0xA1 || data[7] == 0xA2) {
                        xQueueSend(xQueues.xQueue_jtl_send, (void*)data, pdMS_TO_TICKS(500));
                    } else if (data[7] == 0xA8 || data[7] == 0xA9 || data[7] == 0xC1) {
                        xQueueSend(xQueues.xQueue_jtl_recv, (void*)data, pdMS_TO_TICKS(500));
                    } else {
                        ESP_LOGE(tag, "CMD ERROR");
                    }
                } else {
                    ESP_LOGE(tag, "CRC ERROR 1 : %x", (char)crc_high_first(data, 14));
                }
            } else {
                ESP_LOGE(tag, "NOT A CMD");
            }
        }
    }
}