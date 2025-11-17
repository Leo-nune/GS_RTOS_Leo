#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#define MAX_SSID_LEN 32
#define SAFE_NETWORKS_COUNT 5

typedef struct {
    char ssid[MAX_SSID_LEN];
    bool is_safe;
} wifi_message_t;

// Filas
static QueueHandle_t wifiQueue12; // Task1 -> Task2
static QueueHandle_t wifiQueue23; // Task2 -> Task3

// Semáforos
static SemaphoreHandle_t semTask2;
static SemaphoreHandle_t semTask3;

// Redes seguras
static const char *safe_networks[SAFE_NETWORKS_COUNT] = {
   "Home_Leonardo", "Rede_Segura1", "Rede_Segura2", "Rede_Segura3", "Rede_Segura4" 
};

// Simulação de rede
void get_connected_network(char *ssid_out)
{
    static int idx = 0;
    const char *mock[] = {"Rede_Segura1", "Invasor", "Home_Leonardo", "Desconhecida"};
    strcpy(ssid_out, mock[idx]);
    idx = (idx + 1) % 4;
}

//TASK 1
void WiFiMonitorTask(void *pv)
{
    wifi_message_t msg;
    char ssid[MAX_SSID_LEN];

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI("Task1", "Registered in Task WDT");

    for (;;) {
        get_connected_network(ssid);
        printf("[Task1] Rede atual detectada: %s\n", ssid);

        strcpy(msg.ssid, ssid);
        msg.is_safe = false;

        if (xQueueSend(wifiQueue12, &msg, pdMS_TO_TICKS(500)) != pdPASS) {
            ESP_LOGW("Task1", "Fila cheia, mensagem descartada");
        } else {
            xSemaphoreGive(semTask2); // libera Task2
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//TASK 2
void SecurityCheckTask(void *pv)
{
    wifi_message_t msg;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI("Task2", "Registered in Task WDT");

    for (;;) {
        if (xSemaphoreTake(semTask2, pdMS_TO_TICKS(1000)) == pdTRUE) {

            if (xQueueReceive(wifiQueue12, &msg, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGE("Task2", "Nenhuma mensagem recebida! Resetando fila...");
                xQueueReset(wifiQueue12);
                continue; 
            }

            printf("[Task2] Analisando rede: %s\n", msg.ssid);

            msg.is_safe = false;
            for (int i = 0; i < SAFE_NETWORKS_COUNT; i++) {
                if (strcmp(msg.ssid, safe_networks[i]) == 0) {
                    msg.is_safe = true;
                    break;
                }
            }

            if (xQueueSend(wifiQueue23, &msg, pdMS_TO_TICKS(500)) != pdPASS) {
                ESP_LOGW("Task2", "Fila Task2->Task3 cheia, mensagem descartada");
            } else {
                xSemaphoreGive(semTask3); 
            }
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//TASK 3
void AlertTask(void *pv)
{
    wifi_message_t msg;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI("Task3", "Registered in Task WDT");

    for (;;) {
        if (xSemaphoreTake(semTask3, pdMS_TO_TICKS(2000)) == pdTRUE) {
            if (xQueueReceive(wifiQueue23, &msg, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGE("Task3", "Nenhuma mensagem recebida! Resetando fila...");
                xQueueReset(wifiQueue23);
                continue;
            }

            if (msg.is_safe)
                printf("[Task3] Conectado a rede segura: %s\n", msg.ssid);
            else
                printf("[Task3] ALERTA! Rede NÃO autorizada: %s\n", msg.ssid);
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    printf("\n--- Sistema Iniciado com WDT + Timeout + Recuperação ---\n");

    // Reconfigura watchdog
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 6000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_cfg);

    wifiQueue12 = xQueueCreate(1, sizeof(wifi_message_t));
    wifiQueue23 = xQueueCreate(1, sizeof(wifi_message_t));
    if (!wifiQueue12 || !wifiQueue23) {
        ESP_LOGE("MAIN", "Falha ao criar filas");
        esp_restart();
    }

    semTask2 = xSemaphoreCreateBinary();
    semTask3 = xSemaphoreCreateBinary();
    if (!semTask2 || !semTask3) {
        ESP_LOGE("MAIN", "Falha ao criar semáforos");
        esp_restart();
    }

    xTaskCreate(WiFiMonitorTask, "Task1", 4096, NULL, 3, NULL);
    xTaskCreate(SecurityCheckTask, "Task2", 4096, NULL, 2, NULL);
    xTaskCreate(AlertTask, "Task3", 4096, NULL, 1, NULL);

    ESP_LOGI("MAIN", "Tasks criadas, filas e semáforos prontos");
}
 
