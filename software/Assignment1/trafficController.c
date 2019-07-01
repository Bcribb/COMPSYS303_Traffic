/* Traffic Light Controller
 *
 * --- Code is best viewed with the tab size of 4. ---
 */

#include <system.h>
#include <sys/alt_alarm.h>
#include <sys/alt_irq.h>
#include <altera_avalon_pio_regs.h>
#include <alt_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// COMPSYS 303 Assignment 1
// Group 16 : Blain Cribb and Jerry Yang
// Date: 2018/08/23


// FUNCTION PROTOTYPES
// Timer ISRs
alt_u32 tlc_timer_isr(void* context);
alt_u32 camera_timer_isr(void* context);

//  Misc
// Others maybe added eg LEDs / UART
void lcd_set_mode(unsigned int mode);

// TLC state machine functions
void init_tlc(void);
void simple_tlc(int* state);
void pedestrian_tlc(int* state);
void configurable_tlc(int* state);
void camera_tlc(int* state);

// Button Inputs / Interrupts
int buttons = 0;			// status of mode button
void handle_vehicle_button(void);
void init_buttons_pio(int buttons);
void button_isr(void* context, alt_u32 id);

// Received input function
unsigned int getValues();

// CONSTANTS
#define OPERATION_MODES 0x03	// number of operation modes (00 - 03 = 4 modes)
#define CAMERA_TIMEOUT	2000	// timeout period of red light camera (in ms)
#define TIMEOUT_NUM 8			// number of timeouts---changed from 6->8
#define TIME_LEN 8				// buffer length for time digits
#define ESC 27
#define CLEAR_LCD_STRING "[2J"


// USER DATA TYPES
// Timeout buffer structure
typedef struct  {
	int index;
	unsigned int timeout[TIMEOUT_NUM];
} TimeBuf;


// GLOBAL VARIABLES
static alt_alarm tlc_timer;		// alarm used for traffic light timing
static alt_alarm camera_timer;	// alarm used for camera timing


// NOTE:
// set contexts for ISRs to be volatile to avoid unwanted Compiler optimisation

// EVENT FLAGS
static volatile int car_time_value = 0;
static volatile int pedestrianNS = 0;
static volatile int pedestrianEW = 0;
int previousState = -1;
int pedNSHandled = 0; //
int pedEWHandled = 0;
int pedEWFin = 0;
int pedNSFin = 0;
int tick = 0;
int carButton = 0;
char receivedTime[100];
char receivedChar;
int timeoutFlag = 0;


// For calculate the time of a car pass.
clock_t car_time;
clock_t difference;

// 4 States of 'Detection':
// Car Absent
// Car Enters
// Car is Detected running a Red
// Car Leaves
static int vehicle_detected = 0;

// Traffic light timeouts
//static unsigned int timeout[TIMEOUT_NUM] = {500, 6000, 2000, 500, 6000, 2000}; -----DEFAULT
static unsigned int timeout[TIMEOUT_NUM] = {1000, 1000, 1000, 1000, 1000, 1000};
static TimeBuf timeout_buf = { -1, {500, 6000, 2000, 500, 6000, 2000} };

// UART
FILE* fp;

// Traffic light LED values
// NS RYG | EW RYG
// NR,NY | NG,ER,EY,EG
static unsigned char traffic_lights[TIMEOUT_NUM] = {0x24, 0x21, 0x22, 0x24, 0xC, 0x14, 0x61, 0x8C};

enum traffic_states {RR0, GR, YR, RR1, RG, RY};

static unsigned int mode = 0;
// Process states: use -1 as initialization state
static int proc_state[OPERATION_MODES + 1] = {-1, -1, -1, -1};

// Initialize the traffic light controller
// for any / all modes
void init_tlc(void)
{
	// Reset the LCD to the new mode
	lcd_set_mode(mode);

	// Rest the buttons to defaults
	init_buttons_pio(buttons);
	carButton = 0;
	vehicle_detected = 0;

	// Timeoutflag should be set so it doesn't ask for new values after switching modes
	timeoutFlag = 0;

	// Start the state timer, context is current state
	void* timerContext = (void*)&proc_state[mode];
	if(mode != 2 && mode != 3) {
		alt_alarm_start(&tlc_timer, timeout[0], tlc_timer_isr, timerContext);
	} else {
		// In mode 3 run a different timeout from UART
		alt_alarm_start(&tlc_timer, timeout_buf.timeout[0], tlc_timer_isr, timerContext);
	}
}

