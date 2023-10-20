#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define NUMBER_OF_COMPARE_MTACHES_PER_SECOND 1

// Additional Requirement To Restart Stop-Watch After 24 hours
#define TWENTY_FOUR_HOURS_LIMIT 0

// Global Variables
unsigned char g_tick = 0;
unsigned char g_SEC1 = 0 , g_SEC2 = 0;
unsigned char g_MIN1 = 0 , g_MIN2 = 0;
unsigned char g_HRS1 = 0 , g_HRS2 = 0;

// Prototypes - Functions for Displaying Time
void Display_Hours(void);
void Display_Minutes(void);
void Display_Seconds(void);

/*-------------------------------------- TIMER1 CONFIGURATION & ISR HANDLING ---------------------------------------*/

/* Timer1 Configuration */
/*
 * F_Timer = 1MHz/1024(from Pre-scaler) = 977 Hz
 * T_Timer = 1/977 = 1024usec
 * T_Compare = Compare Value * 1024usec
 * As I need one interrupt ( one compare match ) per second, so T_Compare = 1
 * 1 = Compare Value * 1024usec
 * Compare Value = 1/1024usec = 976.5
 * Compare Value = 977
 */
void Timer1_Init_CTC(void)
{
	TCNT1   = 0;                  						  // Timer1 begins from 0
	OCR1A   = 977;	              						  // Compare value = 977 to generate 1 interrupt per second
	TIMSK  |= (1<<OCIE1A);							   	  // Timer1 compare match interrupt is enabled
	TCCR1A  = (1<<FOC1A);			                      // CTC (non-PWM) Mode
	TCCR1B  = (1<<WGM12) | (1<<CS10) | (1<<CS12);         // Enable Compare Match Mode with F_CPU/1024 from pre-scaler
	SREG   |= (1<<7);  				                      // Enable interrupts by setting I-bit
}

// Function to handle seconds (The first two 7-Segments)
void Display_Seconds(void)
{
	g_SEC1++;          			    // Increment Seconds Units Till 9 then roll-back to 0
	if (g_SEC1 == 10)
	{
		g_SEC1 = 0;
		g_SEC2++;					// Begin to Increment Seconds Tenth
		if (g_SEC2 == 6)			// After 60 Seconds ( 6 at Tenth and 0 at Units ) begin to increment Minutes
		{
			g_SEC2 = 0;
			Display_Minutes();
		}
	}
}

// Function to handle minutes (The middle two 7-Segments)
void Display_Minutes(void)
{
	g_MIN1++;						// Increment Minutes Units Till 9 then roll-back to 0
	if (g_MIN1 == 10)
	{
		g_MIN1 = 0;
		g_MIN2++;					// Begin to Increment Minutes Tenth
		if (g_MIN2 == 6)			// After 60 Minutes ( 6 at Tenth and 0 at Units ) begin to increment Hours
		{
			g_MIN2 = 0;
			Display_Hours();
		}
	}
}

// Function to handle minutes (The last two 7-Segments)
void Display_Hours(void)
{
	g_HRS1++;						// Increment Hours Units Till 9 then roll-back to 0
	if (g_HRS1 == 10)
	{
		g_HRS1 = 0;
		g_HRS2++;				    // Begin to Increment Hours Tenth
	}
// Additional Requirement To Restart After 24 hours
#if(TWENTY_FOUR_HOURS_LIMIT)
	else if (g_HRS2 == 2 && g_HRS1 == 4)
	{
		g_HRS1 = g_HRS2 = 0;		// Begin from 0 again after 24 hours
	}
#endif
}

/* Timer1 Interrupt Service Routine - Interrupt Per Second */
ISR(TIMER1_COMPA_vect)
{
	g_tick++;

	if(g_tick == NUMBER_OF_COMPARE_MTACHES_PER_SECOND)
	{
		Display_Seconds();			// Increment one second for each Timer1 interrupt
	}
	g_tick = 0;                     // Clear The Tick Counter to Start a New Second
}

/*------------------------------------- EXTERNAL INTERRUPTS - BUTTONS TRIGGERED -------------------------------------*/

