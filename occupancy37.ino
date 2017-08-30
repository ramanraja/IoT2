// Elliptic occupancy device: RCWL 0516 and TWO PIRs. Set time out through BT/USB
// Modified: LEDs are now connected to PWM pins for brightness control
// TODO: buzzer silence mode
// TODO: one relay is more conservatively operated than the other
// TODO: LDR to sense lights on/off
// Bluetooth MAC address: 00:21:13:01:04:65
#include <SoftwareSerial.h>
#include "Timer.h"

const int pir1 = 4;   
const int pir2 = 5;  
const int radar = A0;  
/**** TODO: diable the flasher for NRF24 version  ******/
const int flasher = 13;   // blink when status is sent
const int pirled1 = 6;         // blue
const int pirled2 = 9;         // orange
const int radarled = 10;        // red
const int roomstatusled = 11;   // green
const int buzzer = A2;      // active low
const int relay1  = A3;     // active low
const int relay2  = A4;  // active low

const int softTx = 3;  // TX pin for Arduino; connect the Rx of BT shield to this pin
const int softRx = 2;   // RX pin for Arduino; Tx of the BT shield
SoftwareSerial softSerial(softRx, softTx); // The order is RX, TX
#define baud 9600
//#define baud 38400 // in case 9800 is not working with new BT module

// *** Timer durations SHOULD be unsigned long int, if they are > 16 bit! ***
unsigned int statusinterval = 5*1000; // 5 seconds
unsigned int tickinterval = 100;      // in milliSec; 10 ticks=1 second 
unsigned int buzzerticks = 54*10;     // n*10 ticks=n seconds
unsigned int releaseticks = 60*10;    // n*10 ticks=n seconds  
Timer T;

int statusCode = 0;
void setup() {
  pinMode(radar, INPUT);
  pinMode(pir1, INPUT);
  pinMode(pir2, INPUT);  
  pinMode(radarled, OUTPUT); 
  pinMode(pirled1, OUTPUT);  
  pinMode(pirled2, OUTPUT);    
  pinMode(roomstatusled, OUTPUT); 
  pinMode(flasher, OUTPUT); 
  pinMode(buzzer, OUTPUT); 
  pinMode(relay1, OUTPUT); 
  pinMode(relay2, OUTPUT); 
  // Saving grace in case of spurious restart:
  digitalWrite(relay1, LOW);  // active low
  digitalWrite(relay2, LOW);  // active low
  digitalWrite(radarled, LOW);
  digitalWrite(pirled1, LOW);
  digitalWrite(pirled2, LOW);  
  digitalWrite(flasher, LOW);    
  digitalWrite(roomstatusled, LOW);
  digitalWrite(buzzer, HIGH); // active low
  Serial.begin(baud);
  softSerial.begin(baud);  
  statusCode = 0; // status 0 = 'Restarting..'  
  sendStatus();   
  // sometimes BT packets are missed. We wnat to capture the
  // restart event at any cost
  delay(20);
  softSerial.println("0"); 
  delay(20);
  softSerial.println("0"); 
  delay(20);
  blinker();    
  occupyRoom(); // program starts in occupied status
  int timerid1 = T.every(tickinterval, tick);
  T.every(statusinterval ,sendStatus);
}

void loop() {
  T.update();
}

void tick() {
  readCommand();
  updateStatus();
}

char rxbyte = 'c';   // global variable used in executeCommand() also
void readCommand () {
  if (softSerial.available() > 0) {
      rxbyte = softSerial.read();
      executeCommand();
  }
  else
      if (Serial.available() > 0){  
          rxbyte = Serial.read(); 
          executeCommand();
      }
}

