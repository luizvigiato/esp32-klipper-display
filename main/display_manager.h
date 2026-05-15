#pragma once

#include "esp_err.h"

esp_err_t display_manager_init(void);
void display_manager_show_status(const char *message, float progress_percent,
                                 float extruder_temp, float bed_temp);
void display_manager_show_error(const char *line1, const char *line2);