/* External INT0 enable and configuration function */
void INT0_Init(void)
{
	DDRD  &= ~(1<<PD2);            			 // Configure INT0/PD2 as input pin
	PORTD |= (1<<PD2);						 // Enable Internal pull-up resistor
	MCUCR |= (1<<ISC01);
	MCUCR &= ~(1<<ISC00);		    		 // Trigger INT0 with the falling edge
	GICR  |= (1<<INT0);            			 // Enable external interrupt pin INT0
	SREG  |= (1<<7);            		   	 // Enable interrupts by setting I-bit
}

/* External INT0 Interrupt Service Routine - To RESET Stop-Watch */
ISR(INT0_vect)
{
	g_SEC1 = g_SEC2 = g_MIN1 = g_MIN2 = g_HRS1 = g_HRS2 = 0;		   // Begin from 0 again
}

/* External INT1 enable and configuration function */
void INT1_Init(void)
{
	DDRD  &= ~(1<<PD3); 					 // Configure INT1/PD3 as input pin
	MCUCR |= (1<<ISC11) | (1<<ISC10); 	   	 // Trigger INT1 with the raising edge
	GICR  |= (1<<INT1);                      // Enable external interrupt pin INT1
	SREG  |= (1<<7);                         // Enable interrupts by setting I-bit
}

/* External INT1 Interrupt Service Routine - To Pause Stop-Watch */
ISR(INT1_vect)
{
	TCCR1B &= ~(1<<CS12) & ~(1<<CS11) &~(1<<CS10);  // No Clock Source for the timer - Timer Stopped (Paused)

}
/* External INT2 enable and configuration function */
void INT2_Init(void)
{
	DDRB   &= ~(1<<PB2);                    // Configure INT2/PB2 as input pin
	PORTB  |= (1<<PB2);		                // Enable Internal pull-up resistor
	MCUCSR &= ~(1<<ISC2);                   // Trigger INT2 with the falling edge
	GICR   |= (1<<INT2);	                // Enable external interrupt pin INT2
	SREG   |= (1<<7);                       // Enable interrupts by setting I-bit
}

/* External INT2 Interrupt Service Routine - To Resume Stop-Watch */
ISR(INT2_vect)
{
	TCCR1B  = (1<<WGM12) | (1<<CS10) | (1<<CS12);  // Clock Source F_CPU/1024 (from pre-scaler) - Timer begins (Resumed)
}

/*------------------------------------------------- MAIN FUNCTION -------------------------------------------------*/
int main(void)
{
	DDRC  |= 0x0F;   // Set lower 4 bits of PORTC as outputs
	PORTC &= 0xF0;   // Display zero at first
	DDRA  |= 0x3F;   // Set lower 6 bits of PORTA as outputs for enable/disable control

	Timer1_Init_CTC();
	INT0_Init();
	INT1_Init();
	INT2_Init();

	/* Always Display The 6 Multiplexed 7 Segments  */
	while(1)
	{
	    // Display Seconds Units
		PORTA = (1<<PA0);
		PORTC = (PORTC & 0xF0) | (g_SEC1 & 0x0F);
		_delay_ms(3);

	    // Display Seconds Tenth
		PORTA = (1<<PA1);
		PORTC = (PORTC & 0xF0) | (g_SEC2 & 0x0F);
		_delay_ms(3);

	    // Display Minutes Units
		PORTA = (1<<PA2);
		PORTC = (PORTC & 0xF0) | (g_MIN1 & 0x0F);
		_delay_ms(3);

	    // Display Minutes Tenth
		PORTA = (1<<PA3);
		PORTC = (PORTC & 0xF0) | (g_MIN2 & 0x0F);
		_delay_ms(3);

	    // Display Hours Units
		PORTA = (1<<PA4);
		PORTC = (PORTC & 0xF0) | (g_HRS1 & 0x0F);
		_delay_ms(3);

	    // Display Hours Tenth
		PORTA = (1<<PA5);
		PORTC = (PORTC & 0xF0) | (g_HRS2 & 0x0F);
		_delay_ms(3);	 // Small Delay After Each Display For Multiplexing
	}
}