/* DESCRIPTION: Writes the mode to the LCD screen
 * PARAMETER:   mode - the current mode
 * RETURNS:     none
 */
void lcd_set_mode(unsigned int mode)
{
	// We need to display in range 1-4 rather than 0 - 3
	mode = mode + 1;

	// Open lcd as write file
	FILE *lcd;
	lcd = fopen(LCD_NAME, "w");

	// Clear the lcd and write the current mode
	if(lcd != NULL)
	{
		fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
		fprintf(lcd, "Current mode: %d\n", mode);
	}
	fclose(lcd);//close Lcd
}

/* DESCRIPTION: Simple traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
void simple_tlc(int* state)
{
	if (*state == -1) {
		// Process initialization state
		init_tlc();
		(*state)++;
		return;
	} else{
		// SET LED States for Simple traffic controller
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[*state]);
	}
}


/* DESCRIPTION: Handles the traffic light timer interrupt
 * PARAMETER:   context - opaque reference to user data
 * RETURNS:     Number of 'ticks' until the next timer interrupt. A return value
 *              of zero stops the timer.
 */
alt_u32 tlc_timer_isr(void* context)
{
	// Cycle through states 0 to 5
	volatile int* trigger = (volatile int*)context;
	if (*trigger == 5) {
		*trigger = 0;
	} else{
		(*trigger)++;
	}

	// Print current state to terminal
	printf("State: %d\n", *trigger);

	// In mode 3 of 4 use timeout_buf value to check time between
	// LED's switching states
	if((mode != 2) && (mode != 3)) {
		tick = timeout[*trigger];
	} else {
		// Use timeout buffer to be the time period
		tick = timeout_buf.timeout[*trigger];
		printf("%d\r\n\0", tick);
	}
	return tick;
}


/* DESCRIPTION: Initialize the interrupts for all buttons
 * PARAMETER:   none
 * RETURNS:     none
 */
void init_buttons_pio(int buttons)
{
	// Initialize NS/EW pedestrian button
	// Reset the edge capture register
	pedEWHandled = 0;
	pedNSHandled = 0;
	void* context_going_to_be_passed = (void*) &buttons;
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE, 0);
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(KEYS_BASE, 0x7);

	// Start the buttons for interrupts
	alt_irq_register(KEYS_IRQ,context_going_to_be_passed, button_isr);
}


/* DESCRIPTION: Pedestrian traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
void pedestrian_tlc(int* state)
{
	if (*state == -1) {
		// Process initialization state
		init_tlc();
		(*state)++;
		return;
	} else {
		// SET LED States, if we've queued pedestrians set them too
		if (pedestrianNS && *state == 1) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[6]);
			pedNSFin = 1;
		} else if (pedestrianEW && *state == 4) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[7]);
			pedEWFin = 1;
		} else {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[*state]);
		}

		// Wait till we're done walking before we register pedestrian button pushes
		if (pedNSFin && *state == 2) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE) & ~(0b1));
			pedestrianNS = 0;
			pedNSFin = 0;
			pedNSHandled = 0;
		}
		if (pedEWFin && *state == 5) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE) & ~(0b10));
			pedestrianEW = 0;
			pedEWFin = 0;
			pedEWHandled = 0;
		}

		// If the buttons has previously been pushed and we're in a valid state, queue the walk light
		if (pedNSHandled && *state != 1) {
			pedNSHandled = 0;
			pedestrianNS = 1;
		}
		if (pedEWHandled && *state != 4) {
			pedEWHandled = 0;
			pedestrianEW = 1;
		}
	}

	previousState = *state;

	// Same as simple TLC
	// with additional states / signals for Pedestrian crossings


}


/* DESCRIPTION: Handles the NSEW pedestrian button interrupt, and car interrupt
 * PARAMETER:   context - opaque reference to user data
 *              id - hardware interrupt number for the device
 * RETURNS:     none
 */
