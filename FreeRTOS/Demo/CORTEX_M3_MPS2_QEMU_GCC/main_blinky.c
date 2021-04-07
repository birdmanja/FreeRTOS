/*
 * FreeRTOS V202012.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <stdio.h>

static void prvQueueReceiveTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

#define mainQUEUE_RECEIVE_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )
#define mainQUEUE_SEND_TASK_PRIORITY        ( tskIDLE_PRIORITY + 1 )
#define mainQUEUE_LENGTH                    ( 1 )
#define mainQUEUE_SEND_FREQUENCY_MS         ( 1000 / portTICK_PERIOD_MS )
/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

// The unit for voltage bounds are mV and for temperature are mili degrees Celcius.
#define PRECISION                           1000UL
#define VOLTAGE_LOWER_BOUND                 0UL
#define VOLTAGE_UPPER_BOUND                 10000UL
#define TEMPERATURE_LOWER_BOUND             -25000L
#define TEMPERATURE_UPPER_BOUND             85000L

const long VOLTAGE_RANGE = VOLTAGE_UPPER_BOUND - VOLTAGE_LOWER_BOUND;
const long TEMPERATURE_RANGE = TEMPERATURE_UPPER_BOUND - TEMPERATURE_LOWER_BOUND;
static long GRADIENT = (VOLTAGE_UPPER_BOUND - VOLTAGE_LOWER_BOUND) / 20UL;

void main_blinky( void )
{
    /* Create the queue. */
    xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint64_t ) );

    if( xQueue != NULL )
    {
        /* Start the two tasks as described in the comments at the top of this
        file. */
        xTaskCreate( prvQueueReceiveTask,            /* The function that implements the task. */
                    "Rx",                            /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                    configMINIMAL_STACK_SIZE,        /* The size of the stack to allocate to the task. */
                    NULL,                            /* The parameter passed to the task - not used in this case. */
                    mainQUEUE_RECEIVE_TASK_PRIORITY, /* The priority assigned to the task. */
                    NULL );                          /* The task handle is not required, so NULL is passed. */

        xTaskCreate( prvQueueSendTask,
                    "TX",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    mainQUEUE_SEND_TASK_PRIORITY,
                    NULL );

        /* Start the tasks and timer running. */
        vTaskStartScheduler();
    }

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was insufficient FreeRTOS heap memory available for the Idle and/or
    timer tasks to be created.  See the memory management section on the
    FreeRTOS web site for more details on the FreeRTOS heap
    http://www.freertos.org/a00111.html. */
    for( ;; );
}

static void prvQueueSendTask( void *pvParameters )
{
TickType_t xNextWakeTime;
uint32_t ulValueToSend = VOLTAGE_LOWER_BOUND;

    /* Remove compiler warning about unused parameter. */
    ( void ) pvParameters;

    /* Initialise xNextWakeTime - this only needs to be done once. */
    xNextWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        /* Place this task in the blocked state until it is time to run again. */
        vTaskDelayUntil( &xNextWakeTime, mainQUEUE_SEND_FREQUENCY_MS );

        // determine whether the next value should increase or decrease
        if(ulValueToSend >= VOLTAGE_UPPER_BOUND) {
            GRADIENT *= -1;
        } else if (GRADIENT < 0 && ulValueToSend <= VOLTAGE_LOWER_BOUND) {
            GRADIENT *= -1;
        }

        // modify next value to send
        ulValueToSend += GRADIENT;

        // Put tick count into message as time stamp
        uint64_t msg = ((uint64_t)xTaskGetTickCount()) << 32;
        // Put reading into the message
        msg |= ulValueToSend;
        
        /* Send to the queue - causing the queue receive task to unblock and
        toggle the LED.  0 is used as the block time so the sending operation
        will not block - it shouldn't need to block as the queue should always
        be empty at this point in the code. */
        xQueueSend( xQueue, &msg, 0U );
    }
}

volatile uint32_t ulRxEvents = 0;
static void prvQueueReceiveTask( void *pvParameters )
{
uint64_t ulReceivedValue;
const uint32_t ulExpectedValue = 100UL;

    /* Remove compiler warning about unused parameter. */
    ( void ) pvParameters;

    for( ;; )
    {
        /* Wait until something arrives in the queue - this task will block
        indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
        FreeRTOSConfig.h. */
        xQueueReceive( xQueue, &ulReceivedValue, portMAX_DELAY );

        // Parse tick count and original reading
        TickType_t tick = (uint32_t) (ulReceivedValue >> 32);
        uint32_t reading = (uint32_t) ulReceivedValue;

        // convert reading to temperature
        long temperature = (reading - VOLTAGE_LOWER_BOUND) / (double)VOLTAGE_RANGE * TEMPERATURE_RANGE + TEMPERATURE_LOWER_BOUND;

        // Print tick count as time stamp and temperature
        printf("Tick %ld:\t%ld E-3 Celcius\n", tick, (long)temperature);

        vTaskDelay(1000);
        ulRxEvents++;
    }
}
/*-----------------------------------------------------------*/
