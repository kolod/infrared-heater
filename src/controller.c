#include "controller.h"
#include "controller_p.h"
#include "display.h"

TaskHandle_t hControllerTask;

void controller_task(void *pvParameters) {
    (void) pvParameters;

    vTaskDelay(100 / portTICK_PERIOD_MS);
    display_fill_screen(COLOR_GREEN);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    display_draw_text(&fira_code, COLOR_WHITE, COLOR_BLACK, 0, 0, L"Hellow");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    display_draw_text(&fira_code, COLOR_WHITE, COLOR_BLACK, 0, fira_code.height, L"World!");

    for(;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void controller_init(void) {
    if (xTaskCreate(controller_task, "Controller", 256, NULL, 4, &hControllerTask) != pdPASS)
		ASSERT("Controller task creation failed");
}
