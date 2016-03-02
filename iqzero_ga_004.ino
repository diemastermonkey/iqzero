/* ------------------------------------------------------------------------
   IQ Zero (fka Wurm)
   Procedurally-Generated Genetic Algorithm Autonomous Micro Robot
   For ATTiny85 and the Arduino environment
   Winner of the Adafruit/Hackaday Pi Zero Contest!
   2016 @diemastermonkey (with special thanks to Charles Darwin)   
   ------------------------------------------------------------------------
*/

/* ------------------------------------------------------------------------
   Memo
   ------------------------------------------------------------------------
   v 0.06  Completely refactored! Timer-based (not step-based) test cycle,
   fewer activities in mainline, fewer prng accesses, LESS flexibility for
   fields and mutations, but safer.
*/

/* ------------------------------------------------------------------------
   GA Structure
   ------------------------------------------------------------------------
*/
// New: Not yet implemented
typedef struct GA_Structure {
  // Basic DNA
  int iScore = 0;
  unsigned long Seed = 0;
  unsigned long Length = 0;
  // Peeled-off top of its prng
  long Charm = 0;
  long Fave = 0;
  long Curve = 0;  
} GA[16];

// GA_Structure GA[16];    // Builds, but doesn't run on device!

unsigned long lScore[16];
unsigned long lSeed[16];
unsigned long lLength[16];
// Static procgen GA properties that are peeled-off the top of its prng
long lCharm = 0;
long lFave = 0;
long lCurve = 0;
// Writable buffers for GA under test
long lIn = 0;
long lAdd = 0;
long lOut = 0;
long lHold = 0;
long lCount = 0;
long lLimit = 0;
long lOutPin = 0;
long lInPin = 0;
// Indexes (into fields) of three fields for cmds to operate on/with
int iReadField = 0, iWriteField = 0, iOpField = 0;

/* ------------------------------------------------------------------------
   Globals
   ------------------------------------------------------------------------
*/

// Zoo parameters
int iZooTotal = 3;               // How many GAs live in the zoo
int iZooCurrent = 0;             // Index of GA currently under test

// Lifetime/environment/runtime stuff
long lRunTimeMS =  2000;          // GA lifetime in ms
long lLastTimeMS = 0;           // When (sys run time) current test ends
long lNowTimeMS = 0;
int iCommandTypes = 64;         // Total command types prng use, with 'defaults'

// Pins available to the GA
int iPins[] = {0, 1, 2, 3, 4, A1, A2, A3};
int iPinsTotal = 8;              // Fast access

// System stuff (not known to GA)
long lSalt = 1890;
unsigned long lRandMax = 256;   // Max size of a rand on target platform
unsigned long lRandLast = 0;      // Most recent value returned from prng
int iSensorLast = 0;              // Most recent reading from sensor
int iLedPin = 3;
int iSensorPin = 1;               // Currently a PIR (DIGITAL) sends high while motion
int iContactPin = 4;              // Trigger switch or contact sensor pin
int iServoPin = 0;                // For normalization only, GA must discover what's what
int iChargedPin = -1;             // When servo charge done, set this pin low
int iServoCharge = 0;
long lCycleCount = 0;             // In-test cycle counter for GAs to read

/* To do: Store leader in EEprom
   #include <EEPROM.h>
   V = EEPROM.read(Addr); EEPROM.write(Addr, V);
*/

/* ------------------------------------------------------------------------
   Run-Once Setup
   ------------------------------------------------------------------------
*/
void setup () {
  fnFlash (1, 100);      
  fnHardwareTest ();
  fnFlash (2, 100);    
  
  randomSeed (analogRead(A0) + lSalt);
  for (int i=0; i < iZooTotal; i++) {
    lScore[i] = 0;    
    fnRandGA(i);
    fnSanityGA (i);
  }
  
  fnFlash (3, 100);  
  iZooCurrent = 0;  
  fnPrepFitness(iZooCurrent);
  lLastTimeMS = millis();   // So we know when test over      
}