char timeoutcode = 'C';  // default time out is 60 seconds
void executeCommand() { 
  switch (rxbyte) {  // rxbyte is a  global variable  **
      case 'a':
          releaseticks = 10*10; // n*10*100 mSec_ticks = n seconds
          timeoutcode = 'A';
          sendAck(timeoutcode);
          break;
      case 'b':
          releaseticks = 30*10;     
          timeoutcode = 'B'; 
          sendAck(timeoutcode);          
          break;
      case 'c':
          releaseticks = 60*10; 
          timeoutcode = 'C';
          sendAck(timeoutcode);          
          break;
      case 'd':
          releaseticks = 120*10; 
          timeoutcode = 'D';
          sendAck(timeoutcode);  
          break;      
      case 's':
          sendAck(timeoutcode);   // current timeout and status
          sendStatus();
          break;           
      default:
          sendAck(rxbyte);  // echo the erroneous command
          sendError(4);  // command error indicator
          break;      
      }
      buzzerticks = releaseticks*0.9;
      /*
      softSerial.println("");
      softSerial.println(releaseticks);
      softSerial.println(buzzerticks);      
      */
}

boolean occupied = 1;  
boolean statusbit = 0;
unsigned int tickcounter = 0;
int numhits = 0;

void updateStatus() {
    numhits = 0;
    statusbit = digitalRead(radar);
    digitalWrite(radarled, statusbit); 
    numhits += statusbit;
    statusbit = digitalRead(pir1);
    digitalWrite(pirled1, statusbit);  
    numhits += statusbit;    
    statusbit = digitalRead(pir2);
    digitalWrite(pirled2, statusbit);  
    numhits += statusbit;    
    
    if (!occupied)
        if (numhits > 1)  // at least two fired - occupy the room
            occupyRoom();
            
    if (numhits > 0)   // at least one fired; the room is in use
          tickcounter = 0;  // keep resetting it, if there is motion
             
    tickcounter++;
    
    if (tickcounter == buzzerticks) {
        if (occupied)
            buzz();
    }
    else        
    if (tickcounter == releaseticks){
         tickcounter = 0;
         if (occupied)
               releaseRoom();  
    }  
}

// Status (an integer variable):
// 0 - Arduino restarted
// 1 - occupied state
// 2 - pre-release warning (not a state, only an indicator)
// 3 - vacant state
// 4 - command error (not a state, only an indicator)
void sendStatus() {
  T.pulse(flasher, 75, LOW);
  softSerial.print(statusCode);  // always sent as character variable
  Serial.print(statusCode);    
}

void sendError(int errorcode) {
  int tempstatus = statusCode; 
  statusCode = errorcode;
  sendStatus();   
  statusCode = tempstatus;   // go back to original status *
}  

void sendAck (char ackchar) {
  softSerial.print(ackchar);  
  Serial.print(ackchar);    
}

void occupyRoom() {
  occupied = 1;
  statusCode = 1;
  digitalWrite(roomstatusled, HIGH);
  digitalWrite(relay1, LOW);  // active low
  digitalWrite(relay2, LOW);  // active low  
  T.pulse (buzzer, 300, HIGH); // active low 
  // the end state is HIGH, i.e, buzzer is off *
  sendStatus(); 
}

void releaseRoom() {
  occupied = 0;
  statusCode = 3;  
  digitalWrite(roomstatusled, LOW);
  digitalWrite(relay1, HIGH); // active low
  digitalWrite(relay2, HIGH); // active low  
  sendStatus(); 
}

// pre-release warning 
void buzz() {
  sendError(2);  // pre-release warning is clubbed with errors
  T.oscillate (buzzer,50, HIGH, 4);  
  // the end state is HIGH, i.e, buzzer is off *  
  T.oscillate (roomstatusled, 100, HIGH, 4);
  // the end state is HIGH, i.e, led is left on 
}

void blinker() {
  for (int i=0; i<6; i++) {
    digitalWrite(radarled, HIGH);
    digitalWrite(pirled1, HIGH);
    digitalWrite(pirled2, HIGH);    
    digitalWrite(roomstatusled, HIGH);
    digitalWrite(flasher, HIGH);    
    delay(100);  
    digitalWrite(radarled, LOW);
    digitalWrite(pirled1, LOW);
    digitalWrite(pirled2, LOW);    
    digitalWrite(roomstatusled, LOW);
    digitalWrite(flasher, LOW);    
    delay(100);   
  }
}

