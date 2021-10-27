//One counter incrementing once per second from 0 to 9 and over again
//Once interrupt is detected, identify and display keypad # for 5 sec while 
//still incrementing count in background

/*
	Objectives:
	1) Handle interrupts from User Push Button and Digital Push/Pull Switch
	2) Enable/Disable Interrupts from GPIO, SYSCFC, EXTI, NVIC, CPU
	
*/

/*
	Functions:
	1) Main     - endless loop after initializing variables to call delay, set direction, and call counting
	2) Delay    - 1 second delay
	3) Counting1 - increment count and display on LEDs
	4) Counting2 - increment or decrement count and display on LEDs
	5) EXTI0_IRQHandler - handles user push button interrupts
	6) EXTI1_IRQHandler - handles Waveforms push/pull switch interrupts
*/	

#include "STM32L1xx.h"

/*
	Global Variables
*/

uint8_t count1;
uint8_t led_control;
uint8_t interrupt_dir;
uint8_t column_itr;
uint8_t row_itr;
uint8_t buttons[4][4] = {
		{1,2,3,0xA},
		{4,5,6,0xB},
		{7,8,9,0xC},
		{14,0,15,0xD}
	};
	
/*----------------------------------------------------------*/
/* Initialize function - initialize input and output pins */

/*----------------------------------------------------------*/
void initialize() {	
	count1 = 0;
	
	//pin setup
	/* Configure PA1 as input pin */
	RCC->AHBENR |= 0x01; 				/* Enable GPIOA clock (bit 0) */
	GPIOA->MODER &= ~(0x0000000C);      /* Sets PA1 to input mode */
	
	/* Configure PB[3:0] as input pins - these are rows */
	RCC->AHBENR |= 0x02; 				/* Enable GPIOB clock (bit 0) */
	GPIOB->MODER &= ~(0x000000FF);      /* Sets PB[3:0] to input mode */
	
	/* Configure PB[7:4] as output pins - these are columns */
	GPIOB->MODER &= ~(0x0000FF00); 		/* Clear PC4-PC7 mode bits */
	GPIOB->MODER |= (0x00005500); 		/* General purpose output mode for PC[7:4]*/
	
	/* Configure PC[3:0] as output pins to drive LEDs*/
	RCC->AHBENR |= 0x04; 				/* Enable GPIOC clock (bit 2) */
	GPIOC->MODER &= ~(0x000000FF); 		/* Clear PC0-PC3 mode bits */
	GPIOC->MODER |= (0x00000055); 		/* General purpose output mode for PC[3:0]*/
	
	//Set SYSCFC Module
	SYSCFG -> EXTICR[1] &= 0xFF00;
	
	//EXTI Config Module
	EXTI->RTSR |= 0x0003;
	EXTI->IMR  |= 0x0003;
	EXTI->PR   |= 0x0003;
	
	/* Set NVIC Module */
	NVIC_EnableIRQ(EXTI0_IRQn); //enable specific Interrupts 
	
	//Enable Interrupts
	__enable_irq();
}

/*----------------------------------------------------------*/
/* Delay function - do nothing for about 1 second */

/*----------------------------------------------------------*/
void delay () {
	int i,j,n;
	
	for (i=0; i<20; i++) 
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
void counting1(int display)
{
	delay();
	//Set first counter
	if (count1 == 9)
	{
		count1 = 0;
		if (display == 1)
		{
			GPIOC->ODR &= 0xFFF0; 	// Set the LEDs to display binary 0
		}
	}
	else
	{
		count1++;
		if (display == 1)
		{
			GPIOC->ODR &= 0xFFF0;
			GPIOC->ODR |= count1;
		}
	}
}

//-------------------------------------------------
//Interrupt Handlers
//
//-------------------------------------------------

void EXTI0_IRQHandler()
{
	__disable_irq();
	
	column_itr = 1;
	row_itr = 1;
	int found = 0;
	int display;
	
	for (column_itr = 1; column_itr >= 8; column_itr << 1) 
	{
		//drive individual columns low and check rows to see if thay are low as well
		GPIOB->ODR &= 0xFF0F;
		GPIOB->ODR |= ~(column_itr) << 4;
		for (row_itr = 1; row_itr >= 8; column_itr << 1)
		{
			if (GPIOB->IDR | ~(row_itr) != GPIOB->IDR)
			{
				found = 1;
				break;
			}
		}
		if (found = 1)  	//calculate what number was pressed using row_itr and column_itr
		{
			display = buttons[row_itr][column_itr];
			GPIOC->ODR = display;
			break;
		}
	}

	//delay for five seconds while displaying keypress and incrementing count
	for (i = 0; i < 5; i++)
	{
		counting1(0);
	}
	
	EXTI->PR |= 0x0001;
	NVIC_ClearPendingIRQ(EXTI0_IRQn); 	//clear pending interrupt request since it's being handled
	
	//enable interrupts again
	__enable_irq();
}


/*----------------------------------------------------------*/
/* Main function - initializes port directions and variables */
/*		- Provides endless loop to count                       */
/*----------------------------------------------------------*/
int main(void)
{
	initialize();	
	while (1)
	{
		counting1(1);
	}
}



