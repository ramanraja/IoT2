// The correct version: occupancy simulator with start/stop: will help to develop python serial code
#include "Timer.h"
#include <SoftwareSerial.h>

Timer T;
int activetimer = 1;

const int led1 = 9;
const int led2 = 10;
const int softTx = 3;  // TX pin for Arduino; connect the Rx of BT shield to this pin
const int softRx = 2;  // RX pin for Arduino; Tx of the BT shield
SoftwareSerial softSerial(softRx, softTx); // The order is RX, TX
char *help = "(a)Serial ON (b)Serial OFF (c)Counter (d)Random (e) None";

void setup() {
  pinMode(led1, OUTPUT);  
  pinMode(led2, OUTPUT);    
  blinker();
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);   
  Serial.begin(9600);
  softSerial.begin(9600);
  Serial.print(0); // 0 indicates 'Arduino has restarted' 
  softSerial.println(help); 
  softSerial.println(help); 
  T.every(50, tick);
  startRand();
  //startNext();
}

void loop() {
  T.update();
}

char rxbyte;
boolean serialon = 1;

void tick() {
  if (softSerial.available() > 0) {
      rxbyte = softSerial.read();  
      switch (rxbyte) {
        case 'a':
          if (!serialon)
              Serial.begin(9600);
           serialon = 1;
           softSerial.println("serial is on");
           break;
        case 'b':
            if (serialon)        
               Serial.end();  
           serialon = 0;      
           softSerial.println("serial is off");
           break;
        case 'c':
           startNext();
           break;
        case 'd':
            startRand();
            break;
         case 'e':
             stopTimers();
             break;
        default:
             softSerial.println(help);
             break;
      }
   }
}

bool flag1 = 0; // to blink the LED
bool flag2 = 0; // to blink the LED

void startRand() {
   activetimer = 1;
   digitalWrite(led2, LOW);
   flag2 = 0;  // catch the last stray timer tick
   T.after(100, sendRand);           
   softSerial.println("random counter");  
}

void startNext() {
    activetimer = 2;       
    digitalWrite(led1, LOW);
    flag1 = 0;  // catch the last stray timer tick
    T.after(100, sendNext);            
    softSerial.println("regular counter");  
}

void stopTimers() {
    activetimer = 25;  // anything non-existant     
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);        
    flag1 = flag2 = 0;
    softSerial.println("counters off");  
}

int counter = 0;
void sendNL () {
  counter++;
  if (counter==50) {
      counter = 0;
      Serial.println(" ");
   }
}

unsigned int MININTERVAL[3] = {5, 25, 2000};
unsigned int MAXINTERVAL[3] = {25,2000,10*000};     // 5*60*1000;
unsigned int randinterval = 100;
int data = 0;
int block = 0;

void sendRand() {
  data = random(1, 4);  // data can be 1,2 or 3  
  Serial.print(data);
  digitalWrite(led1, flag1);
  flag1 = !flag1;
  block = random(0,3); // block 0,1 or 2 
  randinterval = random(MININTERVAL[block], MAXINTERVAL[block]); //stratified random  
  sendNL (); // only needed for terminal console
  if (activetimer == 1)
      T.after(randinterval, sendRand);
}

void sendNext(){
  data = (data+1)%10;
  Serial.print(data);
  digitalWrite(led2, flag2);
  flag2 = !flag2;
  sendNL (); // only needed for terminal console
  if (activetimer == 2)
      T.after(500, sendNext);  
}

void blinker() {
  for (int i=0; i<6; i++) {
     digitalWrite (led1, HIGH);
     digitalWrite (led2,LOW);    
    delay(100);
     digitalWrite (led1,LOW);
     digitalWrite (led2, HIGH);    
     delay(100);
  }
     digitalWrite (led2,LOW);    
}