/* ------------------------------------------------------------------------
   Main
   ------------------------------------------------------------------------
*/
void loop () {
  fnFlash (1, 0.05);
  
  // Execute the next cmd-appropriate value from the prng
  fnExecute (fnRandNext (1, iCommandTypes));   
  
  // Handle GA's internal looping: Reset its prng/count if length reached
  lCycleCount++;
  if (lCycleCount > lLength[iZooCurrent]) {
    fnPrepFitness(iZooCurrent);
    lCycleCount = 0;
  }

  // Accumulate score
  lScore[iZooCurrent] += fnScoreLive();
  
  // Handle end of test cycle
  if (millis() - lLastTimeMS > lRunTimeMS) {
    fnFlash (iZooCurrent + 1, 5);    
    if (fnNextFitness() == -1) {
      //
      // Zoo complete: Pick winner here
      int iHighIndex = fnSelectWinner();
      fnFlash (iHighIndex + 1, 500);
      // Dupe winner to all slots. To do: store winner in eeprom   
      for (int index=0; index < iZooTotal; index++) {   // Iterate all cages in zoo      
          lSeed[index] = lSeed[iHighIndex];
          lLength[index] = lLength[iHighIndex];
      }

      // Mutate all GAs except 0th cage
      fnMutateAll();
      
      // Reset for next generation
      iZooCurrent = 0;
    }
    
    // Ready the next victim
    fnPrepFitness(iZooCurrent);
    lScore[iZooCurrent] = 0;
    lLastTimeMS = millis();   // So we know when test over      
  }
}                            // End main loop

/* ------------------------------------------------------------------------
   fnPrepFitness : Prepare GA and environment for a test run
   ------------------------------------------------------------------------
*/
void fnPrepFitness (int iArgIndex) {
  // Plant GA's seed, reset score
  randomSeed (lSeed[iArgIndex]);
  // Moved // lScore[iArgIndex] = 0;
  
  // The first few values become its initial properties ("morphology"?)
  lCharm = random(lRandMax);
  lFave = random(lRandMax);
  lCurve = random(lRandMax) / lRandMax;  // A floating point result
  lIn = random(lRandMax);
  lOut = random(lRandMax);  
  lHold = random(lRandMax);    
  lCount = random(lRandMax);      
  lLimit = random(lRandMax);        
  
  // Set initial outpin/inpins
  lOutPin = random(iPinsTotal);
  lInPin = random(iPinsTotal);  
  
  // Reset counter (for GA's info only)
  lCycleCount = 0;
}

/* ------------------------------------------------------------------------
   fnFitnessNext : Prep for a single fitness run of (arg) GA
   Returns index of current test subject or -1 if end of zoo
   ------------------------------------------------------------------------
*/
int fnNextFitness () {
  iZooCurrent++;
  if (iZooCurrent >= iZooTotal) {
    iZooCurrent = 0;
    return (-1);            // Inform caller, zoo completed
  }
  
  //    INSERT OTHER PREP WORK HERE
  return (iZooCurrent);    // Superfluous, but eases conversion later
}

/* ------------------------------------------------------------------------
   fnSelectWinner : Return index of best performer
   ------------------------------------------------------------------------
*/
int fnSelectWinner () {
  // Select highest score
  int iHighest = -1; int iWinner = 0;
  for (int iCage=0; iCage < iZooTotal; iCage++) {
    if (lScore[iCage] > iHighest) {
      iHighest = lScore[iCage];            // Tha new winna!
      iWinner = iCage;
    }
  }  // End for   
  return (iWinner);
}    // End function


/* ------------------------------------------------------------------------
   fnRandNext : Return the Xth next 'random' value from the PRNG, within range
   Also maintains "last" and other values. Send "0" as skip for no skip
   ------------------------------------------------------------------------   
*/
int fnRandNext (int iArgSkip, long lArgRange) {
  int lRetval = 0;
  for (int i=0; i < iArgSkip; i++) {
     lRandLast = random (lArgRange);    
  }
  return (lRandLast);
}

