/* Name: main.c
 * Author: Harry Johnson
 * License: CC BY/SA unported. Uses hardware and concepts from Leon Maurer's Mini Random Number Generator:tinyurl.com/DIYrandomgen
 * Automatic D&D dice rolling, different dice values, truly random, woo-hoo! (I hope...)
 * NOTE: CURRENTLY COMPLETELY UNTESTED!!
*/

#include <avr/io.h>
#include <avr/interrupt.h>

void displayForTime();
void setupRandomGenerator(); //sets up clock for random number generator.
void setupADC(); //set up registers for the ADC
void enableSevenSegment(); //set up all of the 7-segment display pins.
void disableSevenSegment(); //set all the 7-segment display pins to high-impedance outputs.
uint8_t readAnalogSwitch(); //read analog value from the switches, and parse it, returning 1-6 to indicate which button is pressed.
void displayValue(uint8_t value); //display a given 2-digit value
void displayTensPlace(uint8_t value); //display 0-9 in the ten's place.
void displayOnesPlace(uint8_t value); //display 0-9 in the one's place
uint8_t rollDice(uint8_t die); //take the current random number and parse it into the proper number. 

const uint8_t displayMap[10] = {~0x7e,~0x30, ~0x6d, ~0x79, ~0x33, ~0x5b, ~0x5f, ~0x70, ~0x7f, ~0x7b}; //7-segment display (abcdefg) mapping. Common anode: 0 is on. 
volatile uint16_t randomNumber; //16 bit random number, always being refreshed.

volatile uint8_t whichbit = 0; //which bit of the two-bit von neumann sequence we are on.
volatile uint8_t lastbit = 0; //what the last input digit was


int main(void)
{
WDTCR |= (1<<WDCE); //enable changes to watchdog timer register
WDTCR &= ~(1<<WDE); //disable watchdog timer.
setupADC();
setupRandomGenerator();
 for(;;){
switch(readAnalogSwitch()) {
case '1': enableSevenSegment(); displayValue(rollDice(20)); displayForTime(); disableSevenSegment(); break; //roll a D20, display for 4 seconds.
case '2': enableSevenSegment(); displayValue(rollDice(12)); displayForTime(); disableSevenSegment(); break; //roll a D12
case '3': enableSevenSegment(); displayValue(rollDice(10)); displayForTime(); disableSevenSegment(); break; //D10
case '4': enableSevenSegment(); displayValue(rollDice(8)); displayForTime(); disableSevenSegment(); break; //D8
case '5': enableSevenSegment(); displayValue(rollDice(6)); displayForTime(); disableSevenSegment(); break; //D6
case '6': enableSevenSegment(); displayValue(rollDice(4)); displayForTime(); disableSevenSegment(); break; //D4
}       
    }
    return 0;   /* never reached */
}

ISR(TIMER0_OVF_vect) {
if(whichbit == 1) { // if on second bit
if(lastbit != ((PORTD&4)>>2)) { //if the bits aren't equal, then throw it into the shift register.
uint16_t temp = randomNumber & 0xFFFE;  
randomNumber = temp | lastbit; 
} 
whichbit = 0; //start sequence again. If the bits were equal, above doesn't execute, and we start getting another 2 bits.
} else { //still on first bit.
lastbit = ((PORTD&4)>>2);
whichbit = 1; //now onto the second bit.
}
TIFR |= (1<<TOV0); //reset the timer vector.
}


void displayForTime() {
uint32_t ii = 0;
while(ii<32000000) {
ii++;
} 
}

void setupRandomGenerator() {
TCCR0 = 1; // internal clock, no prescaling.
TCNT0 = 0; //reset counter 0
TIMSK = 1; //enable overflow register for counter 0;
sei(); //enable interrupts;
}

uint8_t rollDice(uint8_t die) {
return (randomNumber % die)+1; //Sadly, no dice have 0 as a side.
}

void enableSevenSegment() {
DDRC |= 0x3F; //PC0-PC5 outputs; 
DDRB |= 0x7F; //PB0-PB6 outputs;
DDRD = 0x80; //PD7 output, otherwise inputs;
}

void disableSevenSegment() {
DDRC &= ~0x3F; //disable everything!
DDRB &= ~0x7F;
DDRD &= ~0x80;
}

void setupADC() {
ADCSRA = (1<<ADEN);
ADMUX |= (7 && 0x0F); //read ADC7, where switches run in
ADMUX &= ~((1<<REFS1) | (1<<REFS0)); //reference 00, AREF
}

uint8_t readAnalogSwitch() {
uint16_t analogResult;
uint16_t lowCheck;
uint16_t highCheck;
ADCSRA |= (1<<ADSC);//start conversion
while((ADCSRA & ADSC)==1); //loop while conversion goes.
analogResult = ADCL;
analogResult |= (ADCH << 8);

uint16_t ii = 1;
while(ii<=6) {
lowCheck = ((ii-1)*(1024/6)+(ii)*(1024/6))/2; //midpoint on low side
highCheck = ((ii)*(1024/6)+ (ii+1)*(1024/6))/2; //midpoint on high side.
if((analogResult>lowCheck) && (analogResult<highCheck)) return ii; //if the right switch is pressed, return that number. 
ii++; //next switch to test
}
return 0; //no switch pressed.
}

void displayValue(uint8_t value) { //display a 2-digit value on the seven segment.
if(value<100) { //2 digit display, just in case.
uint8_t lowdigit = value % 10; //just the one's place 0-9
uint8_t highdigit = (value - lowdigit)/10; //ten's place 0-9
displayTensPlace(highdigit); //display the high digit in the ten's place
displayOnesPlace(lowdigit); //display the low digit in the one's place.
}
}

void displayTensPlace(uint8_t value) {
if(value<10) { 
PORTB = (displayMap[value]&0x7F); //direct port mapping, yay!
}
}

void displayOnesPlace(uint8_t value) {
if(value<10) {
PORTC = (displayMap[value] & 0x3F);
PORTD |= (displayMap[value] & 0x40)<<1; //original on bit 6, output on bit 7. If original bit isn't there and we want it to be, put it there.
PORTD &= (0x7F | ((displayMap[value] & 0x40)<<1)); //if original bit is there and we don't want it to be, unflag it. All while not touching other pins.
}
}
