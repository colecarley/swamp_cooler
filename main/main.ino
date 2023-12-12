/*
 * Swamp cooler, implemented on an Arduino mega 2560
 * authors: Cole Carley, Madeline Veric
 * 2023-12-01
 */

#include <dht.h>
#include <LiquidCrystal.h>
#include <Stepper.h>
#include "uRTCLib.h"

volatile unsigned char *myTCCR1A = (unsigned char *)0x80;
volatile unsigned char *myTCCR1B = (unsigned char *)0x81;
volatile unsigned int *myTCNT1 = (unsigned int *)0x84;
volatile unsigned char *myTIFR1 = (unsigned char *)0x36;

volatile unsigned char *port_b = (unsigned char *)0x25;
volatile unsigned char *ddr_b = (unsigned char *)0x24;

volatile unsigned char *port_a = (unsigned char *)0x22;
volatile unsigned char *ddr_a = (unsigned char *)0x21;
volatile unsigned char *pin_a = (unsigned char *)0x20;

volatile unsigned char *port_d = (unsigned char *)0x2B;
volatile unsigned char *ddr_d = (unsigned char *)0x2A;

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned char *myUBRR0 = (unsigned char *)0x00C4;
volatile unsigned char *myUDR0 = (unsigned char *)0x00C6;

volatile unsigned char *my_ADMUX = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *)0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *)0x78;

#define TBE 0x20

// state LEDs
#define IDLE_PIN 2     // GREEN
#define ERROR_PIN 1    // RED
#define RUNNING_PIN 0  // BLUE
#define DISABLED_PIN 3 // YELLOW

// port A
#define FAN_PIN 6
#define RESET_PIN 4
#define POWER_PIN 7
#define VENT_PIN 7
#define DHT11_PIN 6

#define ON_OFF_PIN 19 // digital interrupt pin

#define TEMPERATURE_THRESHOLD 23  // degrees C
#define WATER_LEVEL_THRESHOLD 100 // arbitrary units

#define SET_OUTPUT(port, pin) (port |= 0x01 << pin)
#define SET_INPUT(port, pin) (port &= ~(0x01 << pin))
#define SET_HIGH(port, pin) (port |= 0x01 << pin)
#define READ(port, pin) (port & (0x01 << pin))
#define WRITE(port, pin, state) state ? (port |= 0x01 << pin) : (port &= ~(0x01 << pin))
#define NEWLINE put((unsigned char)'\n')

dht DHT; // setup the DHT11 for temperature and humidity
const int RS = 12, EN = 7, D4 = 5, D5 = 4, D6 = 3, D7 = 2;

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7); // setup the LCD
uRTCLib rtc;                               // setup the RTC

// initialize the stepper motor for the vent
const int steps_per_revolution = 2038;
Stepper myStepper = Stepper(steps_per_revolution, 8, 9, 10, 11); // port B

int water_level, temperature, humidity = 0;

enum State // possible states of the system
{
    DISABLED,
    IDLE,
    ERROR,
    RUNNING
};

State state = State::IDLE;
void setup()
{
    SET_OUTPUT(*ddr_b, POWER_PIN);
    SET_OUTPUT(*ddr_a, ERROR_PIN);
    SET_OUTPUT(*ddr_a, IDLE_PIN);
    SET_OUTPUT(*ddr_a, RUNNING_PIN);
    SET_OUTPUT(*ddr_a, DISABLED_PIN);
    SET_INPUT(*ddr_a, RESET_PIN);
    SET_OUTPUT(*ddr_a, FAN_PIN);
    SET_INPUT(*ddr_a, VENT_PIN);
    SET_INPUT(*ddr_d, ON_OFF_PIN);
    SET_HIGH(*port_d, ON_OFF_PIN); // set as pullup for interrupt
    SET_INPUT(*ddr_a, VENT_PIN);
    // initialize the LCD
    lcd.begin(16, 2);
    U0Init(9600);
    URTCLIB_WIRE.begin();
    rtc.set(0, 23, 5, 5, 8, 12, 23);
    adc_init();
    myStepper.setSpeed(10);
    attachInterrupt(digitalPinToInterrupt(ON_OFF_PIN), on_off_interrupt, RISING);

    WRITE(*port_b, POWER_PIN, LOW);
}

