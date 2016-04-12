/* ------------------------------------------------------------------------
 IQ Zero - Procedurally-Generated Genetic Algorithm Autonomous Micro Robot
 For ATTiny85 and the Arduino environment
 Winner of the Adafruit-Hackaday Pi Zero Contest!
 2016 @diemastermonkey (with special thanks to Charles 'Chucky D' Darwin)   
 ------------------------------------------------------------------------
 Memo
 
 v 0.10  Move to GA class, minimal fitness routine in place, "Smart Write"
         and read not yet implemented.
 
 v 0.09  Greatly simplified, miniature fitness test in main loop
         Operable as moving to a new faster model. Only one field enabled,
         actions peeled directly off PRNG. You'd think it would do nothing, but
         selection still manages to find productive combinations. 
 
 v 0.08  Finally a bit more wieldy GA structure. Greatly simplified 
         internal logic, only a simple test cycle implemented.
 
 Next: Increased emphasis on curves (retreating to Karl Simms?) - 
 because I want to explore possible "toy" applications.
 
 v 0.06  Completely refactored! Timer-based (not step-based) test cycle,
 fewer activities in mainline, fewer prng accesses, LESS flexibility for
 fields and mutations, but safer.
 ------------------------------------------------------------------------
 Serving Suggestion/Example Circuit
 
  Ground ---  Gnd  |        | Pin 0 ---- Signal | Servo 1 
                   | Atmel  | Pin 1 ---- Signal | Passive Infrared Sensor
                   |ATTiny85| Pin 2 ---- Signal | Servo 2 or ???
                   |        | Pin 3 ---- Pwr    | LED
  3.5v   ---  Pwr  |        | Pin 4 ---- Signal | Contact Switch or ???
                                                       |
                                                       |
                                                   Ground/power not shown  
 ------------------------------------------------------------------------ 
 */  
 
/* ------------------------------------------------------------------------
   Tuning
   ------------------------------------------------------------------------
 */
int iZooTotal = 5;                  // Total GAs in population
long lRandMax = 16334;              // Range of most rand outs
unsigned long lRunTimeMS =  3000;  // Lifetime of each GA in ms
// Set bonus (or penalty!) for physical world events
int iServoCost = 6;                 // Cost (from bonus) of writing to servo
int iAnalogCost = 20;               // New: Cost for digital ops
int iSensorReward = +10;
int iContactReward = -40;           // Example penalty for triggering contact
long iRootSeed = 1012;              // Initial PRNG starting point

/* ------------------------------------------------------------------------
   GA Structure
   NOTE: Populate in def *mandatory* or death @ runtime. Srsly!
   ------------------------------------------------------------------------
 */
class GAOrganism {
  public:
  // Properties
  long  Seed;
  long  Score;
  long  Charge;
  long  Threshold;
  int   OutPin;
  int   InPin;
  float Curve;
  long  OutValue;
  long  InValue;
  
  // Methods
  // Completely randomize i.e. on first run
  virtual void fnRandomize () {
    Seed = int(random(4096));
    Threshold = int(random(4096));
    OutPin = int(random(4096));
    InPin = int(random(4096));    
    OutValue = int(random(4096));
    InValue = int(random(4096));    
    Curve = (float) random(4096) / 4096;    
  } 
  
  // Prepare the GA for a sim run
  virtual void fnPrepare () {
    randomSeed(Seed);
    Score = 0;
  }
  
  // Mutate GA
  virtual void fnMutate () {
    // Smudges    
    if (int(random(5)) == 0) {Threshold += int(random(3)) - 1;}
    if (int(random(5)) == 0) {OutPin += int(random(3)) - 1;}
    if (int(random(5)) == 0) {InPin += int(random(3)) - 1;}    
    if (int(random(5)) == 0) {OutValue += int(random(3)) - 1;}
    if (int(random(5)) == 0) {InValue += int(random(3)) - 1;}
    if (int(random(5)) == 0) {Curve += (float) (random(3)) - 1;}  
    // Major changes, less common
    if (int(random(10)) == 0) {Seed = random(4096);}          
    if (int(random(10)) == 0) {Curve = (float) random(4096) / 4096;}  
    if (int(random(10)) == 0) {Threshold = random(4096);}      
    if (int(random(10)) == 0) {OutPin = random(4096);}        
    if (int(random(10)) == 0) {InPin = random(4096);}          
    if (int(random(10)) == 0) {OutValue = random(4096);}        
    if (int(random(10)) == 0) {InValue = random(4096);}              
  }    
};      // End class   

