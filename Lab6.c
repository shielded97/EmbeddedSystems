//Provide an interrupt handler that will start, stop, and clear a timer

/*
	Objectives:
	1) Create a working timer that responds to keypad input
*/

/*
	Functions:
	1) Main      		- endless loop after initializing variables to wait for interrupt
	2) initialize       - sets up all ports
	5) EXTI0_IRQHandler - handles user push button interrupts
*/	

#include "STM32L1xx.h"

/*
	Global Variables
*/

uint8_t total;
uint8_t tenths = 0;
uint8_t seconds = 0;
uint8_t arr_value = 900;
uint8_t psc_value = 233;
uint8_t running = 0;
uint8_t column_itr;
uint8_t row_itr;
uint8_t buttons[4][4] = {
		{1,2,3,0xA},
		{4,5,6,0xB},
		{7,8,9,0xC},
		{0xE,0,0xF,0xD}
	};
	
/*----------------------------------------------------------*/
/* Initialize function - initialize input and output pins */
	//TO-DO: initialize timer and stopwatch
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
	GPIOC->MODER &= ~(0x0000FFFF); 		/* Clear PC0-PC3 mode bits */
	GPIOC->MODER |= (0x00005555); 		/* General purpose output mode for PC[3:0]*/
	
	//Configure pull-up resistors for row pins (PB[3:0])
	GPIOB->PUPDR &= ~(0x000000FF);
	GPIOB->PUPDR |= 0x00000055;
	
	//Enable timer 10
	RCC->APB2ENR |= RCC_APB2ENR_TIM10EN;	//enable timer10 clk
	TIM10->PSC = psc_value;					//set correct prescaler value for interrupts every 0.1 sec
	TIM10->ARR = arr_value;					//set correct auto-reload value for interrupts every 0.1 sec
	TIM10->DIER |= 1;						//enable timer10 to signal an interrupt
	
	//Set SYSCFC Module
	SYSCFG -> EXTICR[0] &= 0xFF0F;
	
	//EXTI Config Module
	EXTI->FTSR |= 0x0002;
	EXTI->IMR  |= 0x0002;
	EXTI->PR   |= 0x0002;
	
	/* Set NVIC Module */
	NVIC_EnableIRQ(TIM10_IRQn); //enable timer10 interrupt 
	NVIC_EnableIRQ(EXTI0_IRQn); //enable specific Interrupts 
	
	//Enable Interrupts
	__enable_irq();
}

/*----------------------------------------------------------*/
/* Counting function - Counts up/down from 0-9 */

/*----------------------------------------------------------*/
void counting()
{
	//Set first counter
	if (tenths == 9)
	{
		tenths = 0;
		if (seconds == 9)
		{
			seconds = 0
			GPIOC->ODR &= 0xFF00; 	// Set the LEDs to display binary 0
		}
		else 
		{
			total = (seconds << 4) + (tenths);
			GPIOC->ODR &= total;
		}
	}
	else
	{
		tenths++;
		total = (seconds << 4) + (tenths);
		GPIOC->ODR &= total;
	}
}

//-------------------------------------------------
//Interrupt Handlers
//
//-------------------------------------------------

void EXTI0_IRQHandler()
{
	__disable_irq();
	
	column_itr = 0x1;
	row_itr = 0x1;
	int found = 0;
	int display;
	int k;
	int i;
	int col_count;
	int row_count;
	int row_value;

	for (k = 0; k < 200; k++);
	for (col_count = 1; col_count <= 4; col_count++) 
	{
		//drive individual columns low and check rows to see if they are low as well
		GPIOB->ODR &= 0xFF0F;
		GPIOB->ODR |= (~(column_itr) << 4);

			for (k = 0; k < 4; k++);
			row_value = GPIOB->IDR & 0x000F;
			if ( row_value == 14)
			{
				row_count = 1;
				found = 1;
			}
			else if ( row_value == 13)
			{
				row_count = 2;
				found = 1;
			}
			else if ( row_value == 11)
			{
				row_count = 3;
				found = 1;
			}
			else if ( row_value == 7)
			{
				row_count = 4;
				found = 1;
			}

		if (found == 1)  	//calculate what number was pressed using row_itr and column_itr
		{
			display = buttons[row_count-1][col_count-1];
			GPIOC->ODR = display;
			break;
		}
		if (found == 0)
		{
			column_itr = column_itr << 1;
		}
	}

	if (display != 0)				//when start/stop is pushed
	{
		running = (!(running) & 0x00000001);
		if (running == 1)
		{
			TIM10->CR1  |= TIM_CR1_CEN;		//if running, enable counter in control register
		}
		else 
		{
			TIM10->CR1 &= ~TIM_CR1_CEN;	//if not running, disable counter
		}
	}
	else if ((display == 0) && (running == 0))				//when reset(0) is pushed while counter is not running
	{
		TIM10->CNT &= 0x0000;	//clear tim10 count
		GPIOC->ODR &= 0x00000000; 			//clear output leds as well
	}
	
	GPIOB->ODR &= ~(0x00F0);  //Reset columns to 0
	EXTI->PR |= 0x0002;
	NVIC_ClearPendingIRQ(EXTI0_IRQn); 	//clear pending interrupt request since it's being handled
	
	//enable interrupts again
	__enable_irq();
}

void TIM10_IRQHandler()
{
	//disable interrupts during handling of current interrupt
	__disable_irq();
	
	counting();
	
	TIM10->SR &= ~TIM_SR_UIF;			//clear timer10's update interrupt flag
	NVIC_ClearPendingIRQ(TIM10_IRQn);	//clear pending flag
	
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
	{	}
}



//TKTKTK show prescaler/auto-reload calculations in notebook. Ps = 233 and ARR = 900
//Timer Status Register