bool show_datetime[] = {true, true, true, true}; // idle, running, error, disabled
void on_off_interrupt()
{
    if (state == State::DISABLED)
    {
        update_lcd();
        show_datetime[0] = true;
        show_datetime[1] = true;
        show_datetime[2] = true;

        state = State::IDLE;
    }
    else
    {
        show_datetime[3] = true;
        state = State::DISABLED;
    }
}

void loop()
{
    if (state != State::DISABLED)
        handle_not_disabled();
    if (state == State::DISABLED)
        handle_disabled();
    if (state == State::RUNNING)
        handle_running();
    if (state == State::IDLE)
        handle_idle();
    if (state == State::ERROR)
        handle_error();
}

void update_lcd()
{
    NEWLINE;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WATER:");
    lcd.print(water_level);
    lcd.setCursor(12, 0);
    lcd.print(temperature);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("HUMIDITY:");
    lcd.print(humidity);
}

void handle_running()
{
    if (show_datetime[1])
    {
        put((unsigned char *)"running");
        NEWLINE;
        put_datetime();
        show_datetime[1] = false;
    }

    if (water_level < WATER_LEVEL_THRESHOLD)
    {
        show_datetime[1] = true;
        state = State::ERROR;
        return;
    }
    if (temperature <= TEMPERATURE_THRESHOLD)
    {
        show_datetime[1] = true;
        update_lcd();
        state = State::IDLE;
        return;
    }

    WRITE(*port_a, RUNNING_PIN, HIGH);
    WRITE(*port_a, IDLE_PIN, LOW);
    WRITE(*port_a, ERROR_PIN, LOW);
    WRITE(*port_a, DISABLED_PIN, LOW);

    WRITE(*port_a, FAN_PIN, HIGH);
}

void handle_idle()
{

    if (show_datetime[0])
    {
        put((unsigned char *)"idle");
        NEWLINE;
        put_datetime();
        show_datetime[0] = false;
    }

    if (water_level <= WATER_LEVEL_THRESHOLD)
    {
        show_datetime[0] = true;
        state = State::ERROR;
        return;
    }

    if (temperature > TEMPERATURE_THRESHOLD)
    {
        show_datetime[0] = true;
        state = State::RUNNING;
        return;
    }

    WRITE(*port_a, RUNNING_PIN, LOW);
    WRITE(*port_a, IDLE_PIN, HIGH);
    WRITE(*port_a, ERROR_PIN, LOW);
    WRITE(*port_a, DISABLED_PIN, LOW);

    // turn off fan
    WRITE(*port_a, FAN_PIN, LOW);
}

void handle_disabled()
{
    if (show_datetime[3])
    {
        put((unsigned char *)"disabled");
        NEWLINE;
        put_datetime();
        show_datetime[3] = false;
    }

    WRITE(*port_a, RUNNING_PIN, LOW);
    WRITE(*port_a, IDLE_PIN, LOW);
    WRITE(*port_a, ERROR_PIN, LOW);
    WRITE(*port_a, DISABLED_PIN, HIGH);

    // turn off fan
    WRITE(*port_a, FAN_PIN, LOW);
    lcd.clear();
}

void handle_error()
{
    if (show_datetime[2])
    {
        put((unsigned char *)"error");
        NEWLINE;
        put_datetime();
        show_datetime[2] = false;
    }

    unsigned char reset_button = READ(*pin_a, RESET_PIN);
    if (reset_button)
    {
        update_lcd();
        show_datetime[2] = true;
        state = State::IDLE;
        return;
    }

    WRITE(*port_a, RUNNING_PIN, LOW);
    WRITE(*port_a, IDLE_PIN, LOW);
    WRITE(*port_a, ERROR_PIN, HIGH);
    WRITE(*port_a, DISABLED_PIN, LOW);

    WRITE(*port_a, FAN_PIN, LOW);

    // display error message
    lcd.setCursor(0, 0);
    lcd.clear();
    char *error_msg = "ERROR: WATER LEVEL TOO LOW";
    for (int i = 0; i < 16; i++)
        lcd.print(error_msg[i]);
    lcd.setCursor(0, 1);
    for (int i = 16; i < strlen(error_msg); i++)
        lcd.print(error_msg[i]);
}

