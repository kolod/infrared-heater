
#pragma once
#include "config.h"

typedef struct {
    
} Controller;

extern Controller controller;
extern TaskHandle_t hControllerTask;
extern void controller_task(void *pvParameters);
extern void controller_init(Controller* handle);
