#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "display_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moonraker_client.h"
#include "wifi_manager.h"

static const char *TAG = "moonraker";

#define MOONRAKER_QUERY_PATH "/printer/objects/query?toolhead&extruder&heater_bed&print_stats&display_status"
#define MOONRAKER_MAX_RESPONSE_LEN 4096

typedef struct {
    char *data;
    size_t len;
    bool overflow;
} http_response_buffer_t;

typedef struct {
    char message[96];
    double progress_percent;
    double extruder_temp;
    double bed_temp;
} moonraker_status_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buffer_t *buffer = (http_response_buffer_t *)evt->user_data;

    if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0 || buffer == NULL) {
        return ESP_OK;
    }

    if (buffer->overflow) {
        return ESP_OK;
    }

    size_t new_len = buffer->len + (size_t)evt->data_len;
    if (new_len >= MOONRAKER_MAX_RESPONSE_LEN) {
        buffer->overflow = true;
        return ESP_OK;
    }

    char *new_data = realloc(buffer->data, new_len + 1);
    if (new_data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    buffer->data = new_data;
    memcpy(buffer->data + buffer->len, evt->data, evt->data_len);
    buffer->len = new_len;
    buffer->data[buffer->len] = '\0';
    return ESP_OK;
}

static const char *find_in_range(const char *start, const char *end_exclusive, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || start == NULL || end_exclusive == NULL || start >= end_exclusive) {
        return NULL;
    }

    for (const char *p = start; p + needle_len <= end_exclusive; p++) {
        if (memcmp(p, needle, needle_len) == 0) {
            return p;
        }
    }
    return NULL;
}

static const char *skip_ws(const char *p, const char *end_exclusive)
{
    while (p < end_exclusive && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

static bool json_find_field_value_start(const char *start, const char *end_exclusive,
                                        const char *field_name, const char **value_start)
{
    char pattern[48];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);
    if (n <= 0 || n >= (int)sizeof(pattern)) {
        return false;
    }

    const char *field = find_in_range(start, end_exclusive, pattern);
    if (field == NULL) {
        return false;
    }

    const char *p = field + strlen(pattern);
    p = skip_ws(p, end_exclusive);
    if (p >= end_exclusive || *p != ':') {
        return false;
    }

    p++;
    p = skip_ws(p, end_exclusive);
    if (p >= end_exclusive) {
        return false;
    }

    *value_start = p;
    return true;
}

static bool json_find_object_bounds(const char *json, const char *object_name,
                                    const char **obj_start, const char **obj_end)
{
    char pattern[48];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\"", object_name);
    if (n <= 0 || n >= (int)sizeof(pattern)) {
        return false;
    }

    const char *end = json + strlen(json);
    const char *search = json;
    while (search < end) {
        const char *field = find_in_range(search, end, pattern);
        if (field == NULL) {
            return false;
        }

        const char *p = field + strlen(pattern);
        p = skip_ws(p, end);
        if (p >= end || *p != ':') {
            search = field + 1;
            continue;
        }

        p++;
        p = skip_ws(p, end);
        if (p >= end || *p != '{') {
            search = field + 1;
            continue;
        }

        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (const char *q = p; *q != '\0'; q++) {
            char c = *q;

            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }

            if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    *obj_start = p;
                    *obj_end = q;
                    return true;
                }
            }
        }

        search = field + 1;
    }

    return false;
}

