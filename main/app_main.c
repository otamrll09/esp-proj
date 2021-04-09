/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
//#include "../components/esp_wifi/include/esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#define GPIO_DS18B20_0       (CONFIG_ONE_WIRE_GPIO)
#define MAX_DEVICES          (4)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds


static const char *TAG = "MQTT_EXAMPLE";

float tempera [4] = { 0 };

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

typedef struct Salvavidas
{
    OneWireBus * cara;
    int disp_loc;
    DS18B20_Info * orb[MAX_DEVICES];
};

QueueHandle_t xQueueSensorInfo;
QueueHandle_t xQueueMedBruta;

void LeituraDS18B20(void * pvParameters)
{
    
    int num_devices2;
    DS18B20_Info * bota[MAX_DEVICES] = {0};
    
    struct Salvavidas * pvParametrics;

    xQueueReceive(xQueueSensorInfo, &(pvParametrics),5000);
    num_devices2 = pvParametrics->disp_loc;
        
     for (int i = 0; i < num_devices2; ++i)
    {
        bota[i] = pvParametrics->orb[i];
    }
    // Read temperatures more efficiently by starting conversions on all devices at the same time
    int errors_count2[MAX_DEVICES] = {0};
    int sample_count2 = 0;
    if (num_devices2 > 0)
    {
        OneWireBus * owb2;
        owb2 = pvParametrics->cara;

        while (1)
        {
            ds18b20_convert_all(owb2);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(bota[0]);

            // Read the results immediately after conversion otherwise it may fail
            // (using printf before reading may take too long)
            float readings2[MAX_DEVICES] = { 0 };
            DS18B20_ERROR errors2[MAX_DEVICES] = { 0 };

            for (int i = 0; i < num_devices2; ++i)
            {
                errors2[i] = ds18b20_read_temp(bota[i], &readings2[i]);
                //tempera[i] = readings2[i];
                xQueueSend(xQueueMedBruta, (void*) &readings2[0], pdMS_TO_TICKS(5000) );
            }

            // Print results in a separate loop, after all have been read
            printf("\nTemperature readings (degrees C): sample %d\n", ++sample_count2);
            for (int i = 0; i < num_devices2; ++i)
            {
                if (errors2[i] != DS18B20_OK)
                {
                    ++errors_count2[i];
                }

                printf("  %d: %.1f    %d errors\n", i, readings2[i], errors_count2[i]);
            }
            //return readings[1];

            vTaskDelay(pdMS_TO_TICKS(100)); // wait 100ms
        }
    }
    else
    {
        printf("\nNo DS18B20 devices detected!\n");
    }

}

