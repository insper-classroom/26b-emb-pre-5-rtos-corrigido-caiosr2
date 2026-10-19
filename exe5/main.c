/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>


#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

QueueHandle_t xQueueBtn;
SemaphoreHandle_t xSemaphoreLedR;
SemaphoreHandle_t xSemaphoreLedY;

void btn_callback(uint gpio, uint32_t events) {
    if (events == GPIO_IRQ_EDGE_FALL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(xQueueBtn, &gpio, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// Pisca o LED enquanto estiver ativo; cada give no semáforo alterna
// entre piscando e apagado. O próprio take serve de delay de 100 ms,
// assim o LED responde ao botão na hora.
void led_blink(int pin, SemaphoreHandle_t sem) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);

    int blinking = 0;
    int state = 0;
    while (true) {
        TickType_t wait = blinking ? pdMS_TO_TICKS(100) : portMAX_DELAY;
        if (xSemaphoreTake(sem, wait) == pdTRUE) {
            blinking = !blinking;
            state = 0;
            gpio_put(pin, state);
        } else {
            state = !state;
            gpio_put(pin, state);
        }
    }
}

void led_r_task(void *p) {
    led_blink(LED_PIN_R, xSemaphoreLedR);
}

void led_y_task(void *p) {
    led_blink(LED_PIN_Y, xSemaphoreLedY);
}

void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    uint gpio;
    while (true) {
        if (xQueueReceive(xQueueBtn, &gpio, portMAX_DELAY) == pdTRUE) {
            if (gpio == BTN_PIN_R) {
                xSemaphoreGive(xSemaphoreLedR);
            } else if (gpio == BTN_PIN_Y) {
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}



int main() {
    stdio_init_all();

    xQueueBtn = xQueueCreate(32, sizeof(uint));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_Task R", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Task Y", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}