static bool json_get_number_in_object(const char *obj_start, const char *obj_end,
                                      const char *field_name, double *value)
{
    const char *num_start = NULL;
    if (!json_find_field_value_start(obj_start, obj_end + 1, field_name, &num_start)) {
        return false;
    }

    char *parse_end = NULL;
    double parsed = strtod(num_start, &parse_end);
    if (parse_end == num_start || parse_end > obj_end + 1) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool json_get_string_in_object(const char *obj_start, const char *obj_end,
                                      const char *field_name, char *out, size_t out_len)
{
    const char *value_start = NULL;
    if (!json_find_field_value_start(obj_start, obj_end + 1, field_name, &value_start)) {
        return false;
    }

    if (*value_start == 'n') {
        if ((obj_end + 1 - value_start) >= 4 && memcmp(value_start, "null", 4) == 0) {
            out[0] = '\0';
            return true;
        }
        return false;
    }

    if (*value_start != '"') {
        return false;
    }

    value_start++;
    size_t i = 0;
    bool escaped = false;
    for (const char *p = value_start; p < obj_end; p++) {
        if (escaped) {
            escaped = false;
            if (i + 1 < out_len) {
                out[i++] = *p;
            }
            continue;
        }

        if (*p == '\\') {
            escaped = true;
            continue;
        }

        if (*p == '"') {
            out[i] = '\0';
            return true;
        }

        if (i + 1 < out_len) {
            out[i++] = *p;
        }
    }

    return false;
}

static bool parse_moonraker_status(const char *json, moonraker_status_t *status)
{
    const char *display_start;
    const char *display_end;
    const char *extruder_start;
    const char *extruder_end;
    const char *bed_start;
    const char *bed_end;
    double progress = 0.0;

    if (!json_find_object_bounds(json, "display_status", &display_start, &display_end)) {
        return false;
    }
    if (!json_find_object_bounds(json, "extruder", &extruder_start, &extruder_end)) {
        return false;
    }
    if (!json_find_object_bounds(json, "heater_bed", &bed_start, &bed_end)) {
        return false;
    }

    if (!json_get_string_in_object(display_start, display_end, "message",
                                   status->message, sizeof(status->message))) {
        return false;
    }
    if (!json_get_number_in_object(display_start, display_end, "progress", &progress)) {
        return false;
    }
    if (!json_get_number_in_object(extruder_start, extruder_end, "temperature",
                                   &status->extruder_temp)) {
        return false;
    }
    if (!json_get_number_in_object(bed_start, bed_end, "temperature",
                                   &status->bed_temp)) {
        return false;
    }

    status->progress_percent = progress * 100.0;
    return true;
}

static esp_err_t moonraker_get_status(moonraker_status_t *status)
{
    char url[196];
    int n = snprintf(url, sizeof(url), "http://%s:%d%s",
                     CONFIG_MOONRAKER_IP, CONFIG_MOONRAKER_PORT, MOONRAKER_QUERY_PATH);
    if (n <= 0 || n >= (int)sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    http_response_buffer_t response = {0};
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .event_handler = http_event_handler,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        free(response.data);
        return err;
    }

    if (response.overflow) {
        free(response.data);
        return ESP_ERR_NO_MEM;
    }

    if (status_code != 200 || response.data == NULL) {
        ESP_LOGW(TAG, "Resposta HTTP invalida (status=%d, body_len=%u)",
                 status_code, (unsigned)response.len);
        if (response.data != NULL) {
            ESP_LOGD(TAG, "Body recebido: %.160s", response.data);
        }
        free(response.data);
        return ESP_ERR_INVALID_RESPONSE;
    }

    bool parsed = parse_moonraker_status(response.data, status);
    if (!parsed) {
        ESP_LOGW(TAG, "Falha no parse da resposta Moonraker (body_len=%u)",
                 (unsigned)response.len);
        ESP_LOGD(TAG, "Body recebido: %.220s", response.data);
    }
    free(response.data);
    return parsed ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static void moonraker_task(void *arg)
{
    (void)arg;

    while (1) {
        if (wifi_manager_is_connected()) {
            moonraker_status_t status = {0};
            esp_err_t err = moonraker_get_status(&status);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "msg=\"%s\" progress=%.1f%% extruder=%.2fC bed=%.2fC",
                         status.message,
                         status.progress_percent,
                         status.extruder_temp,
                         status.bed_temp);
                display_manager_show_status(status.message,
                                            (float)status.progress_percent,
                                            (float)status.extruder_temp,
                                            (float)status.bed_temp);
            } else {
                ESP_LOGW(TAG, "Falha ao consultar Moonraker (%s:%d): %s",
                         CONFIG_MOONRAKER_IP,
                         CONFIG_MOONRAKER_PORT,
                         esp_err_to_name(err));
                display_manager_show_error("Moonraker", esp_err_to_name(err));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_MOONRAKER_POLL_INTERVAL_MS));
    }
}

void moonraker_client_start(void)
{
    BaseType_t ok = xTaskCreate(
        moonraker_task,
        "moonraker_task",
        6144,
        NULL,
        5,
        NULL);

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Nao foi possivel criar task de polling Moonraker.");
    }
}
