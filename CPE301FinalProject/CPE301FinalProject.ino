// Author: Abby Battaglia, Jasmine Huang, Marlo Ongkingco, Mark Mitzen
// Date: 5/10/24
// Title: CPE 301 Final Project

// included libraries
#include <LiquidCrystal.h>
#include <DHT11.h>
#include <Stepper.h>
#include <Clock.h>

// macros
#define RDA 0x80
#define TBE 0x20

#define WATER_LEVEL_MIN 0
#define WATER_LEVEL_MAX 521 // TEMP, need to setup based on max value during water sensor calibration

#define TEMPERATURE_THRESHOLD 240 // integer temperature at which to turn on fan

// set up LCD pins
const int RS = 13, EN = 12, D4 = 11, D5 = 10, D6 = 9, D7 = 8;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

bool motorState = false;

// set up DHT pins
DHT11 dht11(7); // DHT sensor connected to digital pin 7

//define pointers for port B, used for LEDs
volatile unsigned char* port_b = (unsigned char*) 0x25;
volatile unsigned char* ddr_b = (unsigned char*) 0x24;


// set up motor
const int stepsPerRevolution = 2038;
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
Stepper myStepper = Stepper(stepsPerRevolution, 1, 3, 2, 4);

// set up water sensor pin and define water level variable
volatile int waterValue = 0;
int waterLevel = 0;

int temperature = 0;
int humidity = 0;

// initialize LED status
int status = 0;

// UART Pointers
volatile unsigned char *myUCSR0A = 0x00C0;
volatile unsigned char *myUCSR0B = 0x00C1;
volatile unsigned char *myUCSR0C = 0x00C2;
volatile unsigned int *myUBRR0 = 0x00C4;
volatile unsigned char *myUDR0 = 0x00C6;

// ADC Pointers
volatile unsigned char *my_ADMUX = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *)0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *)0x78;

//timer Pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;

void setup()
{
  // put your setup code here, to run once:
  U0Init(9600);
  adc_init();
  MyClock.Init();
  lcd.begin(16, 2); // set up number of columns and rows
  status = 1; // system is disabled, yellow LED should be on
  statusLED(status);
}

void loop()
{
  // put your main code here, to run repeatedly:
  //
  // if start button pressed
  //
  status = 2; // system idle, green LED should be 
  statusLED(status);

  temperature = dht11.readTemperature(); // read the temperature
  humidity = dht11.readHumidity();       // read the humidity
  getWaterLevel(); // get the water level
  lcdDisplay(); // display the temperature and humidity

  // if water level is too low
  if (waterLevel < 2)
    {
      motorState = false;
      outputStateChange(String("Motor is off "));
    }

  // check temperature and restart fan motor accordingly

  if (temperature > TEMPERATURE_THRESHOLD && !motorState) {
    motorState = true;
    //fanMotor();
    outputStateChange(String("Fan motor is on "));
  }

  else if (motorState) {
    motorState = true;
    //fanMotor();
    outputStateChange(String("Fan motor is off "));
  }

  //
  // if stop button pressed
  //
  status = 1; //system disabled, yellow LED should be on
  statusLED(status);
}

void lcdDisplay()
{
  // this function sets up the the lCD

  lcd.clear(); // clear the lcd

  // print the temperature, prints "Temperature: # C"
  lcd.setCursor(0, 0); // move cursor to (0, 0)
  lcd.print("Temperature: ");
  lcd.print(temperature);
  lcd.print("C");

  // print the humidity, prints "Humidity: # %"
  lcd.setCursor(0, 1); // move cursor to (0, 1)
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print("%");

  delay(10); // delay for 1 minute
  // REPLACE WITH MILLIS() AT SOME POINT
}

void ventMotor(int direction)
{
    myStepper.setSpeed(5); // arbitrary speed in rpm
    myStepper.step(stepsPerRevolution * direction);
    myStepper.setSpeed(0);              // I think this turns it off (sets rpm to zero)
    myStepper.step(stepsPerRevolution); // don't know if this is still nessesary for turning it off
    // maybe delay here

  // a positive stepsPerRevolution is clockwise and negative is counter clockwise
  // essentially have direction be 1 for clockwise and -1 for counterclockwise
}

int getWaterLevel()
{
  // this function reads the water sensor
  waterValue = adc_read(0);                                             // mask
  waterLevel = map(waterValue, WATER_LEVEL_MIN, WATER_LEVEL_MAX, 0, 4); // 4 levels also need to setup max macro
  Serial.println(waterValue);
  Serial.println(waterLevel);
}

void statusLED(int statusLight)
{
  // this function sets up the status LEDs

  // YELLOW - PB0, pin 53, DISABLED
  // GREEN - PB1, pin 52, IDLE
  // BLUE - PB2, pin 51, RUNNING
  // RED - PB3, pin 50, ERROR

  *ddr_b |= 0x0F; // set pins PB0-PB3 as outputs
  *port_b |= 0x0F; // set pins PB0-PB3 to enable pullup

  if (statusLight == 1) //disabled
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB0 high (yellow)
    *port_b |= (*port_b & 0xF0) | 0x01;
  }
  else if (statusLight == 2) //idle
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB1 high (green)
    *port_b |= (*port_b & 0xF0) | 0x02;
  }
  else if (statusLight == 3) //running
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB2 high (blue)
    *port_b |= (*port_b & 0xF0) | 0x04;
  }
  else if (statusLight == 4) //error
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB3 high (red)
    *port_b |= (*port_b & 0xF0) | 0x08;
  }
}

//ADC functions
void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if (adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while ((*my_ADCSRA & 0x40) != 0)
    ;
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

//timer setup function
void setup_timer_regs()
{
  //set up the timer control registers
  *myTCCR1A= 0x00;
  *myTCCR1B= 0X00;
  *myTCCR1C= 0x00;
  //reset the TOV flag
  *myTIFR1 |= 0x01;
  //enable the TOV interrupt
  *myTIMSK1 |= 0x01;
}
//timer overflow ISR
ISR(TIMER1_OVF_vect)
{
  //Stop the Timer
  *myTCCR1B &= 0xF8;
  //Load the Count
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  //Start the Timer
  *myTCCR1B |= 0b00000001;
  //if it's not the STOP amount
  if(currentTicks != 65535)
  {
    //XOR to toggle PB6
    *portB ^= 0x40;
  }
}

// UART functions
void U0Init(int U0baud)
{
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  // Same as (FCPU / (16 * U0baud)) - 1;
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0 = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while ((*myUCSR0A & TBE) == 0)
    ;
  *myUDR0 = U0pdata;
}

void outputStateChange(String state)
{
  for (int i = 0; i < state.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar(' ');

  String date = String(MyClock.GetDateStr());
  String hour = String(MyClock.Hour());
  String minute = String(MyClock.Minute());

  for (int i = 0; i < date.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar(' ');

  for (int i = 0; i < Hour.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar(':');

  for (int i = 0; i < minute.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar('\n');
}