/* ------------------------------------------------------------------------
   fnScoreLive: In-cycle tests for accumulating score during test
   Returns a value to be *added* to the GA's score, thus supporting
   penalties later. Use 0 for no bonus.   
   ------------------------------------------------------------------------
*/
int fnScoreLive (void) {
  int iScore = 0;

  // Sensor test
  pinMode (iSensorPin, INPUT);  
  // Example for an LDR i.e. "analog value changed"
  // iTemp = int (analogRead (iSensorPin) / 10);    
  // For digital PIR that sends continuous HIGH while motion detect
  iSensorLast = digitalRead(iSensorPin);  
  if (iSensorLast == HIGH) { 
    iScore += 1;                       // Doubled to ensure it weighs more than servo bonus
    fnFlash (1, 0.05);
  }  

  // if switch  pressed (*right now*), big points (to do, switch to hwint)
  pinMode (iContactPin, INPUT);          // Always needed as GA may change?
  if (digitalRead (iContactPin) == HIGH) {
    iScore += 10;
  }

  //
  // Insert more tests here
  //
  return (iScore);
}

  
/* ------------------------------------------------------------------------
   fnFlash : Simple flasher w/fractional seconds
   ------------------------------------------------------------------------
*/
void fnFlash (int iTimes, float iDelaySeconds) {
  pinMode (iLedPin, OUTPUT);  
  int iDelay = 10000 * iDelaySeconds;
  digitalWrite (iLedPin, LOW);      // Start off
  delayMicroseconds (iDelay);  
  for (int i=0; i < iTimes; i++) {
    digitalWrite (iLedPin, HIGH);
    delayMicroseconds (iDelay);
    digitalWrite (iLedPin, LOW);
    delayMicroseconds (iDelay);
  }
 delayMicroseconds (iDelay);
}

/* ------------------------------------------------------------------------
   fnServoTask : "Task" a servo on argpin to a destination (to do: unblocking)
   ------------------------------------------------------------------------
*/
void fnServoTask (int iArgPinIndex, int i) {
  if (iArgPinIndex < 0 || i < 0) { return; }    
  // Normalize pin to range of possible pins
  int iPinIndex = abs (iArgPinIndex % iPinsTotal);
  int iPin = iPins[iPinIndex];        // Faster
  
  iServoCharge = i;                     // Charge counter and send high to servo
  iChargedPin = iPin;                // Store for fnServoProcess to use later
  digitalWrite (iPin, HIGH);  
  delayMicroseconds (i);                // KLUDGE FIX? Might not be doing anything now
  digitalWrite (iPin, LOW);      // Removed to reinstate fnServoProcess?
  delayMicroseconds (20000);            // Kludgey delay helps ensure move finishes
}

/* ------------------------------------------------------------------------
   fnRandGA : A brutal 'mutate' appropriate for first generations, etc
   LEFT TO CALLER to set the PRNG as desired! Note: Resets score.
   ------------------------------------------------------------------------
*/
void fnRandGA (int iArgIndex) {
  lScore[iArgIndex] = 0;  
  lSeed[iArgIndex] = random (lRandMax);    
  lLength[iArgIndex] = random (lRandMax);
}

/* ------------------------------------------------------------------------
   fnSanityGA : Enforce (not just check) sanity on arg GA
   ------------------------------------------------------------------------
*/
void fnSanityGA (int iArgIndex) {
  if (lLength[iArgIndex] < 4) {
    lLength[iArgIndex] = 4;
  }  
}

/* ------------------------------------------------------------------------
   fnHardwareTest : Quick blind hardware test for startup etc
   ------------------------------------------------------------------------
*/
void fnHardwareTest () {
  for (int i=0; i < 5; i++) {
    pinMode(i, OUTPUT);
    for (int s=0; s < 200; s += 20) {
      digitalWrite (i, HIGH);
      delayMicroseconds (s);
      digitalWrite (i, LOW);
      delayMicroseconds (20000);
    }  
    digitalWrite (i, HIGH);
    delayMicroseconds (60);
    digitalWrite (i, LOW);
    delayMicroseconds (20000);    
  }  
}

/* ------------------------------------------------------------------------
   fnSmartWrite : Write data to arg pin INDEX (from pins array) 
   Use means expected by that pin. With minimal sanity checking.
   ------------------------------------------------------------------------
*/
void fnSmartWrite (int iArgPinIndex, int iArgData) {
  if (iArgPinIndex < 0 || iArgData < 0) {
    return;
  }
  
  // Normalize pin argument to be map to pins array
  int iPinIndex = iArgPinIndex % iPinsTotal;
  int iPin = iPins[iPinIndex];          // Faster    
  pinMode (iPin, OUTPUT);              // Must as GA may have changed

  // Task the servo if it's one of those pins (KLUDGED for two servos)
  if (iPin == iServoPin || iPin == 2) {  
    fnServoTask (iPin, iArgData);
    return;
  }

  // Else, treat as routine digital or analog
  if (iPinIndex < 5) {                    // First 4 in array are digital
    if (iArgData < 128) {                 // Treat as low low, high high
      digitalWrite (iPin, LOW);
    } 
    if (iArgData >= 128) {
      digitalWrite (iPin, HIGH);
    }
  } else {
      analogWrite (iPin, iArgData);      // Even simpler
  }                                      // End if
}                                        // End function