// GAOrganism *GA;     // Single instance
GAOrganism* GA[16];  // Array of pointers to GAs

/* ------------------------------------------------------------------------
   Globals
   ------------------------------------------------------------------------
 */
// Pins available to the GA
int iPins[] = {0, 1, 2, 3, 4, A1, A2, A3};
int iPinsTotal = 8;               // Fast access
int iLedPin = 3;
int iSensorPin = 1;               // Currently a PIR (DIGITAL) sends high while motion
int iSensorLast = 0;              // Most recent reading from sensor
int iContactPin = 4;              // Trigger switch or contact sensor pin
int iServoPin = 0;                // For normalization only, GA must discover what's what
// Runtime
unsigned long lLastTimeMS = 0;    // When (sys run time) current test ends
unsigned long lNowTimeMS = 0;
int iZooIndex = 0;                // Index of GA under test now
int iHighScore = 0;
int iHighScoreIndex = 0;
int iCycles = 0;                  // A counter for GA use
int iPin = 0;
int g, i; 

/* ------------------------------------------------------------------------
   Run-Once Setup
   ------------------------------------------------------------------------
 */
void setup () {
  fnHardwareTest ();
  // First generation
  pinMode (A3, INPUT);
  randomSeed (iRootSeed + analogRead (A3));
  for (i=0; i < iZooTotal; i++) {
    fnFlash (1, 100);        
    fnMutate(i);
    // GA[i]->Length = 32;        // Force reasonable length
    GA[i]->Score = 0;
  }

  iZooIndex = 0;
  fnPrepGA(iZooIndex);           // Set for first run  
  lLastTimeMS = millis();  
}

/* ------------------------------------------------------------------------
   Main
   ------------------------------------------------------------------------
 */
void loop () {

  
  // Update GA Internal state  
  if (GA[iZooIndex]->Charge < GA[iZooIndex]->Threshold) { 
    GA[iZooIndex]->Charge += 1;             // Below firing threshold
  } else {
    // Threshold reached, reset charge, fire output
    GA[iZooIndex]->Charge = 0;    
    
    // Get next output value from PRNG
    GA[iZooIndex]->OutValue = random(lRandMax);
    // Constrain output to range of pins
    iPin = random(lRandMax) % iPinsTotal;
    
    // Treat digital pins as servo    
    if (iPin < 5) {
      GA[iZooIndex]->Score -= iServoCost;    // Charge for servo action
      // g = random(lRandMax);    
      pinMode (iPins[iPin], OUTPUT);                 
      digitalWrite (iPins[iPin], HIGH);
      delayMicroseconds (GA[iZooIndex]->OutValue);
      digitalWrite (iPins[iPin], LOW);     
      delayMicroseconds (GA[iZooIndex]->OutValue);      
    } else {
      // Analog pins      
      GA[iZooIndex]->Score -= iAnalogCost;      
      pinMode (iPins[iPin], OUTPUT);                 
      analogWrite (iPins[iPin], GA[iZooIndex]->OutValue);
    }   // End inner if else  
  }     // End big if else
  
  // Scoring
  pinMode (iSensorPin, INPUT);    // PIR reward or penalty
  if (digitalRead (iSensorPin) == HIGH) {
    GA[i]->Score += iSensorReward;        
    pinMode (iLedPin, OUTPUT);    // LED on for info
    digitalWrite (iLedPin, HIGH);        
  } else {
    pinMode (iLedPin, OUTPUT);    // LED off for info
    digitalWrite (iLedPin, LOW);                
  }      
  
  pinMode (iContactPin, INPUT);   // Contact switch reward or penalty
  if (digitalRead (iContactPin) == HIGH) {
    GA[iZooIndex]->Score += iContactReward;        
  }      
  
  // If this GA test run time up
  if (millis() - lLastTimeMS > lRunTimeMS) {      
    // If highest score so far
    if (GA[iZooIndex]->Score > iHighScore) {
      iHighScore = GA[iZooIndex]->Score;
      iHighScoreIndex = iZooIndex;
    }

    // Go to next GA
    delay(100);                 // Help motions stop    
    fnFlash (iZooIndex + 1, 50);    
    delay(50);              
    iZooIndex++;
    // If end of generation    
    if (iZooIndex > iZooTotal) {        
      // Clone winner, spawn mutant siblings
      delay (100);
      fnFlash (iHighScoreIndex + 1, 200);
      delay (100);
      fnClone (iHighScoreIndex);
      for (i=1; i < iZooTotal; i++) {    // Winner unchanged
        fnMutate(i);
      }        
      iZooIndex = 0;                    // Wraparound zoo
      iHighScore = 0;                   // Reset high
      iHighScoreIndex = 0;
    } 
    // Set for next testee
    fnPrepGA(iZooIndex);           // Set for first run  

    lLastTimeMS = millis();
  }  // End if time up
}

