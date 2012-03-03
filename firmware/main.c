/* Name: main.c
 * Author: Harry Johnson
 * License: CC BY/SA Uses hardware and concepts from Leon Maurer's Mini Random Number Generator: http://tinyurl.com/DIYrandomgen
 * Automatic D&D dice rolling, different dice values, truly random, woo-hoo! (I hope...)
 * NOTE: CURRENTLY COMPLETELY UNTESTED!!
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define cycle 		randomNumber = (randomNumber >> 1) | ((((randomNumber >> 0) ^ (randomNumber >> 3)) & 1) << 24)
//this does the linear feedback shift on pool and mixes in a radnom bit
#define mixInBit(bit)	randomNumber = (randomNumber >> 1) | (((bit ^ (randomNumber >> 0) ^ (randomNumber >> 3)) & 1) << 24)
#define COUNTER_HIGH 0x7A //maximum counter value.
#define COUNTER_LOW 0x12 
//Function declarations
void setupRandomGenerator(); //sets up clock for random number generator.
void setupADC(); //set up registers for the ADC
uint8_t readAnalogSwitch(); //read analog value from the switches, and parse it, returning 1-6 to indicate which button is pressed.

void enableSevenSegment(); //set up all of the 7-segment display pins.
void disableSevenSegment(); //set all the 7-segment display pins to high-impedance outputs.
void disableTensPlace(); //not needed for 1-digit numbers.
void enableTensPlace();

void displayValue(uint8_t value); //display a given 2-digit value
void displayTensPlace(uint8_t value); //display 0-9 in the ten's place.
void displayOnesPlace(uint8_t value); //display 0-9 in the one's place
void rollAndDisplay(uint8_t die); //roll a value and display it, and reset the counter.



//variable declarations
const uint8_t displayMap[10] = {~0x3f,~0x06, ~0x5B, ~0x4F, ~0x66, ~0x6D, ~0x7D, ~0x07, ~0x7F, ~0x6F}; //7-segment display (abcdefg) mapping. Common anode: 0 is on. 
const uint8_t highNibbleValues[6] = {15, 13, 10, 7, 5, 2}; //top 4 bits of ADC.
const uint8_t dieMax[6] = {4, 6, 8, 10, 12, 20}; //6 values of max dice count.
volatile uint32_t randomNumber=0b1010101010101010101010101; //32 bit random number, always being refreshed.
volatile uint8_t lastbit = 255; //what the last input digit was, or -1 if we want to force a start-over. (after outputting a bit)
volatile uint8_t currentlyDisplaying = 0; //whether the LED display is on or not. 

int main(void)
{
    WDTCR |= (1<<WDCE); //enable changes to watchdog timer register
    WDTCR &= ~(1<<WDE); //disable watchdog timer.
    setupADC(); //ADC register definitions, etc.
    setupRandomGenerator(); //timer register setup
    sei(); //enable interrupts;
    disableSevenSegment(); //make sure we aren't driving the pins.
    uint8_t switch_value; //value of the read switch.
    for(;;) {
      switch_value = readAnalogSwitch();
        if((switch_value != 255) && (currentlyDisplaying==0))rollAndDisplay(switch_value); 
    }
    
    return 0;   /* never reached */
}


ISR(TIMER0_OVF_vect) { //triggering every 32 microseconds or so. 
    if((lastbit != ((PIND&4)>>2)) && (lastbit != 255)) { //if there are 2 bits, and they aren't equal
        mixInBit(lastbit);
        lastbit = 255; //and start the sequence of capturing another 2 bits.
    } else {
        lastbit = (PIND>>2)&1; //capture the first bit
    }
}

ISR(TIMER1_COMPA_vect) { //LED timeout, every 4 seconds or so.
    TCNT1H = 0; //reset the timer 1 counter.
    TCNT1L = 0;
    disableSevenSegment(); //turn off the display.
    currentlyDisplaying = 0; //and note that we're not currently displaying anything.
}