void button_isr(void* context, alt_u32 id)
{
	// NOTE:
	// Cast context to volatile to avoid unwanted compiler optimization.
	// Store the value in the Button's edge capture register in *context
	volatile int* value = (volatile int*)context;
	(*value) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(KEYS_BASE, 0);
	printf("button: %i\n", *value);

	// Handles the pedestrian buttons
	if (*value & 0b1) {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE) | 1);
		pedNSHandled = 1;
	}
	if (*value & 0b10) {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE, IORD_ALTERA_AVALON_PIO_DATA(LEDS_RED_BASE) | 2);
		pedEWHandled = 1;
	}

	// Handles the car button
	if (*value & 0b100) {
		carButton = 1;
	}
}


/* DESCRIPTION: Configurable traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
/*
If there is new configuration data... Load it.
Else run pedestrian_tlc();
 */
void configurable_tlc(int* state)
{
	if (*state == -1) {
		// Process initialization state
		init_tlc();
		(*state)++;
		return;
	}
	pedestrian_tlc(state);

	// Initialise valid to -1 so we know we need
	// get new timeout values
	unsigned int valid = -1;

	// If we've received a request to change timeouts
	// and we're in a safe state, get new timeout values
	if(timeoutFlag && (*state == 0 || *state == 3)) {
		pedestrian_tlc(state);
		int prevState = *state;
		while(valid == -1) {
			valid = getValues();
		}
		timeoutFlag = 0;

		// Do this so we start with new times instead of old timer
		*state = prevState;
		alt_alarm_stop(&tlc_timer);
		alt_alarm_start(&tlc_timer, timeout_buf.timeout[*state], tlc_timer_isr, state);

		pedestrian_tlc(state);
	}
}

/* DESCRIPTION: Gets the values from the user
 * PARAMETER:   N/A
 * RETURNS:     And unsigned integer, 0 for success, -1 for failure
 */
unsigned int getValues(void) {
	// Request a value from the user
	fprintf(fp, "\r\nEnter a value: ");

	// Get the first character
	if(fp != NULL) receivedChar = fgetc(fp);
	int counter = 0;

	// If the received value is not \n or \r, add it to our received string
	if(fp != NULL) {
		while(receivedChar != 13 && receivedChar != 10 ) {
			receivedTime[counter] = receivedChar;
			counter++;
			receivedTime[counter] = '\0';

			// Helps user see what they have entered
			fprintf(fp, "%c\0\r\n", receivedChar);

			receivedChar = fgetc(fp);
		}
		// Once we've found a \n add a comma to the end
		receivedTime[counter] = ',';
	}

	// If one of the received values is not a numbe ror comma, return -1 for invalid
	for(int i = 0;i<strlen(receivedTime);i++){
		if (((receivedTime[i]<48)  || (receivedTime[i] > 57)) && (receivedTime[i] != 44  )){
			return -1;
		}
	}


	int index = 0;
	unsigned int newNumber;
	unsigned int array[6];

	// Tokenise the first number
	newNumber = atoi(strtok(receivedTime, ","));
	while(index < 6) {
		// If the token is outside the acceptable range, return -1 for invalid
		if(newNumber < 1 || newNumber > 9999) {
			return -1;
		}
		array[index] = newNumber;
		index++;

		// Tokenise the next character
		newNumber = atoi(strtok(NULL, ","));
	}

	// Add the new values to the buffer array
	index = 0;
	while(index < 6) {
		timeout_buf.timeout[index] = array[index];
		index++;
	}

	// Tell the user their change was successful
	fprintf(fp,"\r\nSuccessful change\r\n");

	// Return success
	return 0;
}

/* DESCRIPTION: Handles the red light camera timer interrupt
 * PARAMETER:   context - opaque reference to user data
 * RETURNS:     Number of 'ticks' until the next timer interrupt. A return value
 *              of zero stops the timer.
 */
alt_u32 camera_timer_isr(void* context)
{
	// If a car is detected, send "Snapshot taken"
	volatile int* trigger = (volatile int*)context;
	if(*trigger) {
		if(fp != NULL)fprintf(fp,"Snapshot taken\r\n");
	}
	return 0;
}


