
// STANDARD INCLUDES
#include <stdio.h>
#include <conio.h>

// KERNEL INCLUDES
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

// HARDWARE SIMULATOR UTILITY FUNCTIONS  
#include "HW_access.h"

// TASK PRIORITIES 
#define	SERVICE_TASK_PRI		( tskIDLE_PRIORITY + 1 )
#define	SEND_MESSAGE_PRI		( tskIDLE_PRIORITY + (UBaseType_t)2 )
#define DISTANCE_CHECK			( tskIDLE_PRIORITY + (UBaseType_t)3 )
#define PROCESSING_PRI			( tskIDLE_PRIORITY + (UBaseType_t)4 )
#define RECEIVE_VALUE_PRI		( tskIDLE_PRIORITY + (UBaseType_t)5 )
#define RECEIVE_COMMAND_PRI		( tskIDLE_PRIORITY + (UBaseType_t)6 )


//DEFINES
#define MAX_CHARACTERS (20)
#define ASCII_CR (46)
#define LENGTH_TEMPOMAT_85 (11)
#define LENGTH_TEMPOMAT_OFF (12)
#define DISTANCE_THRESHOLD (500)

static uint8_t rezimRada = (uint8_t)0;


// SERIAL SIMULATOR CHANNEL TO USE 
#define COM_CH_0 (0)
#define COM_CH_1 (1)
#define COM_CH_2 (2)


// TASKS: FORWARD DECLARATIONS 
//void LEDBar_Task(void* pvParameters);
static void ReceiveValueTask(void* pvParameters);
static void ReceiveCommandTask(void* pvParameters);
static void ProcessingDataTask(void* pvParameters);
static void distanceCheck(void* pvParameters);
static void speedControlTask(void* pvParameters);
static void TimerCallback(TimerHandle_t tmH);


// GLOBAL OS-HANDLES 
static SemaphoreHandle_t LED_INT_BinarySemaphore;
static SemaphoreHandle_t UART0_sem, UART1_sem, UART2_sem;
static SemaphoreHandle_t Sens1_SEM, Sens2_SEM, Sens3_SEM;
static TimerHandle_t per_TimerHandle;
static QueueHandle_t Queue1;
static QueueHandle_t Queue2;
//static QueueHandle_t Queue3;
static SemaphoreHandle_t SendInfo_BinarySemaphore;
static SemaphoreHandle_t Display_BinarySemaphore;
static TimerHandle_t per_TimerHandle;


// INTERRUPTS //

static uint32_t UARTInterrupt(void) {
//interapt samo dize flagove, a prijem i obrada podataka se vrsi u taskovima
	BaseType_t xHigherPTW = pdFALSE;
	if (get_RXC_status(0) != 0) {
		if (xSemaphoreGiveFromISR(UART0_sem, &xHigherPTW) != pdPASS) {
			printf("Error UART0_sem\n");
		}
	}
	if (get_RXC_status(1) != 0) {
		if (xSemaphoreGiveFromISR(UART1_sem, &xHigherPTW) != pdPASS) {
			printf("Error UART1_sem\n");
		}
	}
	if (get_RXC_status(2) != 0) {
		if (xSemaphoreGiveFromISR(UART2_sem, &xHigherPTW) != pdPASS) {
			printf("Error UART2_sem\n");
		}
	}
	portYIELD_FROM_ISR((uint32_t)xHigherPTW);
}

