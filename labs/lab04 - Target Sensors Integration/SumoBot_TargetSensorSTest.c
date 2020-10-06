
/*
 * SumoBot.c
 *
 *  Created on: Dec 20, 2014
 *      Author: gonzagasilvas (Sergio Murilo Gonzaga Silva)
 */

// Includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <MSOE/delay.c>
#include <MSOE_I2C/delay.h>
#include <MSOE_I2C/bit.h>
#include <MSOE_I2C/lcd.h>

// Global variables used
int LtargetSensor = 0;
int RtargetSensor = 0;
int LtargetSensorSmoother[20];
int RtargetSensorSmoother[20];
int LtargetSensorMagnitude;
int RtargetSensorMagnitude;
int lastTargetDetection = 0;
int LlineSensorMagnitude;
int RlineSensorMagnitude;
int onLine = 0;
int IRsignalRuler = 0;

// Initializes pins for their designed functions
void initPORTS(void);

// Initializes TIMER0 for generating the IR signal of the target sensors
void initTIMER0(void);

// Initializes TIMER1 for generating PWM signal for the motors
void initTIMER1(void);

// Prepares ADC module for the first conversion
void initADC(void);

// Initializes LCD display with fixed words on it
void initLCD(void);

// Returns a value gotten from ADC module
uint16_t getADCSample(void);

// Stops the left motor
void stopLMotor(void);

// Stops the right motor
void stopRMotor(void);

// Makes the left motor run in a defined intensity
void runLMotor(int aValue);

// Makes the right motor run in a defined intensity
void runRMotor(int aValue);

// Sets the left motor for forward moves
void forwardLMotor(void);

// Sets the right motor for forward moves
void forwardRMotor(void);

// Sets the left motor for backward moves
void backwardLMotor(void);

// Sets the right motor for backward moves
void backwardRMotor(void);

// Checks if the left target sensor is seeing a target (1) or not (0)
int readLTargetSensor(void);

// Checks if the right target sensor is seeing a target (1) or not (0)
int readRTargetSensor(void);

// Checks if the left line sensor is seeing a target (1) or not (0)
int readLLineSensor(void);

// Checks if the right line sensor is seeing a target (1) or not (0)
int readRLineSensor(void);

// Initializes variables reserved for measuring magnitude of sensor detections
void initMagnitudeSystem(void);

// Gets the magnitude detection on the left target sensor
int getLtargetSensorMagnitude(void);

// Gets the magnitude detection on the right target sensor
int getRtargetSensorMagnitude(void);

// Gets the magnitude detection on the left line sensor
int getLlineSensorMagnitude(void);

// Gets the magnitude detection on the right line sensor
int getRlineSensorMagnitude(void);

// Starts search spinning to the left
void searchLeft(void);

// Starts search spinning to the right
void searchRight(void);

// Attacks straight to a front target
void attack(void);

// Goes reverse
void reverse(void);

// Attacks slightly veering to the left
void attackLeft(void);

// Attacks slightly veering to the right
void attackRight(void);

// Stops
void stop(void);

// ISR activated for generating IR designed sequence
ISR(TIMER0_COMPA_vect)
{
	IRsignalRuler++;
	if(IRsignalRuler <= 384)
		PORTD &= ~(1 << 5);
	else
	{
		if(IRsignalRuler <= 416)
			PORTD ^= (1 << 5);
		else
		{
			LtargetSensor = readLTargetSensor();
			RtargetSensor = readRTargetSensor();
			IRsignalRuler = 0;
		}
	}
}

void main(void)
{
	// Initializations
	initPORTS();
	initADC();
	initTIMER0();
	initTIMER1();
	initLCD();
	initMagnitudeSystem();
	sei();

	// Delay
	lcd_printf("T SENSORS TEST");
	delay_ms(1500);

	// Main loop: Robot behavior logic
	while(1)
	{
		lcd_clear();
		lcd_home();

		if(getLtargetSensorMagnitude() > 6 && getRtargetSensorMagnitude() > 6)
		{
				lcd_printf("BOTH");
		}
		else
		{
			if(getLtargetSensorMagnitude() > 6)
			{
				lcd_printf("LEFT");
			}
			else
			{
				if(getRtargetSensorMagnitude() > 6)
				{
					lcd_printf("RIGHT");
				}
				else
				{
					lcd_printf("NONE");
				}
			}
		}

		delay_ms(50);
	}
}

// Initializes pins for their designed functions
void initPORTS(void)
{
	// Pins B1(OC1A) and B2(OC1B) as output
	DDRB |= (1 << 2) | (1 << 1);

	// Pins D5, D6 and D7 as output
	DDRD |= (1 << 7) | (1 << 6) | (1 << 5);

	// Pin D3 and D4 as input
	DDRD &= ~(1 << 4) & ~(1 << 3);
}

// Initializes TIMER0 for generating the IR signal of the target sensors
void initTIMER0(void)
{
	// Sets CTC mode (event when OCR0A overflows)
	TCCR0A |= (1 << 1);

	// Sets prescaler of TIMER0 to 1
	TCCR0B |= (1 << 0);
	TCCR0B &= ~(1 << 1);
	TCCR0B &= ~(1 << 2);

	// Initialization of OCR0A
	OCR0A   = 209;

	// Enables interruption on each TIMER0 event
	TIMSK0 |= (1 << 1);
}

// Initializes TIMER1 for generating PWM signal for the motors
void initTIMER1(void)
{
	// Fast PWM, 9-bit (TOP = 511)
	TCCR1A |= (1 << 1);
	TCCR1B |= (1 << 3);

	// N = 8, TIMER1 starts to count
	TCCR1B |= (1 << 1);
}

