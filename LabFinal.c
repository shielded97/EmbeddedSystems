//Provide an interrupt handler that will start, stop, and clear a timer

/*
	Objectives:
	1) COMPLETE: Working keypad interface to select speed
	2) COMPLETE: 10 different non-zero speeds
	3) COMPLETE: sampling period is 10ms +/- 1us
	4) POSSIBLY COMPLETE: feedback stabilization
	5) COMPLETE: stopwatch with start/stop/reset capabilities
	6) closed-loop step response settling time is 0.5 the open-loop settling time.
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
//___________________________________________________________________________
//Timer variables 
int arr_value = 65535;        
uint8_t psc_value = 1;
uint8_t tenths = 0;
uint8_t seconds = 0;
uint8_t running = 0;
uint8_t stopwatch_counter = 0;
//___________________________________________________________________________
//Amplitude measurement variables
double amplitude = 0;
int expectedAmp[10] = {{1000},{1322},{1644},{1967},
			{2289},{2611},{2933},{3256},{3578},{3900}};
int dutyArray[10] = {{15997},{21155},{26306},{36621},{41779},
					 {46930},{52094},{57245},{62396},{65535}};
double old_error = 0;
int old_duty = 0;
int counter = 0;
//___________________________________________________________________________
//Compensator variables
int expected;
//___________________________________________________________________________
//Keypad input variables
int display;
uint8_t total;
int adjustedDuty;
uint8_t column_itr;
uint8_t row_itr;
uint8_t buttons[4][4] = {
		{1,2,3,0xA},
		{4,5,6,0xB},
		{7,8,9,0xC},
		{0xE,0,0xF,0xD}
	};
//___________________________________________________________________________


//-------------------------------------------------
//ADC Initializer
//
//-------------------------------------------------
void ADCInitializer()
{
	//RCC->AHB1ENR |= 0x07; 						/* Enable ADC clock (bit 0) */
	GPIOA->MODER |= 0x0000C000;						//Set PA7 to analog mode
	RCC->CR |= RCC_CR_HSION;						//HSION = bit 0 of RCC->CR
	while ((RCC->CR & RCC_CR_HSIRDY) == 0);
	RCC->CFGR |= RCC_CFGR_SW_HSI;
	RCC->APB2ENR |= 0x200; 							/* Enable ADC digital interface (bit 9) */
	ADC1->CR1 &= ~(0x00000020);						//Disable end of conversion interrupt
	
	ADC1->CR2 |= 0x1;								//Power on ADC
	
	//Wait until ADONS = 1 in ADC1->SR
	while ((ADC1->SR & 0x40) == 0);
	
	ADC1->SQR5 &= ~(ADC_SQR5_SQ1);		//Clear channel selection 
	ADC1->SQR5 |= 0x00000007;			//Select ADC_IN7 as first channel
	
}

//-------------------------------------------------
//Pin Initializer
//
//-------------------------------------------------
void pinInitializer()
{
	//pin setup
	/* Configure PA1 as input pin */
	RCC->AHBENR |= 0x07; 				/* Enable GPIOA clock (bit 0) */
	GPIOA->MODER &= ~(0x0000300C);      /* Sets PA1 to input mode and clear PA6 */
	
	//Configure PA6 as Alternate Function for PWM (set pin 6 to %10)
	GPIOA->MODER |= 0x00002000;
	GPIOA->AFR[0] &= ~0x0F000000;
	GPIOA->AFR[0] |= 0x03000000;	
	
	//Configure PA7 as Alternate Function for PWM (set pin 7 to %10)
	GPIOA->MODER |= 0x00008000;
	GPIOA->AFR[0] |= 0x30000000;
	
	/* Configure PB[3:0] as input pins - these are rows */
	RCC->AHBENR |= 0x02; 							/* Enable GPIOB clock (bit 0) */
	GPIOB->MODER &= ~(0x000000FF);     	 			/* Sets PB[3:0] to input mode */
	
	/* Configure PB[7:4] as output pins - these are columns */
	GPIOB->MODER &= ~(0x0000FF00); 					/* Clear PB4-PB7 mode bits */
	GPIOB->MODER |= (0x00005500); 					/* General purpose output mode for PC[7:4]*/
	
	/* Configure PC[7:0] as output pins to drive LEDs for stopwatch */
	RCC->AHBENR |= 0x04; 							/* Enable GPIOC clock (bit 2) */
	GPIOC->MODER &= ~(0x0000FFFF); 					/* Clear PC0-PC7 mode bits */
	GPIOC->MODER |= (0x00005555); 					/* General purpose output mode for PC[3:0]*/
	
	//Configure pull-up resistors for row pins (PB[3:0])
	GPIOB->PUPDR &= ~(0x000000FF);
	GPIOB->PUPDR |= 0x00000055;
	
	//Configure pull-up resistors for PA7 for Comparator chip
	GPIOA->PUPDR &= ~(0x0000C000);
	GPIOA->PUPDR |= 0x00004000;
	
	//Configure PA2 for use in TIM9 Channel 1
	//GPIOA->MODER &= ~(0x00000030);	//clear PA2
	//GPIOA->MODER |= 0x00000020;		//set PA2 to alternate function (pin 2 to %10)
	//GPIOA->AFR[0] |= 0x00000300;
}