void setupRandomGenerator() {
    DDRD &= ~(1<<2); //PORTD2 input.
    PORTD &= ~(1<<2); //with no pull-up resistor.
    
    TCNT0 = 0; //reset counter 0
    TCNT1H = 0; //reset the timer 1 counter.
    TCNT1L = 0; 
    
    OCR1AH = COUNTER_HIGH; //16 bit maximum counter. 
    OCR1AL = COUNTER_LOW; 
    
    TCCR0 = 1; // internal clock, no prescaling for random number updater.
    TCCR1A = 0; //no PWM, etc.
    TCCR1B = (1<<CS12) | (1<<CS10); // clock/1024 prescalar for 7-segment timeout.
    
    TIMSK = (1<<TOIE0) | (1<<OCIE1A); //enable overflow register for counter 0. Output compare register on counter 1;
}



void setupADC() {
    ADMUX = 7; //reading ADC7, where switches run in
    ADMUX &= ~((1<<REFS1) | (1<<REFS0)); //reference 00, external AREF
    ADMUX |= (1<<ADPS2) | (1<<ADPS1) | (1<<ADLAR); //internal clock/64. 125Khz.
    ADCSRA = (1<<ADEN); //enable the ADC.
}

uint8_t readAnalogSwitch() {
    uint8_t analogResult = 0; //result of the analog conversion.
    ADCSRA |= (1<<ADSC); //start conversion
    while((ADCSRA & ADSC)>0); //loop while conversion goes.
    analogResult = (ADCH &0xF0) >> 4; //4 top bits, shifted left.
    uint8_t ii; //iteration counter.
    for(ii=0; ii<6; ii++) { 
        if(analogResult == highNibbleValues[ii]) return dieMax[ii]; //if the right switch is pressed, return the maximum die number. 
    }
    return 255; //no switch pressed.
}

//7-segment LED code.
void rollAndDisplay(uint8_t die) { //roll a value, display it on the screen, and reset the timing counter.
    currentlyDisplaying = 1;
    enableSevenSegment();
    TCNT1H = 0; //reset the timer 1 counter.
    TCNT1L = 0;
    displayValue((randomNumber%die)+1); //generate a new number and display it.
}

void enableSevenSegment() {
    DDRC |= 0x3F; //PC0-PC5 outputs; 
    DDRB |= 0x7F; //PB0-PB6 outputs;
    DDRD |= 0x80; //PD7 output, otherwise inputs;
}

void disableSevenSegment() {
    DDRC &= ~0x3F; //disable everything! 
    DDRB &= ~0x7F; //note: Because it's common anode, even if the pull-up resistors are enabled, it doesn't matter, LED still off. 
    DDRD &= ~0x80;
}

void disableTensPlace() { //disable just the ten's place: If it's a single digit number, we don't need ten's place.
    DDRB &= ~0x7F;
}

void enableTensPlace() {
    DDRB |= 0x7F; //PB0-PB6 outputs
}
void displayValue(uint8_t value) { //display a 2-digit value on the seven segment.
    if(value<100) { //2 digit display, check just in case.
        uint8_t lowdigit = value % 10; //just the one's place 0-9
        uint8_t highdigit = (value - lowdigit)/10; //ten's place 0-9
        
        displayTensPlace(highdigit); //display the high digit in the ten's place
        displayOnesPlace(lowdigit); //display the low digit in the one's place.
    }
}

void displayTensPlace(uint8_t value) { //display the value in the ten's place.
    if((value<10) && (value>0)) { 
        enableTensPlace();
        PORTB = (displayMap[value]&0x7F); //direct port mapping, yay!
    } else {
        disableTensPlace();
    }
}

void displayOnesPlace(uint8_t value) { //display the value in the one's place.
    if(value<10) {
        PORTC = (displayMap[value] & 0x3F);
        PORTD |= (displayMap[value] & 0x40)<<1; //original on bit 6, output on bit 7. If original bit isn't there and we want it to be, put it there.
        PORTD &= (0x7F | ((displayMap[value] & 0x40)<<1)); //if original bit is there and we don't want it to be, unflag it. All while not touching other pins.
    }
}