// Prepares ADC module for the first conversion
void initADC(void)
{
	// Sets AVcc as the reference of the system
    ADMUX  |= (1 << REFS0);

    // Sets 128 divider factor prescaler
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Enables the ADC
    ADCSRA |= (1 << ADEN);
}

// Initializes LCD display with fixed words on it
void initLCD(void)
{
	lcd_init();
	lcd_clear();
	lcd_home();
}

// Returns a value gotten from ADC module
uint16_t getADCSample(void)
{
	uint16_t sample;

	// Starts ADC conversion
    ADCSRA |= (1 << ADSC);

    // Waits until conversion is done
    while(ADCSRA & (1 << ADSC));

    // Fits ADCH and ADCL in a single variable of 16 bits
    sample = ADCL;
    sample = (ADCH << 8) + sample;

    // Returns the gotten value
    return sample;
}

// Stops the left motor
void stopLMotor(void)
{
	// OC1A pin disconnected
	TCCR1A &= ~(1 << 7);
}

// Stops the right motor
void stopRMotor(void)
{
	// OC1A pin disconnected
	TCCR1A &= ~(1 << 5);
}

// Makes the left motor run in a defined intensity
void runLMotor(int aValue)
{
	// OC1A pin on non-inverting mode
	TCCR1A |= (1 << 7);

	// Assigns the intensity value to OCR1A
	OCR1A = aValue;
}

// Makes the right motor run in a defined intensity
void runRMotor(int aValue)
{
	// OC1B pin on non-inverting mode
	TCCR1A |= (1 << 5);

	// Assigns the intensity value to OCR1B
	OCR1B = aValue;
}

// Sets the left motor for forward moves
void forwardLMotor(void)
{
	PORTD |= (1 << 7);
}

// Sets the right motor for forward moves
void forwardRMotor(void)
{
	PORTD |= (1 << 6);
}

// Sets the left motor for backward moves
void backwardLMotor(void)
{
	PORTD &= ~(1 << 7);
}

// Sets the right motor for backward moves
void backwardRMotor(void)
{
	PORTD &= ~(1 << 6);
}

// Checks if the left target sensor is seeing a target (1) or not (0)
int readLTargetSensor(void)
{
	if(((PIND >> 4) & 1) == 0)
		return 1;
	else
		return 0;

}

// Checks if the right target sensor is seeing a target (1) or not (0)
int readRTargetSensor(void)
{
	if(((PIND >> 3) & 1) == 0)
		return 1;
	else
		return 0;

}

// Checks if the left line sensor is seeing a target (1) or not (0)
int readLLineSensor(void)
{
	uint16_t ADCSample;

	ADMUX &= ~(1 << 0);
	ADCSample = getADCSample();

	if(ADCSample < 400)
		return 1;
	else
		return 0;
}

// Checks if the right line sensor is seeing a target (1) or not (0)
int readRLineSensor(void)
{
	uint16_t ADCSample;

	ADMUX |= (1 << 0);
	ADCSample = getADCSample();

	if(ADCSample < 400)
		return 1;
	else
		return 0;
}

// Initializes variables reserved for measuring magnitude of sensor detections
void initMagnitudeSystem(void)
{
	int i;

	for(i = 0; i < 20; i++)
	{
		LtargetSensorSmoother[i] = 0;
		RtargetSensorSmoother[i] = 0;
	}
}

// Gets the magnitude detection on the left target sensor
int getLtargetSensorMagnitude(void)
{
	int i;
	int magnitude = 0;

	for(i = 0; i < 19; i++)
		LtargetSensorSmoother[i] = LtargetSensorSmoother[i + 1];

	LtargetSensorSmoother[19] = LtargetSensor;

	for(i = 0; i < 20; i++)
	{
		if(LtargetSensorSmoother[i] == 1)
			magnitude++;
	}

	return magnitude;
}

// Gets the magnitude detection on the right target sensor
int getRtargetSensorMagnitude(void)
{
	int i;
	int magnitude = 0;

	for(i = 0; i < 19; i++)
		RtargetSensorSmoother[i] = RtargetSensorSmoother[i + 1];

	RtargetSensorSmoother[19] = RtargetSensor;

	for(i = 0; i < 20; i++)
	{
		if(RtargetSensorSmoother[i] == 1)
			magnitude++;
	}

	return magnitude;
}

// Gets the magnitude detection on the left line sensor
int getLlineSensorMagnitude(void)
{
	uint16_t sample;

	ADMUX &= ~(1 << 0);
	sample = getADCSample();

	return sample;
}

// Gets the magnitude detection on the right line sensor
int getRlineSensorMagnitude(void)
{
	uint16_t sample;

	ADMUX |= (1 << 0);
	sample = getADCSample();

	return sample;
}

// Starts search spinning to the left
void searchLeft(void)
{
	backwardLMotor();
	forwardRMotor();
	runLMotor(511);
	runRMotor(511);
}

// Starts search spinning to the right
void searchRight(void)
{
	forwardLMotor();
	backwardRMotor();
	runLMotor(511);
	runRMotor(511);
}

// Attacks straight to a front target
void attack(void)
{
	forwardRMotor();
	forwardLMotor();
	runLMotor(511);
	runRMotor(511);
}

// Goes reverse
void reverse(void)
{
	backwardRMotor();
	backwardLMotor();
	runLMotor(511);
	runRMotor(511);
}

// Attacks slightly veering to the left
void attackLeft(void)
{
	forwardRMotor();
	forwardLMotor();
	runLMotor(411);
	runRMotor(511);
}

// Attacks slightly veering to the right
void attackRight(void)
{
	forwardRMotor();
	forwardLMotor();
	runLMotor(511);
	runRMotor(411);
}

// Stops
void stop(void)
{
	stopLMotor();
	stopRMotor();
}
