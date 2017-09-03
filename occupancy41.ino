// Elliptic occupancy device: TWO RCWL 0516s and TWO PIRs. 
// MODE : Run the regular program with one/two sensors, or just LED display
// status is not in a separate timer event now; done by a counter in main loop
// TODO: buzzer silence mode
// TODO: operate one relay more conservatively than the other
// TODO: LDR to sense lights on/off
// Elliptic sensor Bluetooth MAC address: 00:21:13:01:04:65
#include <SoftwareSerial.h>
#include "Timer.h"

const int pir1 = 4;   
const int pir2 = 5;  
const int radar1 = A0;       // front radar
const int radar2 = 12;       // back plane radar
/* TODO: diable the flasher for NRF24 version  */
const int flasher = 13;      // blink when status is sent
const int pirled1 = 6;       // blue
const int pirled2 = 9;       // orange
const int radarled1 = 10;    // red
const int radarled2 = 11;    // green
const int buzzer = A2;       // active low
const int relay1 = A3;       // active low
const int relay2 = A4;       // active low

const int softTx = 3;  // TX pin for Arduino; connect the Rx of BT shield to this pin
const int softRx = 2;   // RX pin for Arduino; Tx of the BT shield
SoftwareSerial softSerial(softRx, softTx); // The order is RX, TX
#define baud 9600
//#define baud 38400 // in case 9800 is not working with new BT module

// *** Timer durations SHOULD be unsigned long int, if they are > 16 bit! ***
unsigned int tickinterval = 100;     // in milliSec; 10 ticks=1 second 
unsigned int statusticks = 5*10;     // n*10 ticks=n seconds (NEW DEFINITION)
unsigned int buzzerticks = 54*10;    // n*10 ticks=n seconds
unsigned int releaseticks = 60*10;   // n*10 ticks=n seconds  

Timer T;
int timerid;
boolean bothrelays = 0;  // operate only the first relay or both
int statusCode = 0; // 1=occupied; 3=free
int MODE = 'X';  // overall program operating mode:
// X - occupancy detector with radar1
// Y - occupancy detector with radar2
// Z - occupancy detector with both radars
// W - just display the status of the 4 sensors; do nothing else
void setup() {
  pinMode(radar1, INPUT);
  pinMode(radar2, INPUT);  
  pinMode(pir1, INPUT);
  pinMode(pir2, INPUT);  
  pinMode(radarled1, OUTPUT); 
  pinMode(radarled2, OUTPUT);   
  pinMode(pirled1, OUTPUT);  
  pinMode(pirled2, OUTPUT);    
  pinMode(flasher, OUTPUT); 
  pinMode(buzzer, OUTPUT); 
  pinMode(relay1, OUTPUT); 
  pinMode(relay2, OUTPUT); 
  // Saving grace in case of spurious restart:
  digitalWrite(relay1, LOW);  // active low-ON
  if (bothrelays)
      digitalWrite(relay2, LOW);  // active low-ON
  else      
      digitalWrite(relay2, HIGH);  // OFF
  digitalWrite(radarled1, LOW);
  digitalWrite(radarled2, LOW);  
  digitalWrite(pirled1, LOW);
  digitalWrite(pirled2, LOW);  
  digitalWrite(flasher, LOW);    
  digitalWrite(buzzer, HIGH); // active low-OFF
  Serial.begin(baud);    // IoT sensor output to the network-stop gap arrangement
  softSerial.begin(baud);  
  blinker();   
  sendInitialStatus(); 
  occupyRoom(); // program starts in occupied status
  timerid = T.every(tickinterval, tick);
  //T.every(statusinterval ,sendStatus);
}

void loop() {
  T.update();
}

// Indicate when the Arduino is restarting 
void sendInitialStatus() {
  statusCode = 0; // status 0 = 'Restarting..'  
  sendStatus();   
  // sometimes BT packets are missed. We wnat to capture the
  // restart event at any cost. (Cludge !)
  delay(20);
  sendStatus();
  delay(20);
}

void tick() {
  readCommand();
  readSensors();
  if (MODE != 'W')
      updateStatus();
}