bool first = true;
unsigned long milliseconds = 0;
bool motor_clockwise = true;
unsigned long last = 0;
void handle_not_disabled()
{
    milliseconds = millis();
    if (state != State::ERROR && (milliseconds - last) > 60000)
    {
        last = milliseconds;
        milliseconds = 0;
        update_lcd();
    }

    unsigned char vent_button = READ(*pin_a, VENT_PIN);
    if (vent_button)
    {
        put((unsigned char *)"vent changing position");
        NEWLINE;
        put_datetime();
        myStepper.step(motor_clockwise ? steps_per_revolution : -steps_per_revolution);
        motor_clockwise = !motor_clockwise;
    }

    WRITE(*port_b, POWER_PIN, HIGH);

    water_level = adc_read(5);
    int chk = DHT.read11(DHT11_PIN);
    temperature = DHT.temperature;
    humidity = DHT.humidity;

    if (first)
    {
        update_lcd();
        first = false;
    }

    WRITE(*port_b, POWER_PIN, LOW);
}

void U0Init(int U0baud)
{
    unsigned long FCPU = 16000000;
    unsigned int tbaud;
    tbaud = (FCPU / 16 / U0baud - 1);
    *myUCSR0A = 0x20;
    *myUCSR0B = 0x18;
    *myUCSR0C = 0x06;
    *myUBRR0 = tbaud;
}

void put_datetime()
{
    rtc.refresh();
    put((unsigned int)rtc.month());
    put((unsigned char)'/');
    put((unsigned int)rtc.day());
    put((unsigned char)'/');
    put((unsigned int)rtc.year());
    put((unsigned char)' ');
    put((unsigned int)rtc.hour());
    put((unsigned char)':');
    if (rtc.minute() < 10)
        put((unsigned char)'0');
    put((unsigned int)rtc.minute());
    put((unsigned char)':');
    if (rtc.second() < 10)
        put((unsigned char)'0');
    put((unsigned int)rtc.second());
    NEWLINE;
}

void put(unsigned char U0pdata)
{
    while ((*myUCSR0A & TBE) == 0)
        ;
    *myUDR0 = U0pdata;
}

void put(unsigned char *inStr)
{
    for (int i = 0; i < strlen(inStr); i++)
    {
        put(inStr[i]);
    }
}

void put(unsigned int out)
{
    if (!out)
    {
        put((unsigned char)'0');
        return;
    }
    if (out >= 1000)
    {
        put((unsigned char)(out / 1000 + '0'));
        out = out % 1000;
    }
    if (out >= 100)
    {
        put((unsigned char)(out / 100 + '0'));
        out = out % 100;
    }
    if (out >= 10)
    {
        put((unsigned char)(out / 10 + '0'));
        out = out % 10;
    }
    put((unsigned char)(out + '0'));
}

void my_delay(unsigned int ms)
{
    double half_period = ms / 2.0f;
    double clk_frequncy = 16000000 / 256;
    double clk_period = 1.0 / clk_frequncy;
    unsigned int ticks = half_period / clk_period;

    *myTCCR1B &= 0xF8;
    *myTCNT1 = (unsigned int)(65536 - ticks);
    // start the timer with 256 prescaler
    *myTCCR1A = 0x0;
    *myTCCR1B |= 0x04;

    while ((*myTIFR1 & 0x01) == 0)
        ;

    *myTCCR1B &= 0xF8;
    *myTIFR1 |= 0x01;
}

unsigned int adc_read(unsigned char adc_channel_num)
{
    *my_ADMUX &= 0b11100000;
    *my_ADCSRB &= 0b11110111;
    if (adc_channel_num > 7)
    {
        adc_channel_num -= 8;
        *my_ADCSRB |= 0b00001000;
    }

    *my_ADMUX += adc_channel_num;
    *my_ADCSRA |= 0x40;
    while ((*my_ADCSRA & 0x40) != 0)
        ;

    return *my_ADC_DATA;
}

void adc_init()
{
    *my_ADCSRA |= 0b10000000;
    *my_ADCSRA &= 0b11011111;
    *my_ADCSRA &= 0b11110111;
    *my_ADCSRA &= 0b11111000;

    *my_ADCSRB &= 0b11110111;
    *my_ADCSRB &= 0b11111000;

    *my_ADMUX &= 0b01111111;
    *my_ADMUX |= 0b01000000;
    *my_ADMUX &= 0b11011111;
    *my_ADMUX &= 0b11100000;
}