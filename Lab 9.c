//Provide an interrupt handler that will start, stop, and clear a timer

/*
	Objectives:
	1) Generate a Pulse-width Modulated signal with scalable widths
*/

/*
	Functions:
	1) Main      		- endless loop after initializing variables to wait for interrupt
	2) initialize       - sets up all ports
	3) EXTI0_IRQHandler - handles user push button interrupts
	4) TIM10_IRQHandler - handles timer interrupts
	5) TIM11_IRQHandler - handles timer interrupts
*/	

//TO-DO
/*
	Write interrupt for TIM11
*/

#include "STM32L1xx.h"

/*
	Global Variables
*/

uint8_t total;
uint8_t tenths = 0;
uint8_t seconds = 0;
int arr_value = 1048;        
uint8_t psc_value = 1;

//Tachometer measurement variables
int capture = 0;
int rps[10];
int counter = 0;
double sum;
double averageValue;
//

int adjustedDuty;
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

/*----------------------------------------------------------*/
void initialize() {	
	
	//pin setup
	/* Configure PA1 as input pin */
	RCC->AHBENR |= 0x01; 				/* Enable GPIOA clock (bit 0) */
	GPIOA->MODER &= ~(0x0000300C);      /* Sets PA1 to input mode */
	
	//Configure PA6 as Alternate Function for PWM (set pin 6 to %10)
	GPIOA->MODER |= 0x00002000;
	GPIOA->AFR[0] &= ~0x0F000000;
	GPIOA->AFR[0] |= 0x03000000;	
	
	//Configure PA7 as Alternate Function for PWM (set pin 7 to %10)
	GPIOA->MODER |= 0x00008000;
	GPIOA->AFR[0] |= 0x10000000;
	
	//Timer 10 setup
	RCC->APB2ENR |= RCC_APB2ENR_TIM10EN;									//enable timer10 clk
	TIM10->PSC = psc_value;													//set correct prescaler value for interrupts every 0.1 sec
	TIM10->ARR = arr_value;													//set correct auto-reload value for interrupts every 0.1 sec
	TIM10->CCR1 &= ~0x00000001;   											//initialize duty cycle to 0
	TIM10->CCER |= TIM_CCER_CC1E;											//Set CC1E high and CC1P low
	TIM10->CCER &= ~(TIM_CCER_CC1P);					
	TIM10->CCMR1 &= ~(TIM_CCMR1_CC1S);				
	TIM10->CCMR1 &= ~(TIM_CCMR1_OC1M);										//clear OC1M and CS1S
	TIM10->CCMR1 |= (TIM_CCMR1_OC1M_2) | (TIM_CCMR1_OC1M_1);				//set OC1M to 110
	
	//TKTKTKTKTK
	//Timer 11 setup
	RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
	TIM11->PSC = psc_value;													//set correct prescaler value for interrupts
	TIM11->ARR = (2 * arr_value);											//set correct auto-reload value for interrupts double the size of TIM10's for accurate sampling
	TIM11->CCR1 &= ~0x00000001;   											//initialize duty cycle to 0
	TIM11->CCER |= TIM_CCER_CC1E;											//Set CC1E high and CC1P low
	TIM11->CCER &= ~(TIM_CCER_CC1P);					
	TIM11->CCER |= 0x2;														//Set CC1P/CC1NP to 01 for Rising Edge Capture
	TIM11->CCMR1 &= 0xFFFFFF00;
	TIM11->CCMR1 |= 0x00000001; //RIGHT HERE Change the filter if receiving signals that are really just noise. Change [7:4]
	
	/* Configure PB[3:0] as input pins - these are rows */
	RCC->AHBENR |= 0x02; 								/* Enable GPIOB clock (bit 0) */
	GPIOB->MODER &= ~(0x000000FF);     	 				/* Sets PB[3:0] to input mode */
	
	/* Configure PB[7:4] as output pins - these are columns */
	GPIOB->MODER &= ~(0x0000FF00); 					/* Clear PC4-PC7 mode bits */
	GPIOB->MODER |= (0x00005500); 					/* General purpose output mode for PC[7:4]*/
	
	/* Configure PC[3:0] as output pins to drive LEDs*/
	RCC->AHBENR |= 0x04; 							/* Enable GPIOC clock (bit 2) */
	GPIOC->MODER &= ~(0x0000FFFF); 					/* Clear PC0-PC3 mode bits */
	GPIOC->MODER |= (0x00005555); 					/* General purpose output mode for PC[3:0]*/
	
	//Configure pull-up resistors for row pins (PB[3:0])
	GPIOB->PUPDR &= ~(0x000000FF);
	GPIOB->PUPDR |= 0x00000055;
	
	//Configure pull-up resistors for PA7 for Comparator chip
	GPIOA->PUPDR &= ~(0x00003000);
	GPIOA->PUPDR |= 0x00002000;
	
	//Enable Timer10 and Timer11
	TIM10->CR1  |= 0x01;				  			//enable timer10
	TIM10->DIER |= 1;								//enable timer10 to signal an interrupt
	TIM11->CR1  |= 0x01;							//enable timer11
	TIM11->DIER |= 1;   							//enable timer11 to signal an interrupt
	
	//Set SYSCFC Module
	SYSCFG -> EXTICR[0] &= 0xFF0F;
	
	//EXTI Config Module
	EXTI->FTSR |= 0x0002;
	EXTI->IMR  |= 0x0002;
	EXTI->PR   |= 0x0002;
	
	/* Set NVIC Module */
	NVIC_SetPriority(TIM10_IRQn, 0);
	NVIC_SetPriority(EXTI1_IRQn, 1);
	NVIC_EnableIRQ(TIM10_IRQn); //enable timer10 interrupt 
	NVIC_EnableIRQ(EXTI1_IRQn); //enable specific Interrupts 
	
	//Enable Interrupts
	__enable_irq();
}