/* ------------------------------------------------------------------------
   fnPrepGA : Prep GA for run: Seeds PRNG, peels-off gene properties, resets states
   ------------------------------------------------------------------------
 */
void fnPrepGA (int iArgIndex) {
  randomSeed (GA[iArgIndex]->Seed);  
//  GA[iArgIndex]->Score = 0;
//  GA[iArgIndex]->Charge = 0;    
}

/* ------------------------------------------------------------------------
   fnMutate : Mutate (arg) GA (by index) - Re-seeds PRNG
   ------------------------------------------------------------------------
 */
void fnMutate (int iArgIndex) {
  randomSeed (iRootSeed + analogRead (A3));
  // Smudges    
  if (int(random(3)) == 0)       
    GA[iArgIndex]->Threshold += int(random(3)) - 1;
  if (int(random(3)) == 0)     
    GA[iArgIndex]->OutPin += int(random(3)) - 1;
  if (int(random(3)) == 0)       
    GA[iArgIndex]->InPin += int(random(3)) - 1;
  if (int(random(3)) == 0)       
    GA[iArgIndex]->Curve += int(random(3)) - 1;
  
  // Major changes, less common
  if (int(random(10)) == 0) 
    GA[iArgIndex]->Threshold = random (lRandMax);  
  if (int(random(10)) == 0) 
    GA[iArgIndex]->OutPin = random (lRandMax);  
  if (int(random(10)) == 0) 
    GA[iArgIndex]->InPin = random (lRandMax);  
  if (int(random(10)) == 0) 
    GA[iArgIndex]->Curve = random (lRandMax);        
    
  // For seed, any change is major
  if (int(random(10)) == 0)       
    GA[iArgIndex]->Seed += int(random(3)) - 1;        
  if (int(random(10)) == 0) 
    GA[iArgIndex]->Seed = random (lRandMax);    
}

/* ------------------------------------------------------------------------
   fnClone : Copy arg1 GA into all other "cages"
   ------------------------------------------------------------------------
 */
void fnClone (int iArgIndex) {
  for (i=0; i < iZooTotal; i++) {
    if (i != iArgIndex) {          // Superfluous
      GA[i]->Seed = GA[iArgIndex]->Seed;
      // GA[i]->Length = GA[iArgIndex]->Length;
      GA[i]->Threshold = GA[iArgIndex]->Threshold;    
      GA[i]->OutPin = GA[iArgIndex]->OutPin;    
      GA[i]->InPin = GA[iArgIndex]->InPin;    
      GA[i]->Curve = GA[iArgIndex]->Curve;          
    }
  }
}

/* ------------------------------------------------------------------------
   fnFlash : Simple flasher
   ------------------------------------------------------------------------
 */
void fnFlash (int iTimes, float iDelaySeconds) {
  if (iTimes < 1) { 
    return; 
  }
//  pinMode (iLedPin, OUTPUT);  
  digitalWrite (iLedPin, LOW);      // Start off
  // delayMicroseconds (iDelay);  
  for (int i=0; i < iTimes; i++) {
    digitalWrite (iLedPin, HIGH);
    delay (iDelaySeconds);
    digitalWrite (iLedPin, LOW);
    delay (iDelaySeconds);
  }
}

/* ------------------------------------------------------------------------
   fnHardwareTest : Quick blind hardware test for startup etc
   ------------------------------------------------------------------------
*/
void fnHardwareTest () {
  for (int i=0; i < 5; i++) {
    pinMode(i, OUTPUT);
    for (int s=0; s < 280; s += 20) {
      digitalWrite (i, HIGH);
      delayMicroseconds (s);
      digitalWrite (i, LOW);
      delayMicroseconds (20000);
    }  
    digitalWrite (i, HIGH);
    delayMicroseconds (90);
    digitalWrite (i, LOW);
    delayMicroseconds (20000);    
  }  
}
