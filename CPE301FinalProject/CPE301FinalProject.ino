// Author: Abby Battaglia, Jasmine Huang, Marlo Ongkingco, Mark Mitzen
// Date: 5/10/24
// Title: CPE 301 Final Project

// included libraries
#include <LiquidCrystal.h>
#include <DHT11.h>
#include <Stepper.h>

#define RDA 0x80
#define TBE 0x20

// set up LCD pins
const int RS = 13, EN = 12, D4 = 11, D5 = 10, D6 = 9, D7 = 8;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// set up DHT pins
DHT11 dht11(7); // DHT sensor connected to digital pin 7

/*THESE ARE WRONG FIX THESE
//define pointers for port D, used for LEDs
volatile unsigned char* port_b = (unsigned char*) 0x25
volatile unsigned char* ddr_b = (unsigned char*) 0x24
*/

// set up motor
const int stepsPerRevolution = 2038;
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
Stepper myStepper = Stepper(stepsPerRevolution, 1, 3, 2, 4);

/*
//set up water sensor pin and define water level variable
const int waterSensor = 48; //Port L, bit 1 (PL1)
volatile int waterValue = 0;
int waterLevel;
#define WATER_LEVEL_MIN 0
#define WATER_LEVEL_MAX 521 //TEMP, need to setup based on max value during water sensor calibration
*/

// UART Pointers
volatile unsigned char *myUCSR0A = 0x00C0;
volatile unsigned char *myUCSR0B = 0x00C1;
volatile unsigned char *myUCSR0C = 0x00C2;
volatile unsigned int *myUBRR0 = 0x00C4;
volatile unsigned char *myUDR0 = 0x00C6;

/*
//timer Pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned char *myTIFR1 =  (unsigned char *) 0x36;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;
*/

void setup()
{
  // put your setup code here, to run once:
  U0Init(9600);
  lcd.begin(16, 2); // set up number of columns and rows
  // setup the Timer for Normal Mode, with the TOV interrupt enabled
  // setup_timer_regs();
  // start the UART
  // U0Init(9600);
}

void loop()
{
  // put your main code here, to run repeatedly:
  lcdDisplay(); // display the temperature and humidity

  // getWaterLevel(); // get the water level
  // if water level is good
  /*
  if (waterLevel == 1)
  {
    // get button input (either 1 or -1)
    Motor(false, button input);
  }
  // if water level is too low
  else
  {
    Motor(true, 0);
  }
  */
}

void lcdDisplay()
{
  // this function sets up the the lCD
  int temperature = dht11.readTemperature(); // read the temperature
  int humidity = dht11.readHumidity();       // read the humidity

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

  delay(60000); // delay for 1 minute
  // REPLACE WITH MILLIS() AT SOME POINT
}
/*
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
*/

void Motor(bool onOff, int direction)
{
  if (onOff)
  {
    myStepper.setSpeed(5); // arbitrary speed in rpm
    myStepper.step(stepsPerRevolution * direction);
    // maybe delay here
  }
  else
  {
    myStepper.setSpeed(0);              // I think this turns it off (sets rpm to zero)
    myStepper.step(stepsPerRevolution); // don't know if this is still nessesary for turning it off
    // maybe delay here
  }
  // a positive stepsPerRevolution is clockwise and negative is counter clockwise
  // essentially have direction be 1 for clockwise and -1 for counterclockwise
}

/*
void getWaterLevel()
{
  //this function reads the water sensor
  waterValue = (pinl >> PL1) & 0x01; //mask
  waterLevel = map(value, WATER_LEVEL_MIN, WATER_LEVEL_MAX, 0, 4); // 4 levels also need to setup max macro
}
*/

/* THIS IS WRONG
void LED()
{
  //this function sets up the status LEDs

  //RED - PD0, pin 53
  //BLUE - PD1, pin 52
  //YELLOW - PD2, pin 51
  //GREEN - PD3, pin 50

  *ddr_d |= 0x0F; //set pins PD0-PD3 as outputs
  *port_d &= 0x0F; //set pins PD0-PD3 to enable pullup


  if ()
  {
    *port_b &= 0x01; // 0000 0001 > PB0 high, all others low
  }
  else if ()
  {
    *port_b &= 0x02; // 0000 0010 > PB1 high, all others low
  }
  else if ()
  {
    *port_b |= 0x04; // 0000 0100 > PB2 high, all others low
  }
  else if ()
  {
    *port_b |= 0x08; // 0000 1000 > PB3 high, all others low
  }
}
*/

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

unsigned char kbhit()
{
  return *myUCSR0A & RDA;
}

unsigned char getChar()
{
  return *myUDR0;
}

void putChar(unsigned char U0pdata)
{
  while ((*myUCSR0A & TBE) == 0)
    ;
  *myUDR0 = U0pdata;
}