//-------------------------------------------------
//Interrupt Handlers
//
//-------------------------------------------------

void EXTI1_IRQHandler()
{
	
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
			break;
		}
		if (found == 0)
		{
			column_itr = column_itr << 1;
		}
	}
	
	//Change duty cycle
	switch(display) {
		case 0:
			adjustedDuty = 0;
			total = 0;
			break;
		case 1:
			adjustedDuty = (arr_value * 0.1);
			total = 1;
			break;
		case 2:
			adjustedDuty = (arr_value * 0.2);
			total = 2;
			break;
		case 3:
			adjustedDuty = (arr_value * 0.3);
			total = 3;
			break;
		case 4:
			adjustedDuty = (arr_value * 0.4);
			total = 4;
			break;
		case 5:
			adjustedDuty = (arr_value * 0.5);
			total = 5;
			break;
		case 6:
			adjustedDuty = (arr_value * 0.6);
			total = 6;
			break;
		case 7:
			adjustedDuty = (arr_value * 0.7);
			total = 7;
			break;
		case 8:
			adjustedDuty = (arr_value * 0.8);
			total = 8;
			break;
		case 9:
			adjustedDuty = (arr_value * 0.9);
			total = 9;
			break;
		case 10:
			adjustedDuty = (arr_value + 1);
			total = 10;
			break;
		default:
			break;
	}
	
	
	/////////////////////////////////////////////////////////////////////
	//UPDATE TIM10_CCR1 and display new duty on LEDs
	TIM10->CCR1 = adjustedDuty;
	GPIOC->ODR &= 0x0000;
	GPIOC->ODR |= total;
	/////////////////////////////////////////////////////////////////////
	
	GPIOB->ODR &= ~(0x00F0);  //Reset columns to 0
	for (int i =0; i < 40000; i++);
	for (int i =0; i < 40000; i++);
	for (int i =0; i < 40000; i++);
	EXTI->PR |= 0x0002;
	NVIC_ClearPendingIRQ(EXTI1_IRQn); 	//clear pending interrupt request since it's being handled
	
}

void TIM10_IRQHandler()
{	
	TIM10->SR &= ~0x00000002;			//CHECK TKTKTKTk 
	TIM10->SR &= ~TIM_SR_UIF;			//clear timer10's update interrupt flag
}

void TIM11_IRQHandler()
{	
	capture = ((TIM11->CNT) & (0xFFF)); 	//Mask out extra digits. Keep three hex digits
	TIM11->CNT &= (0x0);					//Check # of 0s to be &ed with: this is the reset
	if (counter < 10) {
		rps[counter] = capture; 
		counter++;							//Add value to array
	}
	
	for (i = 0; i<10; i++)
	{
		sum = sum + rps[i];
	}
	
	averageValue = sum / 10;
	
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



//Don't forget to clear capture/compare interrupt flag in status register (CC1IF = 0) slide 14