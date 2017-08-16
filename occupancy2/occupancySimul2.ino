// occupancy simulator - will help to develop python serial code
#include "Timer.h"
Timer T;
const int led = A5;
bool flag = 0;

void setup() {
  pinMode(led, OUTPUT);  
  Serial.begin(9600);
  Serial.print(-1);
  blinker();
  digitalWrite(led, LOW);
  T.after(100, sendNext);
  //T.after(100, sendRand);
}

void loop() {
  T.update();
}

unsigned int MININTERVAL[3] = {5, 25, 2000};
unsigned int MAXINTERVAL[3] = {25,2000,10*000};     // 5*60*1000;
unsigned int randinterval;
int data = 0;
int block = 0;

void sendRand() {
  block = random(0,3); // block 0,1 or 2 
  randinterval = random(MININTERVAL[block], MAXINTERVAL[block]);
  data = random(0, 3);  // data can be 0,1 or 2  
  Serial.print(data);
  digitalWrite(led, flag);
  flag = !flag;
  T.after(randinterval, sendNext);
}

void sendNext(){
  data = (data+1)%10;
  Serial.print(data);
  digitalWrite(led, flag);
  flag = !flag;
  T.after(500, sendNext);  
}


void blinker() {
  for (int i=0; i<4; i++) {
    digitalWrite (led, HIGH);
    delay(500);
    digitalWrite (led,LOW);
    delay(500);
  }
}


