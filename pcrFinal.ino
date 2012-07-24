#include <max6675.h>

/* PCR code to cycle temperatures for denaturing, annealing and extending DNA.
Stacey Kuznetsov (stace@cmu.edu) and Matt Mancuso (mcmancuso@gmail.com)
July 2012


*** All temperatures at in degrees Celcius.
*/


/* PCR VARIABLES*/
double DENATURE_TEMP = 94;
double ANNEALING_TEMP = 60.00;
double EXTENSION_TEMP = 72;

// Phase durations in ms. I suggest adding 3-5 seconds to
// the recommended times because it takes a second or two
// for the temps to stabilize
unsigned int DENATURE_TIME = 33000;
unsigned int ANNEALING_TIME= 33000;
unsigned int EXTENSION_TIME = 35000;

// Most protocols suggest having a longer denature time during the first cycle
// and a longer extension time for the final cycle.
unsigned int INITIAL_DENATURE_TIME = 300000;
unsigned int FINAL_EXTENSION_TIME = 600000;

// how many cycles we should do. (most protocols recommend 32-35)
int NUM_CYCLES = 32;  
 
/* Hardware variables */
int heatPin = 7;  // pin that controls the relay w resistors

// Thermocouple pins
int thermoDO = 4;
int thermoCS = 5;
int thermoCLK = 6;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);


int fanPin = 9; // pin for controling the fan
 

//safety vars
short ROOM_TEMP = 18; // if initial temp is below this, we assume thermocouple is diconnected or not working
short MAX_ALLOWED_TEMP = 100; // we never go above 100C
double MAX_HEAT_INCREASE = 2.5; // we never increase the temp by more than 2.5 degrees per 650ms


/* stuff that the program keeps track of */
short CURRENT_CYCLE = 0; // current cycle (kept track by the program)

// current phase. H-> heating up
char CURRENT_PHASE='H'; 

unsigned long time;  // used to track how long program is running
double curTemp; // current temperature


/* Print out current phase, temperature, how long it's been running, etc
startTime -> when specific phase started running in ms
*/
void printTempStats(unsigned long startTime) {
  
   unsigned long timeElapsed = millis() - startTime;
   Serial.print("CCL:");
   Serial.print(CURRENT_CYCLE);
   Serial.print(" PHS:");
   Serial.print(CURRENT_PHASE);
   Serial.print(" ET:");
   Serial.print(timeElapsed);
   Serial.print(" TT:");
   Serial.print(millis());
   Serial.print(" TMP:");
   Serial.println(curTemp);
}

/* Heat up to the desired temperature.
maxTemp-> Temperature we should heat up to
printTemps -> whether or not we should print debug stats
*/
boolean heatUp(double maxTemp, boolean printTemps = true){
  unsigned long startTime = millis(); 
  double prevTemp = thermocouple.readCelsius();
  curTemp = thermocouple.readCelsius();
  if (curTemp < ROOM_TEMP) {
   Serial.println("STARTING TMP TOO LOW");
   Serial.println(prevTemp);
   return false;
  }
  int curIteration = 0;
  
  while (curTemp < maxTemp) {
    curIteration++;
    int pulseDuration = min(650, ((600*(maxTemp-curTemp))+80)); // as we approach desired temp, heat up slower
    digitalWrite(heatPin, HIGH);
    delay(pulseDuration);
    digitalWrite(heatPin, LOW);
    
    curTemp=thermocouple.readCelsius();
    if(curTemp >= maxTemp)
       break;
    if(printTemps) {
       printTempStats(startTime);
    }

   if((maxTemp-curTemp) < 1 || curIteration % 30 == 0) {
     // as we approach desired temperature, wait a little bit longer between heat
     // cycles to make sure we don't overheat. It takes a while for heat to transfer
     // between resistors and heating block. As a sanity check, also do this every 20
     // iterations to make sure we're not overheating
      do  {
        prevTemp = curTemp;
        delay(250); 
        curTemp = thermocouple.readCelsius();
      }      while(curTemp > prevTemp);
    }

   if(curTemp >= maxTemp)
     break;
     
   if ((curIteration%2) == 0) {
    if(curTemp < (prevTemp-1.25)) {
      Serial.print("Temperature is not increasing... ");
      Serial.print(curTemp);
      Serial.print("   ");
      Serial.println(prevTemp);
      return false; 
    }
   } else {   
      prevTemp = curTemp;
   }
     
   while ((curTemp-prevTemp) >= MAX_HEAT_INCREASE) {
     prevTemp = curTemp;
     Serial.print("HEATING UP TOO FAST! ");
     delay(1000);
     curTemp = thermocouple.readCelsius();
     Serial.println(curTemp);
   }
   
   while(curTemp >= MAX_ALLOWED_TEMP) {
     delay(1000);
     Serial.print("OVERHEATING");
     Serial.println(curTemp);
   }
   
  } 
  return true;
}


