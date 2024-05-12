// Author: Abby Battaglia, Jasmine Huang, Marlo Ongkingco, Mark Mitzen
// Date: 5/10/24
// Title: CPE 301 Final Project

// included libraries
#include <LiquidCrystal.h>
#include <DHT11.h>
#include <Stepper.h>
// #include <Clock.h>
#include <DS3231.h>
#include <Wire.h>

// macros
#define RDA 0x80
#define TBE 0x20

// operational state
#define ERROR 3
#define IDLE 2
#define RUNNING 1
#define DISABLED 0

#define WATER_LEVEL_MIN 0
#define WATER_LEVEL_MAX 521 // TEMP, need to setup based on max value during water sensor calibration

#define TEMPERATURE_THRESHOLD 240 // integer temperature at which to turn on fan

// set up LCD pins
const int RS = 13, EN = 12, D4 = 11, D5 = 10, D6 = 9, D7 = 8;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

bool stepperMotorState = false;
bool fanMotorState = false;

// set up DHT pins
DHT11 dht11(7); // DHT sensor connected to digital pin 7

// set up clock object
DS3231 myRTC;

// define pointers for port B, used for LEDs
volatile unsigned char *port_b = (unsigned char *)0x25;
volatile unsigned char *ddr_b = (unsigned char *)0x24;

// Fan motor: controlled by digital pin 4 (PG5)
// Start (reset) button: controlled by digital pin 3 (PE5)
// Stop button: controlled by digital pin 2 (PE4)
volatile unsigned char *port_e = (unsigned char *)0x2E;
volatile unsigned char *ddr_e = (unsigned char *)0x2D;
volatile unsigned char *pin_e = (unsigned char *)0x2C;

volatile unsigned char* port_g = (unsigned char*) 0x34;
volatile unsigned char* ddr_g = (unsigned char*) 0x3D;

// Digital pin 22 (PA0) used for stepper motor input
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a = (unsigned char*) 0x21;
volatile unsigned char* pin_a = (unsigned char*) 0x20;


// set up motor
const int stepsPerRevolution = 1024; // 180 degree rotation
const int stepperSpeed = 10;
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
Stepper myStepper = Stepper(stepsPerRevolution, 29, 25, 27, 23);

// set up water sensor pin and define water level variable
volatile int waterValue = 0;
int waterLevel = 0;

int temperature = 0;
int humidity = 0;

// initialize LED status
int status = DISABLED;

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

// timer Pointers
volatile unsigned char *myTCCR1A = (unsigned char *)0x80;
volatile unsigned char *myTCCR1B = (unsigned char *)0x81;
volatile unsigned char *myTCCR1C = (unsigned char *)0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *)0x6F;
volatile unsigned char *myTIFR1 = (unsigned char *)0x36;
volatile unsigned int *myTCNT1 = (unsigned int *)0x84;

void setup()
{
  // put your setup code here, to run once:
  U0Init(9600);
  adc_init();
  Wire.begin();
  lcd.begin(16, 2);  // set up number of columns and rows
  status = DISABLED; // system is disabled, yellow LED should be on
  statusLED(status);

  //attachInterrupt(digitalPinToInterrupt(3), ISR, RISING); //attach ISR to start button current set to call when pressed but idk if thats what it is supposed to do
  // For attachInterrupt:
  // LOW to trigger the interrupt whenever the pin is low,
  // CHANGE to trigger the interrupt whenever the pin changes value
  // RISING to trigger when the pin goes from low to high,
  // FALLING for when the pin goes from high to low.

  // Setup fan motor DDR
  *ddr_a |= 0x04; // set PA2 as output

  // Setup button DDR
  *ddr_e &= 0xCF; // set PE4 and PE5 as input

  // Setup stepper motor DDR
  *ddr_a &= 0xFE; // set PA0 as input
}

void loop()
{
  // put your main code here, to run repeatedly:

  bool startButtonPressed = (*pin_e & 0x20) > 0; // PE5
  bool stopButtonPressed  = (*pin_e & 0x10) > 0; // PE4
  bool stepperButtonPressed = (*pin_a & 0x01) > 0; // PA0

  // STOP BUTTON
  if (stopButtonPressed && status != DISABLED)
  {
    status = DISABLED; // system disabled, yellow LED should be on
    statusLED(status);
    toggleFanState(DISABLED);
  }

  // START BUTTON
  if (startButtonPressed)
  {
    if (status == ERROR || status == DISABLED)
    {                // should do nothing in other states
      status = IDLE; // system idle, green LED should be
      statusLED(status);
      outputStateChange(status); // should output IDLE
    }
  }

  // ADJUST VENT
  if (stepperButtonPressed & status != ERROR) {
    ventMotor(1);
  }

  // MAIN SYSTEM LOOP
  if (status == RUNNING || status == IDLE)
  {
    temperature = dht11.readTemperature(); // read the temperature
    humidity = dht11.readHumidity();       // read the humidity
    getWaterLevel();                       // get the water level

    // if water level is too low
    if (waterLevel < 2)
    {
      status = ERROR;
      statusLED(status);
      toggleFanState(DISABLED);
      outputStateChange(status); //should output ERROR
      // ISR interrupt error
    }

    // check temperature and restart fan motor accordingly
    else if (temperature > TEMPERATURE_THRESHOLD && fanMotorState == DISABLED)
    {
      status = RUNNING;
      statusLED(status);
      toggleFanState(RUNNING);
      outputStateChange(status); //should output RUNNING
    }

    else if (temperature < TEMPERATURE_THRESHOLD && fanMotorState == RUNNING)
    {
      status = DISABLED;
      statusLED(status);
      toggleFanState(DISABLED);
      outputStateChange(status); //should output DISABLED
    }
  }

  lcdDisplay(); // display the temperature and humidity or error
  delay(1000);
}

