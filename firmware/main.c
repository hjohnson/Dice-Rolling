/* Name: main.c
 * Author: Harry Johnson
 * Firmware file for the DigiDice, produced and sold by kippkitts, LLC (http://kippkitts.com)
 * Source files currently available at https://github.com/hjohnson
 * License: CC BY/SA 
 * Uses hardware and concepts from Leon Maurer's Mini Random Number Generator: http://tinyurl.com/DIYrandomgen
 * Electronic die to replace the six standard sizes of dice used in most games.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define cycle 		randomNumber = (randomNumber >> 1) | ((((randomNumber >> 0) ^ (randomNumber >> 3)) & 1) << 24)
//this does the linear feedback shift on pool and mixes in a radnom bit
#define mixInBit(bit)	randomNumber = (randomNumber >> 1) | (((bit ^ (randomNumber >> 0) ^ (randomNumber >> 3)) & 1) << 24)
#define COUNTER_HIGH 0x7A //maximum counter value. Used to set the display timeout. 
#define COUNTER_LOW 0x12 

//Function declarations
void setupRandomGenerator(); //sets up clock for random number generator.
void setupIO();
void setupSevenSegment();

void enableSevenSegment(); //set up all of the 7-segment display pins.
void disableSevenSegment(); //set all the 7-segment display pins to high-impedance outputs.

void displayValue(uint8_t value); //display a given 2-digit value
void displayTensPlace(uint8_t value); //display 0-9 in the ten's place.
void displayOnesPlace(uint8_t value); //display 0-9 in the one's place
void rollAndDisplay(uint8_t die); //roll a value and display it, and reset the counter.



//variable declarations
const uint8_t displayMap[10] = {~0x3f,~0x06, ~0x5B, ~0x4F, ~0x66, ~0x6D, ~0x7D, ~0x07, ~0x7F, ~0x6F}; //7-segment display (gfedcba) mapping. Common anode: 0 is on. 
const uint8_t dieMax[6] = {20, 12, 10, 8, 6, 4}; //6 values of max dice count.
volatile uint32_t randomNumber=0b1010101010101010101010101; //32 bit random number, always being refreshed.
volatile uint8_t lastbit = 255; //what the last input digit was, or -1 if we want to force a start-over. (after outputting a bit)

volatile uint8_t currentlyDisplaying = 0; //what value the LED is currently displaying. 
volatile uint8_t whichDigit = 0; //0 for one's place, 1 for ten's place.

int main(void)
{
    clock_prescale_set(0); 
    WDTCSR |= (1<<WDCE) |(1<<WDE); //enable changes to watchdog timer register
    WDTCSR = 0; //disable watchdog timer.
    cli(); //temporarily disable interrupts.
    power_adc_disable(); //power down a load of stuff to save power.
    power_spi_disable();
    power_twi_disable();
    power_usart0_disable();
    setupIO(); //switches, seven segment display pin.
    setupRandomGenerator(); //timer register setup
    setupSevenSegment(); //timers for the seven-segment displays
    disableSevenSegment(); //make sure we aren't driving the pins
    sei(); //enable interrupts;
    for(;;) {
    
    }
    return 0;   /* never reached */
}


ISR(TIMER0_OVF_vect) { //triggering every 32 microseconds or so. 
    if((lastbit != ((PIND>>3)&1)) && (lastbit != 255)) { //if there are 2 bits, and they aren't equal
        mixInBit(lastbit);
        lastbit = 255; //and start the sequence of capturing another 2 bits.
    } else {
        lastbit = (PIND>>3)&1; //capture the first bit
    }
}

ISR(TIMER1_COMPA_vect) { //LED timeout, every 4 seconds or so. turn off the display and put everything back to sleep.
    TCNT1H = 0; //reset the timer 1 counter.
    TCNT1L = 0;
    TIMSK2 &= ~(1<<TOIE2);
    disableSevenSegment(); //turn off the display.
    PCIFR |= (1<<PCIF0);
    PCICR |= (1<<PCIE0); //re-enable the pin change interrupt to allow another switch.
    currentlyDisplaying = 0; //and note that we're not currently displaying anything.
    clock_prescale_set(6); //drop the clock speed down to 125Khz while the the device is off to save power.
}

