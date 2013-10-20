/*All IO infrastructures are from zzz0072*/
/*Thank and respect his effort.*/

#define configUSE_TRACE_FACILITY
#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"
#include "stm32_p103.h"
#include "romfs.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"

/* Debug/Testing */
#ifdef RT_TEST
#include "unit_tests.h"
#endif

#include "string.h"
#include "shell.h"

extern const uint8_t _sromfs;

volatile xSemaphoreHandle serial_tx_wait_sem = NULL;
volatile xQueueHandle serial_rx_queue = NULL;

/* Queue structure used for passing characters. */
typedef struct {
    char ch;
} serial_ch_msg;
/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts). */
void USART2_IRQHandler()
{
    static signed portBASE_TYPE xHigherPriorityTaskWoken;
    serial_ch_msg rx_msg;

    /* If this interrupt is for a transmit... */
    if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
        /* "give" the serial_tx_wait_sem semaphore to notfiy processes
         * that the buffer has a spot free for the next byte.
         */
        xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

        /* Diables the transmit interrupt. */
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
        /* If this interrupt is for a receive... */
    }
    else if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        /* Receive the byte from the buffer. */
        rx_msg.ch = USART_ReceiveData(USART2);

        /* Queue the received byte. */
        if(!xQueueSendToBackFromISR(serial_rx_queue, &rx_msg, &xHigherPriorityTaskWoken)) {
            /* If there was an error queueing the received byte,
             * freeze. */
            while(1);
        }
    }
    else {
        /* Only transmit and receive interrupts should be enabled.
         * If this is another type of interrupt, freeze.
         */
        while(1);
    }

    if (xHigherPriorityTaskWoken) {
        taskYIELD();
    }
}

void send_byte(char ch)
{
    /* Wait until the RS232 port can receive another byte (this semaphore
     * is "given" by the RS232 port interrupt when the buffer has room for
     * another byte.
     */
    while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

    /* Send the byte and enable the transmit interrupt (it is disabled by
     * the interrupt).
     */
    USART_SendData(USART2, ch);
    USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

char receive_byte()
{
    serial_ch_msg msg;

    /* Wait for a byte to be queued by the receive interrupts handler. */
    while (!xQueueReceive(serial_rx_queue, &msg, portMAX_DELAY));

    return msg.ch;
}

void SleepingDemo()
{
	while (1);
}


void MyShell()
{
    char ch;
    char str[100];
    int curr_char;
    int done;
    char PsInfo[256];
    while (1)
    {
	/*gather commamd char by char*/
        printf("\r\nMyShell>>");
        curr_char = 0;
        done = 0;
        do
        {
            fio_read(0, &ch, 1);
 
            if (curr_char >= 98 || (ch == '\r') || (ch == '\n'))
            {
                str[curr_char] = '\0';
                done = -1;
            }
            else if (ch == 127)
            {
                if (curr_char>0)
                {
                    printf("\b \b");
                    curr_char--;
                }
            }
            else
            {
                str[curr_char++] = ch;
                printf("%c",ch);
 
            }
        }while (!done);
 
	/*select the proper one to execute*/
        if (0==strcmp("hello",str))
        {
            printf("nice to meet you");
	    
        }
        else if (0==strcmp("mmtest",str))
        {
            mmtest();
        }
        else if (0==strcmp("ps",str))
        {
	    /*still very rough*/
	    /*probably need to modify vTaskList() for better info*/
            vTaskList(PsInfo);
            printf("%s",PsInfo);
        }
        else if (0==strncmp("echo",str,4))
        {
            printf("\r\n%s", str+5);
        }
        else if (0==strncmp("system",str,6))
        {
            host_system(str+7,curr_char-7);
        }
        else if (0==strcmp("exe",str))
        {
            xTaskCreate(SleepingDemo,(signed portCHAR *) "SleepingDemo",512 /* stack size */, NULL,0, NULL);
        }
        else if (0==strcmp("help",str))
        {
            printf("\r\nThis is a very simple shell.");
            printf("\r\nThere are only 6 commands.");
            printf("\r\nhello - shows a welcome message.");
            printf("\r\necho  - shows a message you type after \'echo\' command.");
            printf("\r\nps    - shows all running tasks.");
            printf("\r\nexe   - add a task doing nothing but sleeping for the ");
            printf("purpose of demostrating.");
	    printf("\r\nsystem- execute the command on the host");
            printf("\r\nhelp  - the place where you are");
        }
        else
        {
            printf("\r\ncommand not supported");
            printf("\r\nType \"help\" to see further info.");
        }
    }
}
 

int main()
{
    init_rs232();
    enable_rs232_interrupts();
    enable_rs232();

    fs_init();
    fio_init();

    register_romfs("romfs", &_sromfs);

    /* Create the queue used by the serial task.  Messages for write to
     * the RS232. */
    vSemaphoreCreateBinary(serial_tx_wait_sem);
    serial_rx_queue = xQueueCreate(1, sizeof(serial_ch_msg));


    /* Create shell task */
    xTaskCreate(MyShell,(signed portCHAR *) "My Shell",512 /* stack size */, NULL, tskIDLE_PRIORITY + 1, NULL);

    /* Start running the tasks. */
    vTaskStartScheduler();

    return 0;
}

void vApplicationTickHook()
{
}