// MAIN - SYSTEM STARTUP POINT 
void main_demo(void) {

	// INITIALIZATION //
	if (init_serial_uplink(COM_CH_0) != 0) {
		printf("Error TX_COM0_INIT\n");
	}
	if (init_serial_downlink(COM_CH_0) != 0) {
		printf("Error RX_COM0_INIT\n");
	}
	if (init_serial_uplink(COM_CH_1) != 0) {
		printf("Error TX_COM1_INIT\n");
	}
	if (init_serial_downlink(COM_CH_1) != 0) {
		printf("Error RX_COM1_INIT\n");
	}
	if (init_serial_uplink(COM_CH_2) != 0) {
		printf("Error TX_COM2_INIT\n");
	}
	if (init_serial_downlink(COM_CH_2) != 0) {
		printf("Error RX_COM1_INIT\n");
	}



	// SERIAL RECEPTION INTERRUPT HANDLER //
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, UARTInterrupt);
	//vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);


	// SEMAPHORES //
	UART0_sem = xSemaphoreCreateBinary();
	if (UART0_sem == NULL) {
		printf("Error RX_SEM0_CREATE\n");
	}

	UART1_sem = xSemaphoreCreateBinary();
	if (UART1_sem == NULL) {
		printf("Error RX_SEM1_CREATE\n");
	}

	Sens1_SEM = xSemaphoreCreateBinary();
	if (Sens1_SEM == NULL) {
		printf("Error Sens1_SEM_CREATE\n");
	}

	Sens2_SEM = xSemaphoreCreateBinary();
	if (Sens2_SEM == NULL) {
		printf("Error Sens2_SEM_CREATE\n");
	}

	Sens3_SEM = xSemaphoreCreateBinary();
	if (Sens3_SEM == NULL) {
		printf("Error Sens2_SEM_CREATE\n");
	}

	//SETUP UART
	UART0_sem = xSemaphoreCreateBinary();
	if (UART0_sem == NULL)
	{
		printf("error : UART0_sem create");
	}

	UART1_sem = xSemaphoreCreateBinary();
	if (UART1_sem == NULL)
	{
		printf("error : UART1_sem create");
	}

	UART2_sem = xSemaphoreCreateBinary();
	if (UART2_sem == NULL)
	{
		printf("error : UART2_sem create");
	}


	// QUEUES //
	Queue1 = xQueueCreate((uint8_t)10, (uint8_t)MAX_CHARACTERS * (uint8_t)sizeof(char));
	if (Queue1 == NULL) {
		printf("Error QUEUE1_CREATE\n");
	}

	Queue2 = xQueueCreate((uint8_t)10, (uint8_t)2 * (uint8_t)sizeof(uint8_t));
	if (Queue2 == NULL) {
		printf("Error QUEUE2_CREATE\n");
	}



	/*Queue3 = xQueueCreate((uint8_t)10, (uint8_t)sizeof(uint8_t));
	if (Queue3 == NULL) {
		printf("Error QUEUE3_CREATE\n");
	}*/


	// TIMERS //
	TimerHandle_t Timer200ms = xTimerCreate(
		NULL,
		pdMS_TO_TICKS(2000),
		pdTRUE,
		NULL,
		TimerCallback);
	if (Timer200ms == NULL) {
		printf("Error TIMER_CREATE\n");
	}
	if (xTimerStart(Timer200ms, 0) != pdPASS) {
		printf("Error TIMER_START\n");
	}


	// TASKS //

	BaseType_t status1;

	status1 = xTaskCreate(
		ReceiveCommandTask,
		"prijem komande",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)RECEIVE_COMMAND_PRI,
		NULL);
	if (status1 != pdPASS) {
		printf("Error RECEIVE_COMMAND_TASK_CREATE\n");
	}

	status1 = xTaskCreate(
		ReceiveValueTask,
		"prijem podataka sa senzora",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)RECEIVE_VALUE_PRI,
		NULL);
	if (status1 != pdPASS) {
		printf("Error RECEIVE_VALUE_TASK_CREATE\n");
	}

	status1 = xTaskCreate(
		ProcessingDataTask,
		"obrada podataka",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)PROCESSING_PRI,
		NULL);
	if (status1 != pdPASS) {
		printf("Error PROCESSING_DATA_TASK_CREATE\n");
	}

	status1 = xTaskCreate(
		distanceCheck,
		"provjera udaljenosti",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)DISTANCE_CHECK,
		NULL);
	if (status1 != pdPASS) {
		printf("Error DISTANCE_CHECK_TASK_CREATE\n");
	}

	status1 = xTaskCreate(
		speedControlTask,
		"kontrola brzine",
		configMINIMAL_STACK_SIZE,		// nije ispostovano Directive 4.6 jer je makro definisan u drugom fajlu
		NULL,
		(UBaseType_t)DISTANCE_CHECK,
		NULL);
	if (status1 != pdPASS) {
		printf("Error DISTANCE_CHECK_TASK_CREATE\n");
	}

	vTaskStartScheduler();
	for (;;) {
		// da bi se ispostovalo pravilo misre
	}


}



// TASKS: IMPLEMENTATIONS
/*void LEDBar_Task(void* pvParameters) {
	unsigned i;
	uint8_t d;
	while (1) {
		xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY);
		get_LED_BAR(0, &d);
		i = 3;
		do {
			i--;
			select_7seg_digit(i);
			set_7seg_digit(hexnum[d % 10]);
			d /= 10;
		} while (i > 0);
	}
}*/


// PERIODIC TIMER CALLBACK 
static void TimerCallback(TimerHandle_t tmH) {
	if (send_serial_character((uint8_t)COM_CH_1, (uint8_t)'T') != 0) { //ovo daje ritam aj da kazem tako
		printf("Error SEND_TRIGGER\n");
	}
	vTaskDelay(pdMS_TO_TICKS(200));
}


static void ReceiveCommandTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t position = 0;
	uint8_t cc = 0;

		for (;;) {
			if (xSemaphoreTake(UART0_sem, portMAX_DELAY) != pdTRUE) {
				printf("Error UART0_sem\n");
			}
			if (get_serial_character(COM_CH_0, &cc) != 0) {
				printf("Error GET_CHARACTER1\n");
			}
			if (cc == (uint8_t)ASCII_CR) {
				tmpString[position] = '\0';
				position = 0;
				if (xQueueSend(Queue1, tmpString, 0) != pdTRUE) {
					printf("Error QUEUE1_SEND\n");
				}
				else {
					if (xSemaphoreGive(Sens1_SEM) != pdTRUE) {
						printf("Error: Sens1_SEM Give\n");
					}
				}
				printf("Received word: %s\n", tmpString); // Print received word
			}
			else if (position < (uint8_t)MAX_CHARACTERS) {
				tmpString[position] = (char)cc;
				position++;
			}
			else {
				// Handle overflow or other cases as needed
			}
		}
}