ISR(TIMER2_OVF_vect) { //triggering on 120Hz.
    if(whichDigit == 0) { //display the one's place
        displayOnesPlace(currentlyDisplaying);
        whichDigit = 1; //display the ten's place next.
    } else { //display the ten's place
        displayTensPlace(currentlyDisplaying);
        whichDigit = 0; //display the one's place next.
    }
}

ISR(PCINT0_vect) { //one of the switches was pressed. (high to low)
    PCICR &= ~(1<<PCIE0); //disable the pin change interrupt (debouncing hack).
    PCIFR |= (1<<PCIF0); //clear the flag so it doesn't retrigger on bounce, just in case.
    clock_prescale_set(0); //bring the clock speed back up to the full 8MHz. 
    uint8_t ii = 0;
    while(ii<6) { //which of the switches was pressed?
        if((PINB & (1<<ii)) == 0) {//active low pin
            rollAndDisplay(dieMax[ii]);
        }
    ii++;
    }
}


void setupIO() {
    DDRB = 0; //all of port B input
    PORTB = 0x3F; //turn on the pull-up resistors on the first 6 lines (switches)
    PCMSK0 = 0x3F; //turn on pin change interrupts for the first 6 lines.
    PCICR = (1<<PCIE0); //Enable that pin change interrupt bus.
    
    //Seven-Segment displays. 
    PORTC &= ~(0x3F); //PC0-5 low. 
    DDRC |= 0x3F; //PC0-PC5 outputs; 
    DDRD |= 0x07; //PD0-2 output, otherwise inputs;
    PORTD &= ~(0x07); //bring everything low, so no displays will be on. 
}

void setupRandomGenerator() {
    DDRD &= ~(1<<3); //PORTD3 input.
    PORTD &= ~(1<<3); //with no pull-up resistor.
    TCNT0 = 0; //reset counter 0
    TCCR0B = 1; // internal clock, no prescaling for random number updater.
    TIMSK0 |= (1<<TOIE0); //enable overflow register for counter 0. Output compare register on counter 1;
}

//7-segment LED code.
void rollAndDisplay(uint8_t die) { //roll a value, display it on the screen, and reset the timing counter.
    displayValue((randomNumber%die)+1); //generate a new number and display it.
}

void setupSevenSegment() {
    TCNT1H = 0; //reset the timer 1 counter.
    TCNT1L = 0; 
    
    OCR1AH = COUNTER_HIGH; //16 bit maximum counter. 
    OCR1AL = COUNTER_LOW; 
    TCCR1A = 0; //no PWM, etc.
    TCCR1B = (1<<CS12) | (1<<CS10); // clock/1024 prescalar for 7-segment timeout.
    TIMSK1 |= (1<<OCIE1A);
    
    TCCR2A = 0;
    TCCR2B = (1<<CS22) | (1<<CS21); //main frequency divided by 256. With additional div by 256 gives 122 Hz update rate, more than fast enough.
}
              
void enableSevenSegment() {    
    TCNT1H = 0; //reset the timer 1 counter: 4s countdown.
    TCNT1L = 0;
    TIMSK2 |= (1<<TOIE2); //enable timer 2: POV timer
    TCNT2 = 0;
}

void disableSevenSegment() {
    PORTC = 0;
    PORTD &= ~0x07;
    TIMSK2 &= ~(1<<TOIE2); //disable the timer running the display.
}
              
void displayValue(uint8_t value) { //display a 2-digit value on the seven segment.
    enableSevenSegment();
    
    if(value<100) { //2 digit display, check just in case.
        currentlyDisplaying = value;
    } 
}

void displayTensPlace(uint8_t value) { //display the value in the ten's place.
    uint8_t bitMask = displayMap[(currentlyDisplaying-(currentlyDisplaying%10))/10];
    
    PORTD &= ~(1<<2); //turn off the one's place
    PORTD |= (1<<1); //turn on the ten's place.
    
    PORTC = bitMask & 0x3F; //write the output value to the pins.
    PORTD |= (bitMask >>6)& 0x01;
    PORTD &= (0xFE | ((bitMask >>6)& 0x01));
}

void displayOnesPlace(uint8_t value) { //display the value in the one's place.
    uint8_t bitMask = displayMap[currentlyDisplaying%10];
    
    PORTD |= (1<<2); //turn on the one's place
    PORTD &= ~(1<<1); //turn off the ten's place.
    
    PORTC = bitMask & 0x3F;
    PORTD |= (bitMask >>6)& 0x01;
    PORTD &= (0xFE | ((bitMask >>6)& 0x01));
}
