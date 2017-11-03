/*
TODO:
android app
light sensor+ bubbler on-off
use pulse counter to abort pump
beeper modes
resilient: recovery from RTC /flow meter failure
MAC address of Bluetooth HC-05 is 98:D3:32:20:EA:01 
Connect the HC-05 TX to Arduino's Rx  pin. 
Connect the HC-05 RX to Arduino's Tx  pin through a 1K:2K voltage divider.
RTC module is DS3231.
Temperature & humidity sensor is DHT11.
*/
#include <Timer.h>
#include <dht.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <Time.h>         //http://www.arduino.cc/playground/Code/Time  
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC

const byte pump_relay = 7;          // active LOW
const byte bubbler_relay = 8;       // active LOW
const byte pump_led = 9;            // active LOW
const byte bubbler_led = 10;        // active LOW
const byte watchdog_led = 11;       // active LOW
const byte buzzer_pin = 6;          // active HIGH
const byte flow_pin = 2;            // interrupt 0 pin - no pullup
//const byte rtc_pin = 3;           // interrupt 1 pin - no pullup
const byte ldr_pin = A7;            // external 10K POT for sensitivity
const byte dht_pin = 12;            // 10K external pullup
const byte tube_high_pin = A0;      // active LOW input; internal pullup
const byte tube_low_pin = A1;       // active LOW input; internal pullup
const byte reservoir_high_pin = A2; // active LOW input; internal pullup
const byte reservoir_low_pin = A3;  // active LOW input; internal pullup

Timer T;  // software timer
dht DHT;
SoftwareSerial BTSerial(4, 5); // RX, TX from Arduino's point of view

unsigned int tick_interval = 1000; // in milliSec;
boolean pump_enabled = true;
boolean pump_running = false;
boolean bubbler_running = false;
boolean water_flowing = false;
boolean reservoir_high = 1; // active LOW input
boolean reservoir_low = 1; // active LOW input
boolean tube_high = 1; // active LOW input
boolean tube_low = 1; // active LOW input

volatile int pulse_count = 0; // count per 2 second interval
int pulse_count_copy = 0; // make a copy for leisurely processing
int light_intensity = 0;
float temperatur = 0.0f; // 'temperature' is an existing key word
float humidity = 0.0f;

void loop() {
    T.update();
}

void setup() {
    pinMode(pump_relay, OUTPUT); 
    pinMode(bubbler_relay, OUTPUT);  
    pinMode(pump_led, OUTPUT); 
    pinMode(bubbler_led, OUTPUT);  
    pinMode(watchdog_led, OUTPUT); 
    pinMode(buzzer_pin, OUTPUT); 
    
    digitalWrite(pump_relay, HIGH);       // active LOW
    digitalWrite(bubbler_relay, HIGH);    // active LOW    
    digitalWrite(pump_led, HIGH);         // active LOW 
    digitalWrite(bubbler_led, HIGH);      // active LOW
    digitalWrite(watchdog_led, HIGH);     // active LOW
    digitalWrite(buzzer_pin, LOW);        // active HIGH    
    
    pinMode(flow_pin, INPUT_PULLUP);      // flow meter interrupt - no pullup
    //pinMode(rtc_pin, INPUT_PULLUP);   // RTC interrupt - internal pullup    
    pinMode(dht_pin, INPUT);       // 10K external pullup
    pinMode(ldr_pin, INPUT);       // external 10K POT for sensitivity
    pinMode(tube_high_pin, INPUT_PULLUP);       // active LOW input
    pinMode(tube_low_pin, INPUT_PULLUP);        // active LOW input
    pinMode(reservoir_high_pin, INPUT_PULLUP);  // active LOW input
    pinMode(reservoir_low_pin, INPUT_PULLUP);   // active LOW input
    
    Serial.begin(9600);
    Serial.println("Hydroponics tube starting...");
    BTSerial.begin(9600);
    blinker();
    if (!init_RTC())
        error_loop(); // cry for 10 minutes !
    attachInterrupt(0, pulse_ISR, RISING);  //rising edges from the flow meter    
    T.every(tick_interval, tick);  // software timer
}

