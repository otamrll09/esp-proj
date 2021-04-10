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

struct StSensor
{
    OneWireBus OwBConfig;
    int disp_loc;
    DS18B20_Info * orb[MAX_DEVICES];
} TskLT;

//static struct StSensor *SaveData;

QueueHandle_t xQueueBIOTSK;
QueueHandle_t xQueueSensorInfo;
QueueHandle_t xQueueMedBruta;

static void LeituraDS18B20(void * pvParameters)
{
    while (1)
    {
            // Override global log level
        esp_log_level_set("*", ESP_LOG_INFO);

        // To debug, use 'make menuconfig' to set default Log level to DEBUG, then uncomment:
        //esp_log_level_set("owb", ESP_LOG_DEBUG);
        //esp_log_level_set("ds18b20", ESP_LOG_DEBUG);

        // Stable readings require a brief period before communication
        vTaskDelay(pdMS_TO_TICKS(500));

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

        // In this example, if a single device is present, then the ROM code is probably
        // not very interesting, so just print it out. If there are multiple devices,
        // then it may be useful to check that a specific device is present.

        if (num_devices == 1)
        {
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
        }
        else
        {
            // Search for a known ROM code (LSB first):
            // For example: 0x1502162ca5b2ee28
            OneWireBus_ROMCode known_device = {
                .fields.family = { 0x28 },
                .fields.serial_number = { 0xee, 0xb2, 0xa5, 0x2c, 0x16, 0x02 },
                .fields.crc = { 0x15 },
            };
            char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
            owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
            bool is_present = false;

            owb_status search_status = owb_verify_rom(owb, known_device, &is_present);
            if (search_status == OWB_STATUS_OK)
            {
                printf("Device %s is %s\n", rom_code_s, is_present ? "present" : "not present");
            }
            else
            {
                printf("An error occurred searching for known device: %d", search_status);
            }
        }

        // Create DS18B20 devices on the 1-Wire bus
        DS18B20_Info * devices[MAX_DEVICES] = {0};
        for (int i = 0; i < num_devices; ++i)
        {
            DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
            devices[i] = ds18b20_info;

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
        /*while (1)
        {
               printf("\nTemperature readings (degrees C):\n");
            for (int i = 0; i < num_devices; ++i)
            {
                float temp = ds18b20_convert(devices[i]);
                printf("  %d: %.3f\n", i, temp);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }*/

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

        // Read temperatures more efficiently by starting conversions on all devices at the same time
        int errors_count[MAX_DEVICES] = {0};
        int sample_count = 0;
        //TickType_t last_time;
        if (num_devices > 0)
        {
            //TickType_t last_wake_time = xTaskGetTickCount();
            
            while (1)
            {
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
                }

                // Print results in a separate loop, after all have been read
                printf("\nTemperature readings (degrees C): sample %d\n", ++sample_count);
                for (int i = 0; i < num_devices; ++i)
                {
                    if (errors[i] != DS18B20_OK)
                    {
                        ++errors_count[i];
                    }
                    //last_time = xTaskGetTickCount();

                    printf("  %d: %.1f    %d errors\n", i, readings[i], errors_count[i]);
                    printf("[APP] Free memory: %d bytes  Last Tick: %d \n", esp_get_free_heap_size(),xTaskGetTickCount());
                    xQueueSendToFront(xQueueMedBruta, (void*) &readings[0], pdMS_TO_TICKS(105000) );
                    //xQueueOverwrite(xQueueMedBruta, (void*) &readings[0]);
                }

                vTaskDelay(pdMS_TO_TICKS(10000)); //10seg
            }
        }
        else
        {
            printf("\nNo DS18B20 devices detected!\n");
        }
        //printf("[APP] Free memory: %d bytes", esp_get_free_heap_size());
        // clean up dynamically allocated data
        for (int i = 0; i < num_devices; ++i)
        {
            ds18b20_free(&devices[i]);
        }
        owb_uninitialize(owb);

        printf("Restarting Task.\n");
    }
    

    //fflush(stdout);
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    //esp_restart();
}

void sendMessage(void *pvParameters)
{
    printf("\nTASK MQTT CONFIG..\n");
    esp_mqtt_client_handle_t client3;
    xQueueReceive(xQueueBIOTSK, &client3, 1000);
    
    // create topic variable
    char topic[128];
    char medic[128];
    float brut[MAX_DEVICES] = {0};
    TickType_t last_wake_time = xTaskGetTickCount();

    printf("teste");
    
    // Set the topic varible to be a losant state topic "losant/DEVICE_ID/state"
    sprintf(topic, "channels/1348183/publish/I22SRI0GXR0L844Z");
    //sprintf(medic, "field1=%.3f", brut[0]);
    // Using FreeRTOS task management, forever loop, and send state to the topic
    for (;;)
    {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(90000)); //sincronizando periodo de 90s
        xQueueReceive(xQueueMedBruta, &(brut),105000);
        sprintf(medic, "field1=%.3f", brut[0]);
        // You may change or update the state data that's being reported to Losant here:
        esp_mqtt_client_publish(client3, topic, medic, 0, 0, 0);       
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
    printf("Mqtt Marq");
    xQueueBIOTSK = xQueueCreate(10, sizeof(client));
    if(xQueueBIOTSK == 0){
        for(;;){printf("\nERROR QUEUE BIOTSK CREATE\n");}
    }
   
    xQueueSend(xQueueBIOTSK,(void *) &client, pdMS_TO_TICKS(5000) );
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
    printf("\nWIFI START\n");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    printf("\nWIFI PASS\n");

    xQueueSensorInfo = xQueueCreate(10, 2*sizeof(struct StSensor));
    if(xQueueSensorInfo == 0){
        for(;;){printf("\nERROR QUEUE SENSORINFO CREATE\n");}
    }    
    xQueueMedBruta = xQueueCreate(10, 2*sizeof(int32_t));
    if(xQueueMedBruta == 0){
        for(;;){printf("\nERROR QUEUE MedBruta CREATE\n");}   
    }
    /*xQueueBIOTSK = xQueueCreate(10, 2*sizeof(esp_mqtt_client_handle_t));
    if(xQueueBIOTSK == 0){
        for(;;){printf("\nERROR QUEUE BIOTSK CREATE\n");}
    }*/
    printf("\nQUEUE PASS\n");    
    

    //OneWireOp();
    printf("\nOWB PASS\n");   
    mqtt_app_start();
    printf("\nMQTTE PASS\n");  


    printf("\nTASK CONFIG..\n");
    xTaskCreate(LeituraDS18B20, "DS18B20L", 16000, NULL, 1, NULL);
    xTaskCreate(sendMessage, "Mqttmssg", 3000, NULL, 2, NULL);
    //vTaskStartScheduler();
    printf("\nTASK PASS\n");
    
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100)); //100 ms
    }
      
    //sendMessage();
}
