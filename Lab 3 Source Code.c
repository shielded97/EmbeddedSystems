/*
	Objectives:
	1) decade up/down counter (modulo-10)
	2) controlled by two switches, S1 and S2
		a) S1: 1=start, 0=stop
		b) S2: 1=count down, 0=count up
		c) S1: PA1 to DIO0
		d) S2: PA2 to DIO1
	3) dynamic direction change during runtime
	4) Display count value by writing to port pins PC[3:0]
	5) 0.5 second delay between counts
*/

/*
	Functions:
	1) Main     - endless loop after initializing variables to call delay, set direction, and call counting
	2) Delay    - 0.5 second delay
	3) Counting - increment or decrement count and display on LEDs
*/	

#include "STM32L1xx.h"

/*
	Global Variables
*/

uint8_t count1;
uint8_t count2;
uint8_t led_control;


/*----------------------------------------------------------*/
/* Delay function - do nothing for about 0.5 seconds */

/*----------------------------------------------------------*/
void delay () {
	int i,j,n;
	for (i=0; i<10; i++) 
	{ 							//outer loop
		for (j=0; j<20000; j++) 
		{ 						//inner loop
			n = j; 				//dummy operation for single-step test
		} 						//do nothing
	}
}

/*----------------------------------------------------------*/
/* Counting function - Counts up/down from 0-9 */

/*----------------------------------------------------------*/
void counting (int direction)
{
	if (direction == 0)			//count up for count1 and down for count2
	{
		//Set first counter
		if (count1 == 9)
		{
			count1 = 0;
			GPIOC->ODR = 0x0000; 	// Set the LEDs to display binary 0
		}
		else
		{
			count1++;
			GPIOC->ODR = count1;
		}
		//Set second counter
		if (count2 == 0)
		{
			count2 = 9;
			GPIOC->ODR = 0x0091;	// Set the LEDs to display binary 9
		}
		else
		{
			count2--;
			led_control = count2 << 4;
			led_control = led_control + count1;
			GPIOC->ODR = led_control;
		}
	}
	else 						//count down for count1 and up for count2
	{
		//Set first counter
		if (count1 == 0)
		{
			count1 = 9;
			GPIOC->ODR = 0x0009;	// Set the LEDs to display binary 9
		}
		else
		{
			count1--;
			GPIOC->ODR = count1;
		}
		//Set second counter
		if (count2 == 9)
		{
			count2 = 0;
			GPIOC->ODR = 0x0000; 	// Set the LEDs to display binary 0
		}
		else
		{
			count2++;
			led_control = count2 << 4;
			led_control = led_control + count1;
			GPIOC->ODR = led_control;
		}
	}
}

/*----------------------------------------------------------*/
/* Main function - initializes port directions and variables */
/*		- Provides endless loop to count                       */
/*----------------------------------------------------------*/
int main(void)
{
	unsigned char sw1;
	unsigned char sw2;
	/*unsigned char led1; //state of LED1
	unsigned char led2; //state of LED2
	unsigned char led3; //state of LED3
	unsigned char led4; //state of LED4 */
	count1 = 0;
	count2 = 0;
	
	//pin setup
	
	/* Configure PA1 and PA2 as input pins*/
	RCC->AHBENR |= 0x01; 				        /* Enable GPIOA clock (bit 0) */
	GPIOA->MODER &= ~(0x0000003C);      /* Sets PA1 and PA2 to input mode */
	
	/* Configure PC0, PC1, PC2, and PC3 as output pins to drive LEDs */
	RCC->AHBENR |= 0x04; 				      /* Enable GPIOC clock (bit 2) */
	GPIOC->MODER &= ~(0x0000FFFF); 		/* Clear PC0-PC7 mode bits */
	GPIOC->MODER |= (0x00005555); 		/* General purpose output mode for PC[7:0]*/
	
	while (1)
	{
		delay();
		
		sw1 = (GPIOA->IDR & 0x02) >> 1; 						//Read GPIOA and mask all but bit 1
		sw2 = (GPIOA->IDR & 0x04) >> 2;            //Read GPIOA and mask all but bit 2
	
		if (sw1 == 1)
		{
			counting(sw2);
		}
	}
}



