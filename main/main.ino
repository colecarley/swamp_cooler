/*
 * Swap cooler, implemented on an Arduino mega 2560 with no libraries
 * authors: Cole Carley, Madeline Veric
 * 2023-12-01
 */

#include <dht.h>
#include <LiquidCrystal.h>
#include <Stepper.h>

volatile unsigned char *myTCCR1A = (unsigned char *)0x80;
volatile unsigned char *myTCCR1B = (unsigned char *)0x81;
volatile unsigned char *myTCCR1C = (unsigned char *)0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *)0x6F;
volatile unsigned int *myTCNT1 = (unsigned int *)0x84;
volatile unsigned char *myTIFR1 = (unsigned char *)0x36;

volatile unsigned char *port_b = (unsigned char *)0x25;
volatile unsigned char *ddr_b = (unsigned char *)0x24;
volatile unsigned char *pin_b = (unsigned char *)0x23;

volatile unsigned char *port_a = (unsigned char *)0x22;
volatile unsigned char *ddr_a = (unsigned char *)0x21;
volatile unsigned char *pin_a = (unsigned char *)0x20;

volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned char *myUBRR0 = (unsigned char *)0x00C4;
volatile unsigned char *myUDR0 = (unsigned char *)0x00C6;

volatile unsigned char *my_ADMUX = (unsigned char *)0x7C;
volatile unsigned char *my_ADCSRB = (unsigned char *)0x7B;
volatile unsigned char *my_ADCSRA = (unsigned char *)0x7A;
volatile unsigned int *my_ADC_DATA = (unsigned int *)0x78;

#define IDLE_PIN 2     // GREEN
#define ERROR_PIN 1    // RED
#define RUNNING_PIN 0  // BLUE
#define DISABLED_PIN 3 // YELLOW
#define FAN_PIN 6
#define RESET_PIN 4
#define ON_OFF_PIN 5

#define POWER_PIN 7

// changing vent pin to PA7
#define VENT_PIN 7
#define DHT11_PIN 6

#define TBE 0x20

#define TEMPERATURE_THRESHOLD 0   // degrees C
#define WATER_LEVEL_THRESHOLD 100 // arbitrary units

// possible states of the system
enum State
{
    DISABLED,
    IDLE,
    ERROR,
    RUNNING
};

// setup the DHT11 for temperature and humidity
dht DHT;
const int RS = 12, EN = 7, D4 = 5, D5 = 4, D6 = 3, D7 = 2;

// setup the LCD
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

int water_level = 0;
int temperature = 0;
int humidity = 0;

// initialize the stepper motor for the vent
const int steps_per_revolution = 900;
Stepper myStepper = Stepper(steps_per_revolution, 8, 9, 10, 11);
bool motor_clockwise = true;

State state = State::IDLE;
void setup()
{
    set_pb_as_output(POWER_PIN);
    // set error pin PA1 as output
    set_pa_as_output(ERROR_PIN);
    // set idle pin PA2 as output
    set_pa_as_output(IDLE_PIN);
    // set running pin PA3 as output
    set_pa_as_output(RUNNING_PIN);
    // set disabled pin PA4 as output
    set_pa_as_output(DISABLED_PIN);
    // set reset pin PA5 as input
    set_pa_as_input(RESET_PIN);
    // set on/off pin PA6 as input
    set_pa_as_input(ON_OFF_PIN);
    // set fan pin PA7 as output
    set_pa_as_output(FAN_PIN);

    // initialize the LCD
    lcd.begin(16, 2);

    // initialize the serial port
    U0Init(9600);

    // initialize the DHT11
    adc_init();

    // set_pb_as_input(VENT_PIN);
    set_pa_as_input(VENT_PIN);
    // pinMode(14, INPUT);

    write_pb(POWER_PIN, LOW);
}

void loop()
{
    put((unsigned char)'A');
    my_delay(1000);

    if (state != State::DISABLED)
    {
        unsigned char result = read_pa(VENT_PIN);
        if (result)
        {
            motor_clockwise = !motor_clockwise;
            myStepper.setSpeed(10);
            myStepper.step(steps_per_revolution > 0 ? steps_per_revolution : -steps_per_revolution);
            my_delay(1000);
        }

        handle_not_disabled();
    }

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
    put((unsigned char *)"UPDATING LCD\n");
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
    put((unsigned char *)"RUNNING\n");

    if (water_level < WATER_LEVEL_THRESHOLD)
    {
        state = State::ERROR;
        return;
    }

    if (temperature <= TEMPERATURE_THRESHOLD)
    {
        state = State::IDLE;
        return;
    }

    // TODO: watch for button press to disable
    unsigned char on_off_button = read_pa(ON_OFF_PIN);
    if (on_off_button)
    {
        put((unsigned char *)"on/off button clicked inside running handler\n");
        state = State::DISABLED;
        my_delay(1000);
        return;
    }

    // turn on running pin
    write_pa(RUNNING_PIN, HIGH);
    // turn off idle pin
    write_pa(IDLE_PIN, LOW);
    // turn off error pin
    write_pa(ERROR_PIN, LOW);
    // turn off disabled pin
    write_pa(DISABLED_PIN, LOW);

    // turn on fan
    write_pa(FAN_PIN, HIGH);

    put((unsigned char *)"got to the end of the running handler\n");
}