//-------------------------------------------------
//Interrupt Initializer
//
//-------------------------------------------------
void interruptInitializer()
{
	//Set SYSCFC Module
	SYSCFG -> EXTICR[0] &= 0xFF0F;
	
	//EXTI Config Module
	EXTI->FTSR |= 0x0002;
	EXTI->IMR  |= 0x0002;
	EXTI->PR   |= 0x0002;
	
	/* Set NVIC Module */
	NVIC_SetPriority(EXTI1_IRQn, 1);
	NVIC_SetPriority(TIM11_IRQn, 0);
	//NVIC_SetPriotity(TIM9_IRQn, 2);
	
	NVIC_EnableIRQ(TIM11_IRQn); //enable timer11 interrupt
	//NVIC_EnableIRQ(TIM6_IRQn);  //enable timer9 interrupt 
	NVIC_EnableIRQ(EXTI1_IRQn); //enable specific Interrupts 
	
	NVIC_ClearPendingIRQ(EXTI1_IRQn); /* Clear pending flag */
	NVIC_ClearPendingIRQ(TIM11_IRQn);
	//NVIC_ClearPendingIRQ(TIM6_IRQn);
	
	//Enable Interrupts
	__enable_irq();
}

//-------------------------------------------------
//TIM6 Initializer
//
//-------------------------------------------------
//void tIM6Initializer()
//{
//	//Timer 6 setup
//	RCC->APB2ENR |= RCC_APB2ENR_TIM6EN;										//enable timer6 clk
//	TIM6->PSC = 500;													//set correct prescaler value for interrupts every 0.1 sec
//	TIM6->ARR = 32000;													//set correct auto-reload value for interrupts every 0.1 sec
//	TIM6->DIER |= 1;														//enable timer6 to signal interrupt
//}

//-------------------------------------------------
//TIM10 Initializer
//
//-------------------------------------------------
void tIM10Initializer()
{
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
	
	TIM10->CR1  |= 0x01;				  									//enable timer10
}

//-------------------------------------------------
//TIM11 Initializer
//
//-------------------------------------------------
void tIM11Initializer()
{
	//Timer 11 setup
	RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
	TIM11->PSC = 999;													//set correct prescaler value for interrupts
	TIM11->ARR = 159;													//set correct auto-reload value for interrupts double the size of TIM11's for accurate sampling
		
	TIM11->DIER |= TIM_DIER_UIE;										//enable timer11 to signal an interrupt
	TIM11->CR1  |= 0x01;				  								//enable timer11

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
			seconds = 0;
			GPIOC->ODR &= 0xFF00; 	// Set the LEDs to display binary 0
		}
		else 
		{
			seconds = seconds + 1;
			total = (seconds << 4) + (tenths);
			GPIOC->ODR &= 0x0;
			GPIOC->ODR |= total;
		}
	}
	else
	{
		tenths++;
		total = (seconds << 4) + (tenths);
		GPIOC->ODR &= 0x0;
		GPIOC->ODR |= total;
	}
	
	stopwatch_counter = 0; //reset counter to trigger every 0.1 seconds
}

//-------------------------------------------------
//Keypad Button Identifier
//
//-------------------------------------------------
int getKeypadValue()
{
	column_itr = 0x1;
	row_itr = 0x1;
	int found = 0;
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
			if ( row_value == 14) {
				row_count = 1;
				found = 1;
			}
			else if ( row_value == 13) {
				row_count = 2;
				found = 1;
			}
			else if ( row_value == 11) {
				row_count = 3;
				found = 1;
			}
			else if ( row_value == 7) {
				row_count = 4;
				found = 1;
			}

		if (found == 1) {  	//calculate what number was pressed using row_itr and column_itr
			display = buttons[row_count-1][col_count-1];
			break;
		}
		if (found == 0) {
			column_itr = column_itr << 1;
		}
	}
	
	GPIOB->ODR &= ~(0x00F0);  //Reset columns to 0
	return display;			//return the button pushed on the keypad
}

