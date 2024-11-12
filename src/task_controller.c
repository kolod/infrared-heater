#include "task_controller.h"

Controller controller;
TaskHandle_t hControllerTask;

void controller_task(void *pvParameters) {
    (void) pvParameters;

    // TODO:

    // Wait signal from isr
    xTaskNotifyWait(0, 0xFFFFFFFF, NULL, portMAX_DELAY);
}

void controller_init(Controller *handle) {
    (void) handle;
}