void lcdDisplay()
{
  // this function sets up the the lCD

  if (status == RUNNING || status == IDLE)
  {
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

  else if (status == ERROR)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Water level");
    lcd.setCursor(0, 1);
    lcd.print("is too low");
  }

  else if (status == DISABLED)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Disabled");
  }
}

void ventMotor(int direction)
{
    myStepper.setSpeed(stepperSpeed); // arbitrary speed in rpm
    myStepper.step(stepsPerRevolution * direction);

  // a positive stepsPerRevolution is clockwise and negative is counter clockwise
  // essentially have direction be 1 for clockwise and -1 for counterclockwise
}

void toggleFanState(int state)
{
  // Send signal from digital pin 24 (PA2) to DC motor IN1
  if (state == 0) {
    *port_a |= 0x04;
    fanMotorState = 1;
    outputStateChange("Fan motor: STATE = ON \n");
  }

  else if (state == 1) {
    *port_a &= 0xFB;
    fanMotorState = 0;
    outputStateChange("Fan motor: STATE = OFF \n");
  }    
}

int getWaterLevel()
{
  // this function reads the water sensor
  waterValue = adc_read(0);                                             // mask
  waterLevel = map(waterValue, WATER_LEVEL_MIN, WATER_LEVEL_MAX, 0, 4); // 4 levels also need to setup max macro

  // DEBUG
  Serial.print("Water value / level: ");
  Serial.print(waterValue);
  Serial.print("/");
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
  // pullup is unenabled for default LOW

  // clear PB0-PB3
  *port_b &= 0xF0;

  if (statusLight == DISABLED)
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB0 high (yellow)
    *port_b |= (*port_b & 0xF0) | 0x01;
  }
  else if (statusLight == IDLE)
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB1 high (green)
    *port_b |= (*port_b & 0xF0) | 0x02;
  }
  else if (statusLight == RUNNING)
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB2 high (blue)
    *port_b |= (*port_b & 0xF0) | 0x04;
  }
  else if (statusLight == ERROR)
  {
    // clears PB0-PB3, leaving PB4-PB7 unchanged, then sets PB3 high (red)
    *port_b |= (*port_b & 0xF0) | 0x08;
  }
}

// ADC functions
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

// timer setup function
void setup_timer_regs()
{
  // set up the timer control registers
  *myTCCR1A = 0x00;
  *myTCCR1B = 0X00;
  *myTCCR1C = 0x00;
  // reset the TOV flag
  *myTIFR1 |= 0x01;
  // enable the TOV interrupt
  *myTIMSK1 |= 0x01;
}
// timer overflow ISR
//  ISR(TIMER1_OVF_vect)
//  {
//    //Stop the Timer
//    *myTCCR1B &= 0xF8;
//    //Load the Count
//    *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
//    //Start the Timer
//    *myTCCR1B |= 0b00000001;
//    //if it's not the STOP amount
//    if(currentTicks != 65535)
//    {
//      //XOR to toggle PB6
//      *portB ^= 0x40;
//    }
//  }

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

/*void outputStateChange(String state)
{
  // for (int i = 0; i < state.length(); i++)
  // {
  //   U0putchar(state[i]);
  // }
  // U0putchar(' ');

  bool is24hour = true;
  bool isPMtime = false;

  byte date = myRTC.getDate();
  byte hour = myRTC.getHour(is24hour, isPMtime);
  byte minute = myRTC.getMinute();

  // DEBUG
  Serial.print("State: ");
  Serial.println(status);
  Serial.print("Date: ");
  Serial.println(date);
  Serial.print("Hour: ");
  Serial.println(hour);
  Serial.print("Minute: ");
  Serial.println(minute);

  for (int i = 0; i < date.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar(' ');

  for (int i = 0; i < hour.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar(':');

  for (int i = 0; i < minute.length(); i++)
  {
    U0putchar(state[i]);
  }
  U0putchar('\n');
}*/

void outputStateChange(int state)
{
  bool century = false;
  bool h12Flag;
  bool pmFlag;

  byte year = myRTC.getYear();
  byte month = myRTC.getMonth(century);
  byte date = myRTC.getDate();
  byte hour = myRTC.getHour(h12Flag, pmFlag);
  byte minute = myRTC.getMinute();
  byte second = myRTC.getSecond();

  //print time stamp
  Serial.print(year, DEC);
  Serial.print("-");
  Serial.print(month, DEC);
  Serial.print("-");
  Serial.print(date, DEC);
  Serial.print(" ");
  Serial.print(hour, DEC); //24-hr
  Serial.print(":");
  Serial.print(minute, DEC);
  Serial.print(":");
  Serial.println(second, DEC);
  
  //print state
  U0putchar("System State: ");
  if (state == DISABLED)
  {
    U0putchar("DISABLED\n");
  }
  if (state == IDLE)
  {
    U0putchar("IDLE\n");
  }
  if (state == RUNNING)
  {
    U0putchar("RUNNING\n");
  }
  if (state == ERROR)
  {
    U0putchar("ERROR\n");
  }
}

void myDelay(unsigned int seconds)
{
  int ticks = seconds / 0.0000000625;
  *myTCCR1B &= 0xF8;                        // make sure timer is off
  *myTCNT1 = (unsigned int)(65536 - ticks); // load the counter
  *myTCCR1B |= 0b00000101;                  // prescalar 1024, turn timer on
  while ((*myTIFR1 & 0x01) == 0)
    ;                // wait for TIFR overflow flag bit to be set
  *myTCCR1B &= 0xF8; // Turn off the timer after getting delay
  *myTIFR1 |= 0x01;  // clear the flag bit
}