//-------------------------------------------------
//Change Duty Cycle
//
//-------------------------------------------------
int updateDuty(int display)
{
	//Change duty cycle
	switch(display) {
		case 0:
			adjustedDuty = 0;
			total = 0;
			break;
		case 1:
			adjustedDuty = (arr_value * 0.2441);
			total = 1;
			break;
		case 2:
			adjustedDuty = (arr_value * 0.3228);
			total = 2;
			break;
		case 3:
			adjustedDuty = (arr_value * 0.4014);
			total = 3;
			break;
		case 4:
			adjustedDuty = (arr_value * 0.5588);
			total = 4;
			break;
		case 5:
			adjustedDuty = (arr_value * 0.6375);
			total = 5;
			break;
		case 6:
			adjustedDuty = (arr_value * 0.7161);
			total = 6;
			break;
		case 7:
			adjustedDuty = (arr_value * 0.7949);
			total = 7;
			break;
		case 8:
			adjustedDuty = (arr_value * 0.8735);
			total = 8;
			break;
		case 9:
			adjustedDuty = (arr_value * 0.9521);
			total = 9;
			break;
		case 10:
			adjustedDuty = (arr_value * 1);
			total = 10;
			break;
		case 11:
			running = (!(running) & 0x00000001);
			//if (running == 1) {
			//	TIM6->CR1  |= 0x01;				//if running, enable counter in control register
			//}
			//else {
			//	TIM6->CR1 &= ~0x01;				//if not running, disable counter
			//}
			break;
		case 12:
			if (running == 0)						//when reset(0) is pushed while counter is not running
			{
				//TIM6->CNT &= 0x0000;				//clear tim6 count
				//GPIOC->ODR &= 0x00000000; 			//clear output leds as well
				tenths = 0;
				seconds = 0;
			}
		case 13:
			break;
		case 14:
			break;
		case 15:
			break;
		default:
			adjustedDuty = display;
			break;
	}
	/////////////////////////////////////////////////////////////////////
	//UPDATE TIM10_CCR1 and display new duty on LEDs
	TIM10->CCR1 = adjustedDuty;
	return expected;
	/////////////////////////////////////////////////////////////////////
}

//-------------------------------------------------
//Compensator 
//
//-------------------------------------------------
void compensator() {
	//take ADC sample
	//Run PID calculations
	//Calculate new duty cycle
	
	//read in amplitude
	//convert to duty
	//calculate error on duty
	//run PID
	//scale duty
	//update duty
	
	double ampRatio = amplitude / expectedAmp[total];
	
	int current_duty = round(arr_value * ampRatio);
	
	int error = dutyArray[total] - current_duty; 
	
	//use PID to compute scale factor for accel/decel
	double scale = (5.36057366997635 * error) 
		+ (169.435259818658 * ((error + old_error) / 2)) 
		+ (0.01164969583744 * ((error - old_error) / 0.01);
	
	//update duty cycle with scale factor
	int new_duty = round(current_duty * scale);
	//updateDuty(new_duty);
	updateDuty(round(scale));
	
	old_error = error;
}

//-------------------------------------------------
//Keypad Interrupt Handler
//
//-------------------------------------------------
void EXTI1_IRQHandler()
{	
	display = getKeypadValue();
	expected = updateDuty(display);
	
	//get ready to calculate Tim11 period again by clearing all relevant variables
	//counter = 0;
	
	for (int i = 0; i < 4000; i++);		//add delay to aid in reading accuracy
	EXTI->PR |= 0x0002;
	
	NVIC_ClearPendingIRQ(EXTI1_IRQn); 	//clear pending interrupt request since it's being handled
	
}
//-------------------------------------------------
//Tim6 Interrupt Handler
//
//-------------------------------------------------
//void TIM6_IRQHandler()
//{	
//	counting();
//	
//	TIM6->SR &= ~TIM_SR_UIF;			//clear timer9's update interrupt flag
//}

//-------------------------------------------------
//Tim10 Interrupt Handler
//
//-------------------------------------------------
void TIM10_IRQHandler()
{	
	TIM10->SR &= ~0x00000002;			
	TIM10->SR &= ~TIM_SR_UIF;			//clear timer10's update interrupt flag
}

//-------------------------------------------------
//Tim11 Interrupt Handler
//
//-------------------------------------------------
void TIM11_IRQHandler()
{	
	//Sample once and call compensator
	ADC1->CR2 |= ADC_CR2_SWSTART;		//Software trigger
	for(int n=0;n<10;n++);				//substitute for waiting for conversion to finish
	amplitude = ADC1->DR;				//uncorrected amplitude signal
	compensator();						//update duty cycle with PID system

	//run counting every 0.1sec without implementing new timer
	if ((stopwatch_counter == 9) && (running == 1)) {
		counting();
	}
	stopwatch_counter++;				//increment counter so that counting() runs every 0.1sec
	
	TIM11->SR &= ~TIM_SR_UIF;			//clear timer11's update interrupt flag
	NVIC_ClearPendingIRQ(TIM11_IRQn);	//clear pending flag
}

/*----------------------------------------------------------*/
/* Initialize function - initialize input and output pins */

/*----------------------------------------------------------*/
void initialize() 
{	
	pinInitializer();
	tIM10Initializer();
	tIM11Initializer();
	interruptInitializer();
	ADCInitializer();
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



 