static void OneWireOp(void){
    // Override global log level
    esp_log_level_set("*", ESP_LOG_INFO);

    struct Salvavidas salvador;

    // To debug, use 'make menuconfig' to set default Log level to DEBUG, then uncomment:
    //esp_log_level_set("owb", ESP_LOG_DEBUG);
    //esp_log_level_set("ds18b20", ESP_LOG_DEBUG);

    // Stable readings require a brief period before communication
    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Create a 1-Wire bus, using the RMT timeslot driver
    OneWireBus * owb;
    owb_rmt_driver_info rmt_driver_info;
    owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(owb, true);  // enable CRC check for ROM code

    // Find all connected devices
    printf("Find devices:\n");
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", num_devices, rom_code_s);
        device_rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        owb_search_next(owb, &search_state, &found);
    }
    printf("Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");

    salvador.disp_loc = num_devices;

    // In this example, if a single device is present, then the ROM code is probably
    // not very interesting, so just print it out. If there are multiple devices,
    // then it may be useful to check that a specific device is present.

    // For a single device only:
    OneWireBus_ROMCode rom_code;
    owb_status status = owb_read_rom(owb, &rom_code);
    if (status == OWB_STATUS_OK)
    {
        char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
        owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
        printf("Single device %s present\n", rom_code_s);
    }
    else
    {
        printf("An error occurred reading ROM code: %d", status);
    }
    
    // Create DS18B20 devices on the 1-Wire bus
    DS18B20_Info * devices[MAX_DEVICES] = {0};
    for (int i = 0; i < num_devices; ++i)
    {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
        devices[i] = ds18b20_info;
        salvador.orb[i] = devices[i];

        if (num_devices == 1)
        {
            printf("Single device optimisations enabled\n");
            ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
        }
        else
        {
            ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check on all reads
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }

//    // Read temperatures from all sensors sequentially
//    while (1)
//    {
//        printf("\nTemperature readings (degrees C):\n");
//        for (int i = 0; i < num_devices; ++i)
//        {
//            float temp = ds18b20_get_temp(devices[i]);
//            printf("  %d: %.3f\n", i, temp);
//        }
//        vTaskDelay(1000 / portTICK_PERIOD_MS);
//    }

    // Check for parasitic-powered devices
    bool parasitic_power = false;
    ds18b20_check_for_parasite_power(owb, &parasitic_power);
    if (parasitic_power) {
        printf("Parasitic-powered devices detected");
    }

    // In parasitic-power mode, devices cannot indicate when conversions are complete,
    // so waiting for a temperature conversion must be done by waiting a prescribed duration
    owb_use_parasitic_power(owb, parasitic_power);

#ifdef CONFIG_ENABLE_STRONG_PULLUP_GPIO
    // An external pull-up circuit is used to supply extra current to OneWireBus devices
    // during temperature conversions.
    owb_use_strong_pullup_gpio(owb, CONFIG_STRONG_PULLUP_GPIO);
#endif

    salvador.cara = owb;
    struct Salvavidas *prttask = &salvador;

    

    xTaskCreate(LeituraDS18B20, "DS18B20L", 1000, (void*) prttask, 1, NULL);
    xQueueSend(xQueueSensorInfo, (void*) &salvador, pdMS_TO_TICKS(5000) );

    /*// Read temperatures more efficiently by starting conversions on all devices at the same time
    int errors_count[MAX_DEVICES] = {0};
    int sample_count = 0;
    if (num_devices > 0)
    {
        

        //while (1)
        //{
            ds18b20_convert_all(owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            // (using printf before reading may take too long)
            float readings[MAX_DEVICES] = { 0 };
            DS18B20_ERROR errors[MAX_DEVICES] = { 0 };

            for (int i = 0; i < num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(devices[i], &readings[i]);
                tempera[i] = readings[i];
            }

            // Print results in a separate loop, after all have been read
            printf("\nTemperature readings (degrees C): sample %d\n", ++sample_count);
            for (int i = 0; i < num_devices; ++i)
            {
                if (errors[i] != DS18B20_OK)
                {
                    ++errors_count[i];
                }

                printf("  %d: %.1f    %d errors\n", i, readings[i], errors_count[i]);
            }
            //return readings[1];

            vTaskDelay(pdMS_TO_TICKS(1000)); // wait 1 seconds
        //}
    }
    else
    {
        printf("\nNo DS18B20 devices detected!\n");
    }*/
}

void sendMessage(void *pvParameters)
{
    esp_mqtt_client_handle_t client = *((esp_mqtt_client_handle_t *)pvParameters);
    // create topic variable
    char topic[128];
    char medic[128];
    float brut[MAX_DEVICES] = {0};
    TickType_t last_wake_time = xTaskGetTickCount();

    // Set the topic varible to be a losant state topic "losant/DEVICE_ID/state"
    sprintf(topic, "channels/1348183/publish/I22SRI0GXR0L844Z");

    //sprintf(medic, "field1=%.3f", brut[0]);
    

    // Using FreeRTOS task management, forever loop, and send state to the topic
    for (;;)
    {
        xQueueReceive(xQueueMedBruta, &(brut),5000);
        sprintf(medic, "field1=%.3f", brut[0]);
        // You may change or update the state data that's being reported to Losant here:
        esp_mqtt_client_publish(client, topic, medic, 0, 0, 0);

        vTaskDelayUntil(&last_wake_time,pdMS_TO_TICKS(90000)); //sincronizando periodo de 90s
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    //char topic[128];
    //char medic[128];

    xTaskCreate(sendMessage, "Mqttmssg", 3000, (void*)client, 2, NULL);

    /*/ Set the topic varible to be a losant state topic "losant/DEVICE_ID/state"
    sprintf(topic, "channels/1348183/publish/I22SRI0GXR0L844Z");
    sprintf(medic, "field1=%.3f", tempera[1]);

     Using FreeRTOS task management, forever loop, and send state to the topic
    //for (;;)
    //{
        OneWireOp(); 
        sprintf(medic, "field1=%.3f", tempera[0]);
        // You may change or update the state data that's being reported to Losant here:
        esp_mqtt_client_publish(client, topic, medic, 0, 0, 0);

        //vTaskDelay(pdMS_TO_TICKS(90000)); // wait 90 seconds
    //}*/
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */

    xQueueSensorInfo = xQueueCreate(10, sizeof(struct Salvavidas));
    xQueueMedBruta = xQueueCreate(10, sizeof(int32_t));

    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();

    OneWireOp();

    vTaskStartScheduler();

    //while (1)
    //{
    //    mqtt_app_start();
    //    vTaskDelay(pdMS_TO_TICKS(90000));
    //}
    

    
    //sendMessage();
}