static void ReceiveValueTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t position = 0;
	static uint16_t* value = 0;
	uint8_t cc = 0;

	for (;;) {
		if (xSemaphoreTake(UART1_sem, portMAX_DELAY) != pdTRUE) {
			printf("Error UART1_sem\n");
		}

		if (get_serial_character(COM_CH_1, &cc) != 0) {
			printf("Error get_serial_character\n");
		}

		printf("Kanal1: %c\n", cc);

		if (cc == (uint8_t)ASCII_CR) {
			// Convert string to integer
			value = (uint16_t)(tmpString[0] - (uint16_t)'0') * (uint16_t) 100;
			value += (uint16_t) (tmpString[1] - (uint16_t)'0') * (uint16_t)5;
			value += (uint16_t) (tmpString[2] - (uint16_t)'0') / (uint16_t)2;
			tmpString[position] = '\0';
			position = 0;

			printf("Received : %d\n", value);

			if (xQueueSend(Queue2, &value, 0) != pdTRUE) {
				printf("Error QUEUE1_SEND\n");
			}
			else {
				if (xSemaphoreGive(Sens2_SEM) != pdTRUE) {
					printf("Error: Sens2_SEM Give\n");
				}
				if (xSemaphoreGive(Sens3_SEM) != pdTRUE) {
					printf("Error: Sens3_SEM Give\n");
				}
			}

		}

		else if (position < (uint8_t)MAX_CHARACTERS) {
			tmpString[position] = (char)cc;
			position++;
		}
		else {
			// Handle overflow or other cases as needed
		}

		

	}
}

static void ProcessingDataTask(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint8_t state = (uint8_t)0;

	for (;;) {
		if (xSemaphoreTake(Sens1_SEM, portMAX_DELAY/*pdMS_TO_TICKS(3000) */) != pdTRUE) {
			printf("Error: Sens1_SEM Receive\n");
		}
		if (xQueueReceive(Queue1, tmpString, portMAX_DELAY) != pdTRUE) {
			printf("Error QUEUE1_RECEIVE\n");
		}
		else if (memcmp(tmpString, "TEMPOMAT_85.", LENGTH_TEMPOMAT_85) == 0) {
			state = (uint8_t)0;
		}
		else if (memcmp(tmpString, "TEMPOMAT_OFF.", LENGTH_TEMPOMAT_OFF) == 0) {
			state = (uint8_t)1;
		}
		else {
			printf("Doslo je do greske!\n");
			continue; // Preskace ostatak petlje i prelazi na sledecu iteraciju
		}

		if (state == (uint8_t)0) {
			rezimRada = (uint8_t)0;
			printf("Tempomat: UKLJUCEN\n");
			printf("Brzina: 85 km/h\n");
		}
		else if (state == (uint8_t)1) {
			rezimRada = (uint8_t)1;
			printf("Tempomat: ISKLJUCEN\n");
		}
	}
}

static void distanceCheck(void* pvParameters) {
	static char tmpString[MAX_CHARACTERS];
	static uint16_t udaljenost = (uint16_t)0;

	for (;;) {
		if (xSemaphoreTake(Sens2_SEM, portMAX_DELAY/*pdMS_TO_TICKS(3000) */) != pdTRUE) {
			printf("Error: Sens2_SEM Receive\n");
		}
		if (xQueuePeek(Queue2, &udaljenost, portMAX_DELAY) != pdTRUE) {
			printf("Error QUEUE2_RECEIVE\n");
		}
		if (udaljenost < 500) {
			printf("Auto ispred\n");
		}
		else {
			printf("Dozvoljena udaljenost\n");
		}
	}
}


static void speedControlTask(void* pvParameters) {
	uint32_t udaljenost1 = 0;
	static uint8_t currentSpeed = 85; // Initial speed in km/h (dummy value)

	for (;;) {
		if (xSemaphoreTake(Sens3_SEM, portMAX_DELAY/*pdMS_TO_TICKS(3000) */) != pdTRUE) {
			printf("Error: Sens3_SEM Receive\n");
		}
		// Receive distance data from the queue
		if (xQueueReceive(Queue2, &udaljenost1, portMAX_DELAY) != pdTRUE) {
			printf("Error receiving distance data from queue\n");
		}

		// Adjust speed based on distance
		if (udaljenost1 < 500) {
			// Decrease speed if distance is less than threshold
			if (currentSpeed > 0) {
				currentSpeed -= 1;
				//vTaskDelay(pdMS_TO_TICKS(3000));
			}

			printf("C: %d \n", udaljenost1);
			
		}

			else {
				// Increase speed if distance is greater than or equal to threshold
				if (currentSpeed < 85) { // Maximum speed is set to 85 km/h
					currentSpeed += 1;
					//vTaskDelay(pdMS_TO_TICKS(3000));
				}
			}
		

		// Print current speed
		printf("Current speed: %d km/h\n", currentSpeed);


	}

}