/* Cool down to the desired temperature by turning on the fan. 
minTemp-> Temperature we want to cool off to
maxTimeout -> how often we poll the thermocouple (300ms is good)
printTemps -> whether or not to print out stats
*/
void coolDown(double minTemp, int maxTimeout = 300, boolean printTemps = true) {  
  unsigned long startTime = millis(); 
  while ((curTemp = thermocouple.readCelsius()) > (minTemp+0.75)) {
    // I've found that turning off the fan a bit before the minTemp is reached
    // is best (because fan blades continue to rotate even after fan is turned off.
    digitalWrite(fanPin, HIGH);
    if(printTemps) {
       printTempStats(startTime);
     }
   delay(maxTimeout);
   } 
   digitalWrite(fanPin, LOW);
}


/* 
Try to stay close to the desired temperature by making micro adjustments to the 
resistors and fan. Assumes that the current temperature is already very close
to the idealTemp.
idealTemp -> desired temperature
duration ->  how long we should keep the temperature (in ms)
*/
boolean holdConstantTemp(long duration, double idealTemp) {
  unsigned long startTime = millis();
  long timeElapsed = millis() - startTime;
  // keep track of how much time passed
  while (timeElapsed < duration) {
    curTemp = thermocouple.readCelsius();
    printTempStats(startTime);
      if(curTemp < idealTemp) {  
        // turn up the heat for 90ms if the temperature is cooler
        digitalWrite(heatPin, HIGH);
        delay(90);
        digitalWrite(heatPin, LOW);
      } else if (curTemp > (idealTemp+0.5)) {
        // turn up the fan if the temp is too high
        // generally if temp is within 0.5degrees, don't use the fan
        // waiting for the temp to cool naturally seems to be more stable
        digitalWrite(fanPin, HIGH);
        delay(90);
        digitalWrite(fanPin, LOW);
      }
     delay(210);
     timeElapsed = millis() - startTime;
  }
  return true; 
}

/* Execute the desired number of PCR cycles.
*/
void runPCR() {
  for (; cycles < NUM_CYCLES; cycles++) {
    CURRENT_CYCLE = cycles;
    unsigned long cycleStartTime = millis();
    Serial.print("///CYCLE  ");
    Serial.print(cycles);
      
    time = millis();
    Serial.println("HEATING UP");
    CURRENT_PHASE='H';
    if(!heatUp(DENATURE_TEMP)){
      // if unable to heat up, stop
      Serial.println("Unable to heat up... something is wrong :(");
      cycles = NUM_CYCLES;
      break;
    }
    
    long dif = millis() - time;
    Serial.print("***TOTAL HEAT TIME ");
    Serial.println(dif);
    Serial.println();
   
    time = millis();
    Serial.println("DENATURATION");
    CURRENT_PHASE='D';
    if(cycles > 0) {
      holdConstantTemp(DENATURE_TIME, DENATURE_TEMP);
    } else {
      // if this is the first cycles, hold denature temp for longer
      holdConstantTemp(INITIAL_DENATURE_TIME, DENATURE_TEMP);
    }
    Serial.println();
  
    Serial.println("COOLING");
    time = millis();
    CURRENT_PHASE='C';
    coolDown((ANNEALING_TEMP));
    dif = millis()-time;
    Serial.print("***TOTAL COOLING TIME ");
    Serial.println(dif);
    Serial.println();
     
    Serial.println("ANNEALING");
    time = millis();
    CURRENT_PHASE='A';
    holdConstantTemp(ANNEALING_TIME, ANNEALING_TEMP);
    dif = millis()-time;
    Serial.print("***TOTAL ANNEALING TIME ");
    Serial.println(dif);
    Serial.println();
    
    
    Serial.println("HEATING UP");
    time =millis();
    CURRENT_PHASE='D';
    heatUp((EXTENSION_TEMP));
    dif = millis()-time;
    Serial.print("***TOTAL HEAT UP TIME IS ");
    Serial.println(dif);
    Serial.println();
  
     
    Serial.println("EXTENSION");
    time = millis();
    CURRENT_PHASE='E';
    if (cycles<(NUM_CYCLES-1)) {
      holdConstantTemp(EXTENSION_TIME, EXTENSION_TEMP);
    } else {
       // if this is the last cycle, hold extension temp for longer
       holdConstantTemp(FINAL_EXTENSION_TIME, EXTENSION_TEMP);
    }
    dif = millis()-time;
    Serial.print("***TOTAL EXTENSION TIME IS ");
    Serial.println(dif);
    Serial.println();
    Serial.println();
    
    Serial.print("///TOTAL CYCLE TIME: ");
    Serial.println(millis()-cycleStartTime);
    Serial.println();
} 
    
  Serial.println("DONE");
}


/* Set up all pins */
void setup() {
  
 Serial.begin(9600);  
 
 pinMode(heatPin, OUTPUT); 
 digitalWrite(heatPin, LOW);  
 pinMode(fanPin, OUTPUT);
 digitalWrite(fanPin, LOW);
 
 // time out for 5 seconds.
 Serial.println("Starting in");
 for (int i = 5; i > 0; i--) {
   Serial.print(i);
   Serial.print("... ");
   delay(1000);
 }
 Serial.println();
 runPCR();
}


int cycles = 0;


void loop() {
      
 //nothing
}
