/******************************************************************************/
/*                                                                            */
/* project  : Trabajo SE-II UNIZAR                                            */
/* filename : comun.c                                                         */
/* version  : 2                                                               */
/* date     : 21/01/2025                                                      */
/* author   : Inés Román Gracia                                               */
/* description : Lecturas GPS con funciones de alto nievel y sin parsear.     */
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

#include "tm4c123gh6pm.h"
#include "computos.h"

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_uart.h"
#include "inc/hw_sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"

/******************************************************************************/
/*                        Global variables                                    */
/******************************************************************************/
Task_Handle periodicTask;

Semaphore_Handle periodicSem;

Clock_Handle periodicClock;

Clock_Params clockParams;
Task_Params taskParams;

char gpsBuffer[GPS_BUFFER_SIZE];
volatile int gpsBufferIndex = 0;
bool messageStarted = false;

char latencycommand[] = "$PMTK220,1000*1F\x0D\x0A";
char baudcommand[] = "$PMTK251,9600*17\x0D\x0A";
char latlongcommand[]= "$PMTK314,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\x0D\x0A";

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/
Void periodicTaskFunc(UArg arg0, UArg arg1);
Void InitUart(void);
Void periodic_release(void);
Void sendGpsCommand(const char *command);

/******************************************************************************/
/*                        Main                                                */
/******************************************************************************/
Void main()
{
    // Inicialización de UART y GPIO
    InitUart();

    // Task de tarea periódica
    Task_Params_init(&taskParams);
    taskParams.priority = 1;
    periodicTask = Task_create(periodicTaskFunc, &taskParams, NULL);

    // Clock para tarea periódica
    Clock_Params_init(&clockParams);
    clockParams.period = 100;
    clockParams.startFlag = TRUE;
    periodicClock = Clock_create((Clock_FuncPtr)periodic_release, 10, &clockParams, NULL);

    periodicSem = Semaphore_create(0, NULL, NULL);

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

    // Enviar comandos al GPS para configurar su salida
    sendGpsCommand(latencycommand);
    sendGpsCommand(baudcommand);
    sendGpsCommand(latlongcommand);
}

/******************************************************************************/
/*                        Periodic Task                                       */
/******************************************************************************/
Void periodic_release(void) {
    Semaphore_post(periodicSem);
}

Void periodicTaskFunc(UArg arg0, UArg arg1) {
    for (;;) {
        Semaphore_pend(periodicSem, BIOS_WAIT_FOREVER);
        while (UARTCharsAvail(UART1_BASE)) {
            char c = UARTCharGet(UART1_BASE);
            if (c == '$') {
                // Comenzar un nuevo mensaje
                messageStarted = true;
                gpsBufferIndex = 0;
            }

            if (messageStarted) {
                if (c == '\n' || gpsBufferIndex >= (GPS_BUFFER_SIZE - 1)) {
                    gpsBuffer[gpsBufferIndex] = '\0';
                    messageStarted = false;

                    // Mostrar el mensaje recibido
                    System_printf("GPS Data: %s\n", gpsBuffer);
                    System_flush();
                } else {
                    gpsBuffer[gpsBufferIndex++] = c;
                }
            }
        }
    }
}

/******************************************************************************/
/*                     Method to send command to GPS module                   */
/******************************************************************************/
void sendGpsCommand(const char *command) {
    while (*command) {
        UARTCharPut(UART1_BASE, *command++);
    }
}
