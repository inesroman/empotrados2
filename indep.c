/******************************************************************************/
/*                                                                            */
/* project  : PRACTICAS SE-II UNIZAR                                          */
/* filename : comun.c                                                         */
/* version  : 2                                                               */
/* date     : 28/09/2020                                                      */
/* author   : Jose Luis Villarroel                                            */
/* description : Comun con servidor esporadico. PR2                           */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                        Defines                                             */
/******************************************************************************/

#define TARGET_IS_TM4C123_RB1
#define GPS_BUFFER_SIZE 128

/******************************************************************************/
/*                        Used modules                                        */
/******************************************************************************/

#include <xdc/std.h>
#include <stdbool.h>
#include <stdint.h>

#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <xdc/runtime/Log.h>
#include <ti/uia/events/UIABenchmark.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "computos.h"

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_uart.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_nvic.h"

#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"


/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/

Task_Handle uartReadTask;
Task_Handle uartWriteTask;

Semaphore_Handle uartReadSem;
Semaphore_Handle uartWriteSem;

Clock_Handle uartClockRead;
Clock_Handle uartClockWrite;

Clock_Params clockParams;
Task_Params taskParams;

char gpsBuffer[GPS_BUFFER_SIZE];
volatile int gpsBufferIndex = 0;

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/

Void uartReadTaskFunc(UArg arg0, UArg arg1);
Void uartWriteTaskFunc(UArg arg0, UArg arg1);
Void InitUart(void);
Void uartReadRelease(void);
Void uartWriteRelease(void);

/******************************************************************************/
/*                        Main                                                */
/******************************************************************************/

Void main()
{
    // Inicialización de UART y GPIO
    InitUart();

    // Task de lectura de UART
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    uartReadTask = Task_create(uartReadTaskFunc, &taskParams, NULL);

    // Clock para leer de UART
    Clock_Params_init(&clockParams);
    clockParams.period = 100;
    clockParams.startFlag = TRUE;
    uartClockRead = Clock_create((Clock_FuncPtr)uartReadRelease, 10, &clockParams, NULL);

    uartReadSem = Semaphore_create(0, NULL, NULL);

    // Task de escritura de UART
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    uartWriteTask = Task_create(uartWriteTaskFunc, &taskParams, NULL);

    // Clock para escribir a UART
    Clock_Params_init(&clockParams);
    clockParams.period = 200;
    clockParams.startFlag = TRUE;
    uartClockWrite = Clock_create((Clock_FuncPtr)uartWriteRelease, 10, &clockParams, NULL);

    uartWriteSem = Semaphore_create(0, NULL, NULL);

    BIOS_start();
}

/******************************************************************************/
/*                        UART Initialization                                */
/******************************************************************************/

Void InitUart(void) {
    // Habilitar el reloj de UART1 y GPIOB
        SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);  // Habilitar UART1
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);  // Habilitar GPIOB

        // Configurar PB0 y PB1 como UART
        GPIOPinConfigure(GPIO_PB0_U1RX);   // Configurar PB0 como RX
        GPIOPinConfigure(GPIO_PB1_U1TX);   // Configurar PB1 como TX
        GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Establecer pines como UART

        // Configuración de UART (9600 baudios, 8N1)
        UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), 9600, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

        // Limpiar las interrupciones de UART1
        UARTIntClear(UART1_BASE, UART_INT_RX);

        // Activar las interrupciones de UART1 (habilitar interrupción de recepción)
        UARTIntEnable(UART1_BASE, UART_INT_RX);

        // Habilitar la interrupción de UART1 en el NVIC (Interrupt vector number 22)
        IntEnable(22);

        IntMasterEnable();

        // Enviar comando de configuración al GPS
        sendGpsCommand("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n");
}

/******************************************************************************/
/*                        Tasks                                               */
/******************************************************************************/

Void uartReadRelease(void) {
    Semaphore_post(uartReadSem);
}

Void uartReadTaskFunc(UArg arg0, UArg arg1) {
    for (;;) {
        Semaphore_pend(uartReadSem, BIOS_WAIT_FOREVER);

        while (UARTCharsAvail(UART1_BASE)) {
            char c = UARTCharGet(UART1_BASE);  // Leer un carácter desde UART
            if (c == '\n' || gpsBufferIndex >= (GPS_BUFFER_SIZE - 1)) {
                gpsBuffer[gpsBufferIndex] = '\0';  // Terminar la cadena
                gpsBufferIndex = 0;

                // Mostrar el mensaje recibido
                System_printf("GPS Data: %s\n", gpsBuffer);
                System_flush();  // Asegurarse de que se imprima en la consola
            } else {
                gpsBuffer[gpsBufferIndex++] = c;
            }
        }
    }
}

Void uartWriteRelease(void) {
    Semaphore_post(uartWriteSem);
}

Void uartWriteTaskFunc(UArg arg0, UArg arg1)
{
    for (;;) {
        Semaphore_pend(uartWriteSem, BIOS_WAIT_FOREVER);
        CS (40) ;
    }
}

/******************************************************************************/
/*                     Tarea para enviar comandos al GPS                      */
/******************************************************************************/
void sendGpsCommand(const char *command) {
    while (*command) {
        UARTCharPut(UART1_BASE, *command++);  // Envía cada carácter por UART
    }
}


/******************************************************************************/
/*                         Interruption handler                               */
/******************************************************************************/
void UART1IntHandler(void) {
    uint32_t status = UARTIntStatus(UART1_BASE, true);
    UARTIntClear(UART1_BASE, status);

    if (status & UART_INT_RX) {
        Semaphore_post(uartReadSem);
    }
}