char rxbyte = 'c';   // global variable used by executeCommand() also
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
          buzzerticks = releaseticks*0.9;
          timeoutcode = 'A';
          sendAck(timeoutcode);
          break;
      case 'b':
          releaseticks = 30*10;   
          buzzerticks = releaseticks*0.9;  
          timeoutcode = 'B'; 
          sendAck(timeoutcode);          
          break;
      case 'c':
          releaseticks = 60*10; 
          buzzerticks = releaseticks*0.9;
          timeoutcode = 'C';
          sendAck(timeoutcode);          
          break;
      case 'd':
          releaseticks = 120*10; 
          buzzerticks = releaseticks*0.9;
          timeoutcode = 'D';
          sendAck(timeoutcode);  
          break;      
      case 's':
          sendAck(timeoutcode);   // send current timeout and status
          sendStatus();
          break;           
      case 'x':
          MODE = 'X';    
          sendAck(MODE);   
          break;  
      case 'y':
          MODE = 'Y';    
          sendAck(MODE);     
          break;          
      case 'z':
          MODE = 'Z';    
          sendAck(MODE);   
          break; 
      case 'w':
          MODE = 'W';    
          sendAck(MODE);   
          releaseRoom();      
          break;             
      case 'u': 
          bothrelays = 1; // will take effect from the next occupancy change
          sendAck('U');   
          break;     
      case 'v': 
          bothrelays = 0;
          sendAck('V');   
          // immediately release the second relay, if it was ON
          digitalWrite(relay2, HIGH); // active low    
          // TODO: move this to a separate function       
          break;            
      default:
          sendAck(rxbyte);  // echo the erroneous command
          sendDirect(4);  // command error indicator
          break;      
      }
}

boolean occupied = 1;  
boolean statusbit = 0;
unsigned int tickcounter = 0;
int numhits = 0;  // global variable
// always read and display the 4 sensors; but use them depending on MODE
void readSensors() {
    numhits = 0;
    statusbit = digitalRead(pir1);
    digitalWrite(pirled1, statusbit);  
    numhits += statusbit;    
    statusbit = digitalRead(pir2);
    digitalWrite(pirled2, statusbit);  
    numhits += statusbit;      
    statusbit = digitalRead(radar1);
    digitalWrite(radarled1, statusbit); 
    if (MODE=='X' || MODE=='Z')
        numhits += statusbit;
    statusbit = digitalRead(radar2);
    digitalWrite(radarled2, statusbit); 
    if (MODE=='Y' || MODE=='Z')
        numhits += statusbit;  
}

// uses global variable numhits
void updateStatus() {    
    if (!occupied)
        if (numhits > 1)  // at least two sensors fired - occupy the room
            occupyRoom();
    if (numhits > 0)   // at least one fired; the room is in use
          tickcounter = 0;  // keep resetting it, if there is any motion
    tickcounter++;
    if ((tickcounter % statusticks)==0) 
        sendStatus();
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
// status 2 and 4 are sent through sendDirect()
void sendStatus() {
  T.pulse(flasher, 75, LOW);
  softSerial.print(statusCode);  // always sent as character variable
  Serial.print(statusCode);    
}

//input is integer
void sendDirect(int codenumber) {
  softSerial.print(codenumber);  // always send as character variable
  Serial.print(codenumber);  
}  

// input is char
void sendAck (char ackchar) {
  softSerial.print(ackchar);  
  Serial.print(ackchar);    
}

void occupyRoom() {
  occupied = 1;
  statusCode = 1;
  digitalWrite(relay1, LOW);  // active low
  if (bothrelays)
    digitalWrite(relay2, LOW);  // active low  
  T.pulse (buzzer, 300, HIGH); // active low  // 100
  // the end state is HIGH, i.e, buzzer is off *
  sendStatus(); 
}

void releaseRoom() {
  occupied = 0;
  statusCode = 3;  
  digitalWrite(relay1, HIGH); // active low
  if (bothrelays)  
    digitalWrite(relay2, HIGH); // active low  
  sendStatus(); 
}

// pre-release warning 
void buzz() {
  sendDirect(2);  // pre-release warning is clubbed with errors
  T.oscillate (buzzer,50, HIGH, 4);  
  // the end state is HIGH, i.e, buzzer is off *  
  T.oscillate (flasher, 100, HIGH, 8);
  // the end state is HIGH, i.e, led is left on 
}

void blinker() {
  for (int i=0; i<6; i++) {
    digitalWrite(radarled1, HIGH);
    digitalWrite(pirled1, HIGH);
    digitalWrite(pirled2, HIGH);    
    digitalWrite(radarled2, HIGH);
    digitalWrite(flasher, HIGH);    
    delay(100);  
    digitalWrite(radarled1, LOW);
    digitalWrite(pirled1, LOW);
    digitalWrite(pirled2, LOW);    
    digitalWrite(radarled2, LOW);
    digitalWrite(flasher, LOW);    
    delay(100);   
  }
}