byte epoch = 0; // 4 seconds epoch, split at 1 second resolution
byte block_count = 0;  // 1 minute is a block
void tick() {  
    switch(epoch) {
        case 0:
            check_pump_start();        
            if (!check_oflow_uflow()) {
                stop_pump();    // emergency stop         
                pump_enabled = false;
            }
            T.pulse(watchdog_led, 20, HIGH); // active low            
            break;
        case 1:
            read_flow_meter();   // sensitive to delay; do this first on the epoch         
            check_pump_stop();   // scheduled stop            
            if (pump_running)
                T.pulse(pump_led, 40, HIGH); // active low       
            break;
        case 2: 
            if (!check_oflow_uflow()) {
                stop_pump();   // emergency stop
                pump_enabled = false;
            }
            send_status();
            T.pulse(watchdog_led, 20, HIGH); // active low                
            break;             
        case 3: 
            read_flow_meter();   // sensitive to delay; do this first on the epoch  
            check_bubbler_start_stop();        
            if (bubbler_running)
                T.pulse(bubbler_led, 40, HIGH); // active low  
            break;                
    }    
    epoch = (epoch+1)%4;  // 4 seconds
    block_count = (block_count+1)%60;  // 1 minute
    if (block_count==0) {
        read_sensors();
        send_data();
    }
}

void read_sensors() { // TODO: read them once a minute
    light_intensity = analogRead(ldr_pin);
    // Reading the DHT11 takes about 250 milliseconds!
    DHT.read11(dht_pin);
    temperatur = DHT.temperature;
    humidity = DHT.humidity;  
}

boolean check_pump_start() {
    //if (!alarm_triggered) return 0;  
    if (!RTC.alarm(ALARM_1)) return 0;   
    if (!pump_enabled) return 0;
    if (!check_oflow_uflow) return 0;
    return(start_pump());
}

boolean check_pump_stop(){
    //if (!alarm_triggered) return 0;  
    if (!RTC.alarm(ALARM_2)) return 0;   
    return(stop_pump());
}

boolean start_pump(){
    arm_flow_meter_alarm();  
    digitalWrite(pump_relay, LOW); // active low
    pump_running = true;
    pump_enabled = true; // TODO: revisit this
    return 1;
}

boolean stop_pump(){
    digitalWrite(pump_relay, HIGH); // active low
    pump_running = false;
    return 1;
}

/*
 * Updates the 4 flags of water level indicator.
 * Returns 0 if there is a crisis, 1 otherwise
 * Crisis1: reservoir_low_pin==LOW (underflow: reservoir is nearly empty)
 * Crisis2: tube_high==LOW (overflow from tube)
 */
boolean check_oflow_uflow() {
    reservoir_high = digitalRead(reservoir_high_pin); // active LOW input
    reservoir_low = digitalRead(reservoir_low_pin); // active LOW input
    tube_high = digitalRead(tube_high_pin); // active LOW input
    tube_low = digitalRead(tube_low_pin); // active LOW input
    if (!tube_high) return (0);
    if (!reservoir_low) return (0);
    return (1);
}

/*
 * Status packet: S 0101 1010 9999\r\n
 * From left to right:
 * S followed by a SPACE. Then a group of 4 bits
 * bit 1: pump enabled ?
 * bit 2: pump running ?
 * bit 3: bubbler running ?
 * bit 4: water flowing ?
 * Then, a SPACE and then a group of 4 bits
 * bit 1,2: reservoir level high, low reached (0=asserted)
 * bit 3,4: tube level high, low reached (0=asserted)
 * integer at the end: flow meter reading: count per 2 seconds
 */
