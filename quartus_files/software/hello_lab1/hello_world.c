#include <system.h>
#include <altera_avalon_pio_regs.h>
#include <stdio.h>
#include "sccharts.h"
int main()
{
	// Initialise
	A, B, R = 1;

	unsigned int uiButtonsValue = 0;
	reset();
	while(1)
	{
		printf("A = %d\n", A);
		printf("B = %d\n", B);
		printf("R = %d\n", R);
		printf("O = %d\n", O);
		// Fetch button inputs
		uiButtonsValue = IORD_ALTERA_AVALON_PIO_DATA(KEYS_BASE);
		// A is Key 2, B is Key 1, R is Key 0
		// Remember that keys are active low
		// Do a tick!
		if (uiButtonsValue & 0b1) {
			R = 0;
		} else {
			R = 1;
		}

		if (uiButtonsValue & 0b10) {
			B = 0;
		} else {
			B = 1;
		}

		if (uiButtonsValue & 0b100) {
			A = 0;
		} else {
			A = 1;
		}
		tick();
		// Output O to Red LED
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, O);
	}
	return 0;
}

//#include <stdio.h>
//#include "system.h"
//#include "altera_avalon_pio_regs.h"
//#include "unistd.h"
//
//int main(void)
//{
//	unsigned int uiSwitchValue = 0;
//	unsigned int uiButtonsValue = 0;
//	unsigned int tempValuePrevious = 0;
//	unsigned int uiButtonsValuePrevious = 0;
//	unsigned int tempValue = 0;
//	FILE *lcd;
//	lcd = fopen(LCD_NAME, "w");
//
//
//	while(1) { //while the next node is not null
//		tempValuePrevious = tempValue;
//		uiButtonsValuePrevious = uiButtonsValue;
//		uiButtonsValue = IORD_ALTERA_AVALON_PIO_DATA(KEYS_BASE);
//
//		if (uiButtonsValue == 5) {
//
//			printf("%d\n",tempValue); //print the value of the node
//			usleep(500000);
//			tempValue++; //goto next node
//		}
//
//		if(lcd != NULL)
//		{
//			if(uiButtonsValue != uiButtonsValuePrevious) {
//				if (uiButtonsValue == 6) {
//					tempValue++; //goto next node
//					printf("%d\n",tempValue); //print the value of the node
//				}
//			}
//			if(tempValuePrevious != tempValue)
//			{
//
//				#define ESC 27
//				#define CLEAR_LCD_STRING "[2J"
//				fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
//				fprintf(lcd, "COUNTER VALUE: %d\n", tempValue);
//			}
//
//
//		}
//
//	}
//}