void handle_idle()
{
    put((unsigned char *)"IDLE\n");

    if (water_level <= WATER_LEVEL_THRESHOLD)
    {
        state = State::ERROR;
        return;
    }

    if (temperature > TEMPERATURE_THRESHOLD)
    {
        state = State::RUNNING;
        return;
    }

    unsigned char on_off_button = read_pa(ON_OFF_PIN);
    if (on_off_button)
    {
        state = State::DISABLED;
        my_delay(1000);
        return;
    }

    // turn off running pin
    write_pa(RUNNING_PIN, LOW);
    // turn on idle pin
    write_pa(IDLE_PIN, HIGH);
    // turn off error pin
    write_pa(ERROR_PIN, LOW);
    // turn off disabled pin
    write_pa(DISABLED_PIN, LOW);

    // turn off fan
    write_pa(FAN_PIN, LOW);
}

void handle_disabled()
{
    put((unsigned char *)"DISABLED\n");

    unsigned char on_off_button = read_pa(ON_OFF_PIN);
    if (on_off_button)
    {
        put((unsigned char *)"on/off button clicked inside disabled handler\n");
        state = State::IDLE;
        my_delay(1000);
        return;
    }

    // turn off running pin
    write_pa(RUNNING_PIN, LOW);
    // turn off idle pin
    write_pa(IDLE_PIN, LOW);
    // turn off error pin
    write_pa(ERROR_PIN, LOW);
    // turn on disabled pin
    write_pa(DISABLED_PIN, HIGH);

    // turn off fan
    write_pa(FAN_PIN, LOW);
    lcd.clear();
}

void handle_error()
{
    put((unsigned char *)"ERROR\n");

    unsigned char reset_button = read_pa(RESET_PIN);
    if (reset_button)
    {
        put((unsigned char *)"reset button clicked inside error handler\n");
        state = State::IDLE;
        return;
    }

    unsigned char on_off_button = read_pa(ON_OFF_PIN);
    if (on_off_button)
    {
        put((unsigned char *)"on/off button clicked inside error handler\n");
        state = State::DISABLED;
        my_delay(1000);
        return;
    }

    // turn off running pin
    write_pa(RUNNING_PIN, LOW);
    // turn off idle pin
    write_pa(IDLE_PIN, LOW);
    // turn on error pin
    write_pa(ERROR_PIN, HIGH);
    // turn off disabled pin
    write_pa(DISABLED_PIN, LOW);

    // turn off fan
    write_pa(FAN_PIN, LOW);

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

void handle_not_disabled()
{
    if (state != State::ERROR)
    {
        update_lcd();
    }

    put((unsigned char *)"NOT DISABLED\n");
    write_pb(POWER_PIN, HIGH);

    water_level = adc_read(5);
    int chk = DHT.read11(DHT11_PIN);
    temperature = DHT.temperature;
    humidity = DHT.humidity;

    write_pb(POWER_PIN, LOW);

    put((unsigned int)water_level);
    newline();
    put((unsigned int)temperature);
    newline();
    put((unsigned int)humidity);
    newline();
    newline();
}

void set_pb_as_output(unsigned char pin_num)
{
    *ddr_b |= 0x01 << pin_num;
}

void set_pb_as_input(unsigned char pin_num)
{
    *ddr_b &= ~(0x01 << pin_num);
}

void set_pa_as_output(unsigned char pin_num)
{
    *ddr_a |= 0x01 << pin_num;
}

void set_pa_as_input(unsigned char pin_num)
{
    *ddr_a &= ~(0x01 << pin_num);
}

void write_pb(unsigned char pin_num, unsigned char state)
{
    state ? (*port_b |= 0x01 << pin_num) : (*port_b &= ~(0x01 << pin_num));
}

void write_pa(unsigned char pin_num, unsigned char state)
{
    state ? (*port_a |= 0x01 << pin_num) : (*port_a &= ~(0x01 << pin_num));
}

unsigned char read_pb(unsigned char pin_num)
{
    return *pin_b & (0x01 << pin_num);
}

unsigned char read_pa(unsigned char pin_num)
{
    return *pin_a & (0x01 << pin_num);
}

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

void newline()
{
    put((unsigned char)'\n');
}

void put(unsigned int out_num)
{
    unsigned char print_flag = 0;
    unsigned char lz_flag = 0;

    if (out_num >= 1000)
    {
        put((unsigned char)(out_num / 1000 + '0'));
        print_flag = 1;
        lz_flag = 1;
        out_num = out_num % 1000;
    }
    if (out_num >= 100 || print_flag)
    {
        put((unsigned char)(out_num / 100 + '0'));
        print_flag = 1;
        lz_flag = 1;
        out_num = out_num % 100;
    }
    if (out_num >= 10 || print_flag)
    {
        put((unsigned char)(out_num / 10 + '0'));
        print_flag = 1;
        lz_flag = 1;
        out_num = out_num % 10;
    }
    if (!lz_flag)
    {
        put((unsigned char)'0');
    }
    put((unsigned char)(out_num + '0'));
}

void my_delay(unsigned int ms)
{
    // 50% duty cycle
    double half_period = ms / 2.0f;

    // clock period with 256 prescaler
    double clk_frequncy = 16000000 / 256;
    double clk_period = 1.0 / clk_frequncy;
    // calc ticks
    unsigned int ticks = half_period / clk_period;

    // make sure the timer is stopped
    *myTCCR1B &= 0xF8;

    // set the counts
    *myTCNT1 = (unsigned int)(65536 - ticks);
    // start the timer with 256 prescaler
    *myTCCR1A = 0x0;
    *myTCCR1B |= 0x04;

    // wait for overflow
    while ((*myTIFR1 & 0x01) == 0)
        ; // 0b 0000 0000

    // stop the timer
    *myTCCR1B &= 0xF8; // 0b 0000 0000
    // reset TOV
    *myTIFR1 |= 0x01;
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