/* ------------------------------------------------------------------------
   fnMutate : Mutate all GAs, with sanity checking.
   ------------------------------------------------------------------------
*/
void fnMutateAll () {
  for (int iCage=1; iCage < iZooTotal; iCage++) {
    // Re-seed to a real seed mandatory
    randomSeed (millis() + analogRead (A1)); 
    
    // Increment, decrement, or do nothing
    lLength[iCage] += random (3) - 1 ; 
    
    // Seed changes rare
    if (random(3) == 0) {
        lSeed[iCage] += random (3) - 1 ; 
    }
        
    fnSanityGA(iCage);
  }              
}

/* ------------------------------------------------------------------------
   fnExecute : Where sh*t actually happens!
   Down here because it's a mess and subject to tweaking. 
   Fields: 
     lCharm, lFave, lCurve, lIn, lAdd, lOut, lHold, lCount, 
     lLimit, lOutPin, lInPin
     
   TO DO: Convert to a reference-array random pick 
   ------------------------------------------------------------------------
*/
void fnExecute (int iArgCommand) {
  
  switch (iArgCommand) {
    // Old fashioned 'out value to out pin'
    case 0:  
      fnSmartWrite (lOutPin, lOut);    
      break;    
      
    // Process flow (harder now, fewer options)
    case 1:   // "Jump back" to top of instructions by re-initializing GA
      fnPrepFitness(iZooCurrent);
      break;      
      
    case 2:  // Loop to top only if...
      if (lCycleCount > lLimit) {
        fnPrepFitness(iZooCurrent);
      }
      break;      

    case 3:  // Loop to top only if...
      if (lCycleCount == lCharm) {
        fnPrepFitness(iZooCurrent);
      }
      break;      
      
    // Curve operations      
    case 8:  
      lAdd = sin (lCount) * lCurve;
      break;

    case 9: 
      lAdd = cos (lCount) * lCurve;
      break;
      
    case 10:
      lOut = sin (lCount) * lCurve;
      break;

    case 11:
      lOut = sin (lCount) * lCurve;
      break;
      
    // Conditional operations
    case 16:
      if (lLimit > lHold) {
        lLimit = 0;
      }
      break;
    
    case 17:
      if (lCount > lLimit) {
        lCount = 0;
      }
      break;

    case 18:
      if (lCount > lLimit) {
        fnSmartWrite (lOutPin, lOut);
        lCount = 0;
      }
      break;

    case 19:
      if (lCount > lLimit) {
        fnSmartWrite (lOutPin, lOut % 256);
        lCount = 0;
      }
      break;

    case 20:
      if (lCount > lLimit) {
        fnSmartWrite (lOutPin, lCurve * lOut % 256);
        lCount = 0;
      }
      break;

    case 21:
      if (lCount > lLimit) {
        fnSmartWrite (lOutPin, lCurve * lCount);
        lCount = 0;
      }
      break;

            
    // Traditional hardwired operations           
    case 25:
      lHold += lIn;
      break;
      
    case 26:
      lHold += lCount;
      break;
      
    case 27:
      lHold = 0;
      break;
      
    case 28:
      lHold = lRandLast;
      break;
      
    case 29:
      lLimit++;
      break;
      
    case 30:
      lLimit--;
      break;
           
    case 32:
      lOut += lHold;
      break;
      
    case 33:
      lAdd++;
      break;
      
    case 34:
      lAdd += lIn;
      break;
      
    case 35:
      lAdd--;
      break;    
      
    case 36:
      lLimit += lAdd;
      break;
      
    case 37:
      lLimit = lHold;
      break;
      
    // Pin pointing      
    case 38:
      lInPin++;
      break;
      
    case 39:
      lInPin--;
      break;
    
    case 40:
      lOutPin++;
      break;
      
    case 41:
      lOutPin--;
      break;
      
    case 42:
      lInPin = lFave;
      break;
      
    case 43:
      lOutPin = lFave;
      break;
      
    case 44:
      lInPin = lHold;
      break;
      
    case 45:
      lOutPin = lHold;
      break;

    default:
      // "Training wheels"
      fnSmartWrite (lOutPin, 10 + lOut % 180);
      break;      
  }
}
