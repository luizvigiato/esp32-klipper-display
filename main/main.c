#include "esp_log.h"
#include "nvs_flash.h"
#include "display_manager.h"
#include "moonraker_client.h"
#include "wifi_manager.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (display_manager_init() != ESP_OK) {
        ESP_LOGW(TAG, "Display OLED indisponivel, seguindo sem display.");
    }

    if (wifi_manager_init_sta() != ESP_OK) {
        ESP_LOGE(TAG, "Inicializacao de Wi-Fi falhou.");
        display_manager_show_error("Wi-Fi", "Falha conexao");
        return;
    }

    moonraker_client_start();
}