char status_str[25] = "**** **** **** **** ****";
void send_status(){
    sprintf(status_str, 
        "S %1d%1d%1d%1d %1d%1d%1d%1d %3d\r\n", 
        pump_enabled, pump_running, bubbler_running, water_flowing,
        reservoir_high, reservoir_low, tube_high, tube_low,
        pulse_count_copy
        );
    BTSerial.write(status_str);
}

/*
 * Data packet: D 9999 99.9 99.9\r\n 
 * first, integer: light level from LDR
 * second, float: temperature in C from DHT11
 * third, float: % humidity from DHT11
 */
char data_str[25] = "**** **** **** **** ****"; 
void send_data(){
    sprintf (data_str,
        "D %d %d.%1d %d.%1d\r\n",
        light_intensity, 
        (int) temperatur, ((int)temperatur*10)%10,
        (int) humidity, ((int)humidity*10)%10
        );
    BTSerial.write(data_str);
}

void arm_flow_meter_alarm(){}

int flow_threshold = 20;  // TODO: check this
void read_flow_meter(){
    cli();            // Disable interrupts
    pulse_count_copy = pulse_count;  
    pulse_count = 0;  // ready for next counting       
    sei();            // Enable interrupts 
    water_flowing = (pulse_count_copy > flow_threshold);
}

boolean flow_meter_alarm_triggered(){// TODO: implement fully
    pump_enabled = false;  
}

void check_bubbler_start_stop(){}
void make_time_stamp(){}

void beep (int mode) {
  digitalWrite(buzzer_pin, HIGH);
  delay(200);
  digitalWrite(buzzer_pin, LOW);
}

void alarm_ISR(){}

void pulse_ISR () { // pulse counting ISR for flow meter
    pulse_count++; //
}

void reset_alarm() {  // TODO: A push button to call this ? remote reset by BT ? auto reset ?
    pump_enabled = true;
}

boolean init_RTC() {
    setSyncProvider(RTC.get); // sync with RTC every 5 minutes
    if (timeStatus() != timeSet) {
        Serial.println("* FAILED to Synchronize with RTC !!!!");
        return (false);
    }
    else
        Serial.println("Synchronized with RTC."); 
    //Disable the default square wave of the SQW pin
    RTC.squareWave(SQWAVE_NONE);  // use the SQW pin only for alarm triggering

    //Attach an interrupt on the falling of the SQW pin of DS3231.
    //pinMode(rtc_pin, INPUT_PULLUP);
    //attachInterrupt(INT1, alarm_ISR, FALLING); // SQW occurs on the falling edge (active low)

    //Set an alarm at the start of every hour
    RTC.setAlarm(ALM1_MATCH_MINUTES, 0, 0, 0, 1);    //daydate parameter should be between 1 and 7
    RTC.alarm(ALARM_1);                   //ensure RTC interrupt flag is cleared
    //RTC.alarmInterrupt(ALARM_1, true);

    //Set an alarm every 10th minute of every hour.
    RTC.setAlarm(ALM2_MATCH_MINUTES, 0, 10, 0, 1);    //daydate parameter should be between 1 and 7
    RTC.alarm(ALARM_2);                   //ensure RTC interrupt flag is cleared
    //RTC.alarmInterrupt(ALARM_2, true);
    return (true);    
}

// CAUTION: long loop crying for attention
void error_loop (){
    for (int i=0; i<200; i++)
        blinker();
}

void blinker() {
    digitalWrite(pump_relay, HIGH);   // active LOW
    digitalWrite(bubbler_relay, HIGH);   // active LOW  
    beep(0);
    for (int i=0; i<6; i++) {
        digitalWrite(pump_led, LOW);
        digitalWrite(bubbler_led, LOW);
        digitalWrite(watchdog_led, LOW);       
        delay(500); 
        digitalWrite(pump_led, HIGH);
        digitalWrite(bubbler_led, HIGH);
        digitalWrite(watchdog_led, HIGH);       
        delay(500);     
    }
}
