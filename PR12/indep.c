/******************************************************************************/
/*                                                                            */
/* project  : Trabajo SE-II UNIZAR                                            */
/* date     : 24/01/2025                                                      */
/* author   : Inés Román Gracia                                               */
/* description : Lecturas GPS bajo nivel y sin parsear                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*                        Defines                                             */
/******************************************************************************/
#define TARGET_IS_TM4C123_RB1
#define BUFFER_SIZE 128

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
#include <ti/sysbios/hal/Hwi.h>

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

Hwi_Params hwiParams;
Hwi_Handle myHwi;

bool messageStarted = false;

char buffer[BUFFER_SIZE];
volatile int readIndex = 0;
volatile int writeIndex = 0;

char latencycommand[] = "$PMTK220,1000*1F\x0D\x0A";
char baudcommand[] = "$PMTK251,9600*17\x0D\x0A";
char latlongcommand[] = "$PMTK314,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\x0D\x0A";

/******************************************************************************/
/*                        Function Prototypes                                 */
/******************************************************************************/
Void periodicTaskFunc(UArg arg0, UArg arg1);
Void InitUart(void);
Void periodic_release(void);
Void sendGpsCommand(const char *command);
Void GPSInt(UArg);

/******************************************************************************/
/*                        Main                                                */
/******************************************************************************/
Void main()
{
    // Inicialización de UART y GPIO
    InitUart();

    // Configurar la interrupción
    Hwi_Params_init(&hwiParams);
    hwiParams.maskSetting = Hwi_MaskingOption_SELF;
    myHwi = Hwi_create(22, GPSInt, &hwiParams, NULL);

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

    // Enviar comandos al GPS
    sendGpsCommand(latencycommand);
    sendGpsCommand(baudcommand);
    sendGpsCommand(latlongcommand);

    BIOS_start();
}

/******************************************************************************/
/*                        UART Initialization                                */
/******************************************************************************/
Void InitUart(void) {
    SYSCTL_RCGCUART_R = 0x2;            // Activa UART1
    SYSCTL_RCGCGPIO_R = 0x2;            // Activa GPIOB

    GPIO_PORTB_DEN_R = 0x3;             // Habilita PB0 y PB1 como pines digitales
    GPIO_PORTB_AFSEL_R = 0x3;           // Habilita funciones hardware alternas en PB0 y PB1
    GPIO_PORTB_AMSEL_R = 0x0;           // Deshabilita función analógica en PB0 y PB1
    GPIO_PORTB_PCTL_R = 0x11;           // Configura PB0 y PB1 para UART1

    UART1_IBRD_R = 104;                 // Configura la parte entera del divisor de baudios
    UART1_FBRD_R = 11;                  // Configura la parte fraccional del divisor de baudios
    UART1_LCRH_R = 0x60;                // Configura 8 bits de palabra, sin paridad, 1 bit de stop
    UART1_CC_R = 0x0;                   // Selecciona el reloj del sistema para UART1
    UART1_CTL_R = 0x301;                // Habilita UART1, TX y RX

    UART1_ICR_R = ~0x10;                // Limpia las interrupciones previas
    UART1_IM_R = 0x10;                  // Habilita interrupción por recepción (RX)
    NVIC_EN0_R = 0x40;                  // Habilita interrupciones para UART1 (bit 6 en EN0)
}

/******************************************************************************/
/*                        Handler interruption                                */
/******************************************************************************/
Void GPSInt(UArg arg) {
    //if (UART1_MIS_R & 0x10) {
    if (!(UART1_DR_R & 0xF00)) {
        char received = UART1_DR_R & 0xFF;
        UART1_ICR_R |= 0x10;  // Clean
        if(((writeIndex + 1) % BUFFER_SIZE) != readIndex) {
            buffer[writeIndex] = received;
            writeIndex = (writeIndex+1)%BUFFER_SIZE;
        }
    }
}

/******************************************************************************/
/*                        Periodic Task                                       */
/******************************************************************************/
Void periodic_release(void) {
    Semaphore_post(periodicSem);
}

Void periodicTaskFunc(UArg arg0, UArg arg1) {
    char msgBuffer[BUFFER_SIZE];
    int msgIndex = 0;

    for (;;) {
        Semaphore_pend(periodicSem, BIOS_WAIT_FOREVER);

        while (readIndex != writeIndex) {
            char received = buffer[readIndex];
            readIndex = (readIndex + 1) % BUFFER_SIZE;

            if (received == '$') {
                msgIndex = 0;
            }

            if (msgIndex < BUFFER_SIZE - 1) {
                msgBuffer[msgIndex++] = received;
            }

            if (received == '\n') {
                msgBuffer[msgIndex] = '\0';
                System_printf("%s", msgBuffer);
                System_flush();

                msgIndex = 0;
            }
        }
    }
}


/******************************************************************************/
/*                     Method to send command to GPS module                   */
/******************************************************************************/
Void sendGpsCommand(const char *command) {
    while (*command) {
        while ((UART1_FR_R & UART_FR_TXFF) != 0) {
        }
        UART1_DR_R = *command;

        command++;
    }
}

