#ifndef _READUART_H_
#define _READUART_H_

// FreeRTOS
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <semphr.h>

// Tiva
#include <stdbool.h>
#include <stdint.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <driverlib/gpio.h>
#include <driverlib/i2c.h>
#include <driverlib/pin_map.h>
#include <driverlib/rom.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>
#include <utils/uartstdio.h>


#define Q_SIZE 100

static QueueHandle_t read_uart;

void init_read_uart(void);

// Triggered on a UART interrupt.
void _read_uart_handler(void);

void read_uart_task(void* params);

#endif