#include "config.h"

// Minimal __assert() uses __assert__func()
__attribute__((noreturn)) void __assert(const char *file, const int line, char* failedexpr) {
	(void) file;
	(void) line;
	(void) failedexpr;

	// TODO: Print error message
	for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName ) {
	(void) xTask;
	ASSERT(pcTaskName);
}