/* DESCRIPTION: Camera traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
/*
 Same functionality as configurable_tlc
 But also handles Red-light camera
 */
void camera_tlc(int* state)
{
	// Call the configurable tlc
	configurable_tlc(state);

	// Check if a car has entered, react according to which state we're in
	handle_vehicle_button();

	// If a car entered in green light set a timer when it changes to yellow
	if(vehicle_detected == 2 && (*state == 2 || *state == 5)) {
		// Stop the timer and start it again
		alt_alarm_stop(&camera_timer);
		alt_alarm_start(&camera_timer, CAMERA_TIMEOUT, camera_timer_isr, &vehicle_detected);

		if(fp != NULL)fprintf(fp,"Camera activated\r\n");
		vehicle_detected = 1;
	}
}


/* DESCRIPTION: Simulates the entry and exit of vehicles at the intersection
 * PARAMETER:   none
 * RETURNS:     none
 */
void handle_vehicle_button(void)
{
	int millsec = 0;

	// If we've received a button press
	if(carButton) {
		// If no vehicle in the intersection
		if(vehicle_detected == 0) {
			car_time = clock();
			if(proc_state[3] == 0 || proc_state[3] == 3) {
				// If in red state start a long timer so that stopping it doesn't break the code
				alt_alarm_start(&camera_timer, 1000000, camera_timer_isr, &vehicle_detected);
				if(fp != NULL)fprintf(fp,"Snapshot taken\r\n");
				vehicle_detected = 1;
			} else if (proc_state[3] == 2 || proc_state[3] == 5) {
				// If in yellow state start the camera timer
				alt_alarm_start(&camera_timer, CAMERA_TIMEOUT, camera_timer_isr, &vehicle_detected);
				if(fp != NULL)fprintf(fp,"Camera activated\r\n");
				vehicle_detected = 1;
			} else {
				// Car enters on green start a long timer
				alt_alarm_start(&camera_timer, 1000000, camera_timer_isr, &vehicle_detected);
				if(fp != NULL)fprintf(fp,"Car passes on green\r\n");
				// Set to 2 so we can tell the timer to start when it changes to yellow
				vehicle_detected = 2;
			}

		// If a vehicle is in the intersection, check how long
		// it was there and tell the user.
		} else if (vehicle_detected) {
			vehicle_detected = 0;
			difference = clock()- car_time;
			millsec = difference * 1000 / CLOCKS_PER_SEC;
			alt_alarm_stop(&camera_timer);

			if(fp != NULL)fprintf(fp,"Vehicle Left, time: %d seconds %d milliseconds\r\n", millsec/1000, millsec%1000);
		}
		carButton = 0;
	}
}

int main(void)
{
	// Initialise the switches to zero
	unsigned int uiSwitchValue = 0;
	unsigned int previousSwitches = 0;
	unsigned int changeMode = 0;

	// Open the UART globally
	fp = fopen(UART_NAME,"r+");

	// initialize lcd to mode 1
	lcd_set_mode(0);

	// initialize buttons
	init_buttons_pio(buttons);

	while (1) {
		// Switch detection
		previousSwitches = uiSwitchValue;
		uiSwitchValue = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);

		// If the switch values have changed, raise the flag
		if ((previousSwitches & 0b11) != (uiSwitchValue & 0b11)) {
			usleep(1000); // wait for debouncing
			changeMode = 1;
		}

		// If we've changed switches and in a safe state, change the mode
		if (changeMode && (proc_state[mode] == 0 || proc_state[mode] == 3)) {
			changeMode = 0;
			proc_state[mode] = -1;
			mode = uiSwitchValue & 0b11;
			alt_alarm_stop(&tlc_timer);
		}

		// If sw2 is up, set flag to say we should ask for new values
		if (uiSwitchValue & 0b100) {
			timeoutFlag = 1;
		}

		// Execute the correct TLC
		switch (mode) {
		case 0:
			simple_tlc(&proc_state[0]);
			break;
		case 1:
			pedestrian_tlc(&proc_state[1]);
			break;
		case 2:
			configurable_tlc(&proc_state[2]);
			break;
		case 3:
			camera_tlc(&proc_state[3]);
			break;

		}
		// Update Displays
	}
	fclose(fp);
	return 1;
}
