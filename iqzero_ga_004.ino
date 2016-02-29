// IQ Zero (fka Wurm): Procedurally-generate Genetic Algorithm Autonomous Micro Robot
// For ATTiny85 and the Arduino environment
// 2016 @diemastermonkey (with special thanks to Charles Darwin)
// 
// ---------------------------------------------------------------------------------
// Memo
// ---------------------------------------------------------------------------------
// REALLY ugly lab prototype - sorry for the mess folks, cleaner revs in the works!
// Yes I know it needs Major Refactoring. :\ -DMM
// ---------------------------------------------------------------------------------
//

// #include <EEPROM.h>    // For storing top GA later?
// V = EEPROM.read(Addr); EEPROM.write(Addr, V);

// GA Structures - static properties
int iGASeed = 31337;   // Base seed used for GA's program
int iGALength = 3;     // How many instructions in GA's program
int iGARange = 32;    // How many sides on GA's dice
int iGAScore = 0;      
// Runtime properties (exist only while under test)
int iGACmd = -1;        // Actual "program command" value
int iGAPin = 2;         // General-purpose 'pointing at a pin' register
int iGAIn = -1;         // Single-value input/output registers
int iGAOut = -1;        
int iGALast= -1;        // Last value generated by randgen
int iGAStep = 0;        // Current step in test GA's 'program'

// For the zoo
int iGA[16][4];          // A zoo of GAs, and their properties
int SEED=31337, LENGTH=1, RANGE=32, SCORE=3;  // For easier array syntax

// For entire sim - err, not sim, cause this is real :)
int iZooIndex = 0;         // Which GA from zoo currently being tested
int iZooTotal = 8;         // How many total in zoo i.e. population size
int iTestCyclesMax = 64;   // How many 'program instruction runs' per sim run
int iTestCycles = 0;       // Current test cycle "charge" style reverse counter
int iCommandTypes = 32;    // Total # of available commands

// Pins available to the GA
int iPins[] = {0, 1, 2, 3, 4, A1, A2, A3};
int iPinsTotal = 8;          // Fast for frequent use
// Minimal structural 'hints'
int iServoPin = 0;                // For normalization only, GA must discover what's what
int iChargedPin = -1;              // When servo charge done, set this pin low
int iLedPin = 3;                  // Diags
int iContactPin = 4;              // Trigger switch or contact sensor pin
int iSensorPin = 1;               // Currently a PIR (DIGITAL) sends high while motion
// "Environment" state stuff
int iServoCharge = -1;            // Set high then charge, set low on 0 then -1 to disable
int iSensorLast = -1;             // Last value from sensor for up/down compares
int iServoBonus = 0;              // Set to "TestCycleMax" each cycle, decrement for each servo, add at end
int iRecurseArg = -1;             // Used only in recurse fnExecutes

// Run-once setup
// ---------------------------------------------------------------------------------
void setup () {
  pinMode (A1, INPUT);           // Because hope to use in a sec
  pinMode (A2, INPUT); 
  pinMode (A3, INPUT);
  
  // Crudely init Zoo's GA's seeds, repeat while down helps randomize
  for (int i=0; i <= iZooTotal; i++) {
    // Inefective:
    randomSeed (analogRead(A1) + 8901 - micros());       
    iGA[i][SEED] = random(65536);
    iGA[i][RANGE] = 32;          
  }
  
  /* Debug: Blind startup hardware demo */
  for (int i=0; i < 5; i++) {
    pinMode(i, OUTPUT);
    for (int s=0; s < 200; s += 20) {
      digitalWrite (i, HIGH);
      delayMicroseconds (s);
      digitalWrite (i, LOW);
      delayMicroseconds (20000);
    }  
  }

  fnGAPrep (0);   // Prime 0th GA for life
  iTestCycles = iTestCyclesMax;  // Charge for test run
  delay(300);    // time to get out of its way
}

/* Main
   ---------------------------------------------------------------------------
   Run each GA's program for fixed x cycles, then fitness tests
   Select winner, mutate, repeat. Thank you Darwin!
*/
void loop () {  
  // Discharge test cycles, or handle next testee, or bake off
  iTestCycles--;
  if (iTestCycles < 0) {
    // This GA's run complete, update it's score, including bonus
    iGA[iZooIndex][SCORE] = iGAScore;
    // Go to next in zoo
    iZooIndex++;
    // fnFlash (iZooIndex, 50);

    // If all GAs tested: Select and dupe best, mutate others
    if (iZooIndex >= iZooTotal) {
      int iHighIndex = fnSelectWinner();
      // Dupe winner to all slots - To do: store winner in eeprom   
      for (int index=0; index <= iZooTotal; index++) {   // Iterate all cages in zoo      
        for (int field=0; field < 4; field++) {
          iGA[index][field] = iGA[iHighIndex][field];
        }
      }

      // Mutate all children except the winner (0th)
      fnMutate();
      // Reset for next cycle of tests
      iZooIndex = 0;      
      iTestCycles = iTestCyclesMax;      
      // Status: Blink-out og index of winner
      delay (100);
      fnFlash (iHighIndex + 1, 500);
      delay (100);                
    }                         // End 'if gt zoo total' all zoo iterated

    // Prep the new zoo victim for test (loads globals)
    fnGAPrep (iZooIndex);
  }                          // End 'if cycles < 1' (cycles exhausted)

  // Handle current testee's "program step" (with wraparound)
  iGAStep++;
  if (iGAStep > iGALength) {    // Wraparound GA's program...
    iGAStep = 0;
    randomSeed (iGASeed);       // ...by reseeding
  }

  // Get and execute this GA's next instruction from the PRNG
  iGACmd = fnProcGenAdvance (1, iGARange);
  fnExecute (iGACmd);
  
  // Accumulate score
  iGAScore += fnScoreLive();
  
  // fnServoProcess ();  // Dispatch any servo work (retired?) 
}                                  // End (main) loop

/*  fnGAPrep
   ---------------------------------------------------------------------------
    Prep (arg index) GA from the zoo for a test run (and seed randgenerator)
*/

void fnGAPrep (int argIndex) {    
  iGASeed = iGA[argIndex][SEED];  // Superfluous
  iGALength = iGA[argIndex][LENGTH];
  iGARange = 32;                 // RETIRED (remove soon)
  iGAScore = 0;                  // Note score is reset each run
  // iServoBonus = iTestCyclesMax;  // RETIRED Recharge servo bonus
  iGAStep = 0;                   // Ditto
  // Init prng with this GA's seed       
  randomSeed (iGASeed);           
  
  // Important: Start the GA with the same register values, taken
  // in the same order from the *top* of the prng, *first*
  iGAIn = random(iGARange);          // Yes these are unpredicted values
  iGAOut = random(iGARange);         // making them work, that's the GA's problem!
  iGAPin = random(iGARange);  
  
}

/* fnProcGenAdvance: Return the xth random value from the generator.
   ---------------------------------------------------------------------------
   With size 1 it's just a normal die. Any more is like
   'fast forwarding' the procgen, skipping steps in the
   program. Also updates 'last' buffer.
*/
int fnProcGenAdvance (int iArgStepSize, int iArgDieSides) {
  for (int i=0; i < iArgStepSize; i++) {
    iGALast = random (iArgDieSides); 
  }
  return (iGALast); 
}

/* fnScoreLive: In-cycle tests for accumulating score during test
   ---------------------------------------------------------------------------
   returns a value to be *added* to the GA's score, thus supporting
   penalties later. Use 0 for no bonus.
*/
int fnScoreLive (void) {
  int iScore = 0;
  int iTemp = 0;

  // if switch  pressed (*right now*), big points
  pinMode (iContactPin, INPUT);          // Always needed as GA may change?
  if (digitalRead (iContactPin) == HIGH) {
    iScore += 50;
  }

  // Sensor test, atm an LDR - more light means lower resistance
  pinMode (iSensorPin, INPUT);  
  // Example for an LDR i.e. "analog value changed"
  // iTemp = int (analogRead (iSensorPin) / 10);  
  
  // For digital PIR that sends continuous HIGH while motion detect
  iTemp = digitalRead(iSensorPin);  
  if (iTemp == HIGH) { 
    iScore += 1;                       // Doubled to ensure it weighs more than servo bonus
    fnFlash (1, 1);
  }
  iSensorLast = iTemp;                // May be used by GA

  //
  // Insert more tests here
  //
  return (iScore);
}

/* fnSelectWinner: Returns index of the GA with highest current score
   ---------------------------------------------------------------------------
*/
int fnSelectWinner () {
  // Select highest score
  int iHighest, iWinner;
  for (int i=0; i <= iZooTotal; i++) {
    if (iGA[i][SCORE] > iHighest) {
      iHighest = iGA[i][SCORE];            // Tha new winna!
      iWinner = i;
    }
  }  // End for   
  return (iWinner);
}    // End function

/* fnMutate : Mutate GAs in all cages in zoo except 0th (winner)
   ---------------------------------------------------------------------------
*/
void fnMutate () {
  for (int cage=1; cage <= iZooTotal; cage++) {
    // Re-seed to a real seed mandatory
    randomSeed (millis() + analogRead (A0)); 
    
    // Pick a field to randomize
    int field = random(4);                // Beware hardwired range
    if (field != 0) {                    
      int iTemp = iGA[cage][field] + random(3) - 1;
      iGA[cage][field] = iTemp;         // Add if safe to do so
    } else {
      if (random(2) == 0) {               
        iGA[cage][SEED] = random(65536);  // Likely disastrous
      }
    }                                      // End else
  }                                        // End for (copy mutants)  
}

/* fnSmartWrite : Write arg data to (normalized) arg pin (of pins array) 
   ---------------------------------------------------------------------------
   ...by the means expected by that pin. Squahes any arg pin index into 
   a value appropriate for the array of known pins. 
*/
void fnSmartWrite (int iArgPinIndex, int iArgData) {
  // Normalize the pin argument to be from 0 to iPinsMax
  int iPinIndex = iArgPinIndex % iPinsTotal;
  int iPin = iPins[iPinIndex];          // Faster    
  pinMode (iPin, OUTPUT);              // Must as GA may have changed

  // Task the servo if it's one of those pins
  //  if (iPin == iServoPin) {
  if (iPin == iServoPin || iPin == 2) {         // KLUDGETEST for two servos
    fnServoTask (iPin, iArgData);   // Baby steps: Constrain to sorta servo range
    return;
  }

  // Else, treat as routine digital or analog
  if (iPinIndex < 5) {                       // First 4 in array are digital
    if (iArgData % 2 == 0) {                 // Treat even/odd as high/low
      digitalWrite (iPin, HIGH);
    } 
    else {
      digitalWrite (iPin, LOW);
    }
  } 
  else {                                 // Upper 3 in array are analog
    analogWrite (iPin, iArgData);        // Simple!
  }                                      // End if
}                                        // End function

/*  fnSmartRead : Just like SmartWrite but reads, returns result
   ---------------------------------------------------------------------------
*/
int fnSmartRead (int iArgPinIndex) {
  // Normalize the pin argument to fit within total pins
  int iPinIndex = abs (iArgPinIndex % iPinsTotal);
  int iPin = iPins[iPinIndex];        // Faster
  pinMode (iPin, OUTPUT);  // Must as GA may change
  if (iPinIndex < 5) {                
    return (digitalRead(iPin));
  } 
  else {                               
    return (analogRead (iPin));
  }  
}

/*  fnServoTask : Send servo to a destination i (60=0' 280=180') (REINSTATE!)
   ---------------------------------------------------------------------------
*/
void fnServoTask (int iArgPin, int i) {
  iServoCharge = i;                     // Charge counter and send high to servo
  iServoBonus--;                        // Charge GA's bonus for the servo activity
  iChargedPin = iArgPin;                // Store for fnServoProcess to use later
  digitalWrite (iArgPin, HIGH);  
  delayMicroseconds (i);                // KLUDGE FIX? Might not be doing anything now
  digitalWrite (iArgPin, LOW);      // Removed to reinstate fnServoProcess?
}

/*  fnServoProcess : Process servo if necessary (nonblocking - disused, reinstsate!)
   ---------------------------------------------------------------------------
*/
void fnServoProcess () {
  if (iServoCharge == -1) { 
    return; 
  }      // Nothing to do
  iServoCharge--;                          // Decrement charge
  if (iServoCharge == 0) {
    if (iChargedPin != -1) {               // Superfluous?
      digitalWrite (iChargedPin, LOW);     // Discharged, send low to servo
    }
    iServoCharge = -1;                     // Done, disable countdown
  }
}

/*  fnFlash : Simple flasher w/fractional seconds
   ---------------------------------------------------------------------------
*/
void fnFlash (int iTimes, float iDelaySeconds) {
  pinMode (iLedPin, OUTPUT);  
  int iDelay = 10000 * iDelaySeconds;
  for (int i=0; i < iTimes; i++) {
    digitalWrite (iLedPin, HIGH);
    delayMicroseconds (iDelay);
    digitalWrite (iLedPin, LOW);
    delayMicroseconds (iDelay);
  }
}

/* fnExecute : Execute a single (INT) instruction from the GA's DNA or elsewhere
   ---------------------------------------------------------------------------    
   Really has to operate on globs cause it might change all kinds of stuff
   It's way down here because it gets tweaked ALOT and is a mess
   Important: Flattens int argCmd to iCommandTypes available commands
*/
void fnExecute (int iArgCmd) {
  iArgCmd = iArgCmd % iCommandTypes;          // Note above
  
  switch (iArgCmd) {
    
    /* ----------------------------------------------------
        Program Flow Stuff    
       ----------------------------------------------------                
    */
    case 0: break;              // Case zero do nothing

    // Loop self by jumping to actual bottom
    case 1:   
      iGAStep = iGALength;      // Simply await wraparound
      break;
  
    // Jump to bottom (loop w/last cmd execute)
    case 2:
      iGAStep = iGALength - 1; // Last command actually runs if time
      break;            

    // Discard next instruction (Note: Still updates 'gaLast')
    case 4: 
      fnProcGenAdvance (1, iGARange);
      break;
  
    // Skip "iGAOut" instructions
    case 5:
      fnProcGenAdvance (iGAOut, iCommandTypes);
      break;
      
    // Read next instruction into GAIn, but don't execute it
    case 6:
     iGAIn = fnProcGenAdvance (1, iCommandTypes);
     break;
     
    // 'skip next cmd if In gt Out'
    case 7:
      if (iGAIn > iGAOut) { 
        fnProcGenAdvance (1, iGARange);  // discard next cmd
      }
      break;
  
    // 'skip next cmd if In lt Out' (opposite)
    case 8:
      if (iGAIn < iGAOut) { 
        fnProcGenAdvance (1, iGARange);  // discard next cmd
      }  
      break;

    case 9:   // Return to top if no score (should be handy)
      if (iGAScore < 2) {
        iGAStep = iGALength;    // Simply await wraparound      
      }
      break;           
     
    // Execute contents of GAIn as an instruction
    // Recurses fnExecute, so we MUST clear GAIn first
    case 10:
      iRecurseArg = iGAIn;
      iGAIn = 0;                // Thus is also a 'clear ga in'
      fnExecute (iRecurseArg);
      break;

    /* ----------------------------------------------------
        Outputs
       ----------------------------------------------------                
    */
    
    // No longer allowed to blink Status LED)    
    case 11:   // Simple 'out' to 'pin'
      if (iGAPin != iLedPin) {          // Disallow blinking LED
        fnSmartWrite (iGAPin, iGAOut);
      }
      break;
      
    // "Training wheels" servo outputs
    case 12: 
      fnServoTask (iGAPin, iGAOut);
      break;      
      
    case 13:
      fnServoTask (iServoPin, iGAOut); 
      break;
      
    /* ----------------------------------------------------
        Inputs
       ----------------------------------------------------                
    */
    case 14:   // Simple in from pin
      iGAIn = fnSmartRead (iGAPin);
      break;
      
    case 15:   // Accumulator version of in from pin
      iGAIn += fnSmartRead (iGAPin);
      break;
    
    case 16: 
      iGAPin = iGAIn; 
      break;
      
    // Increment in
    case 17:
      iGAIn++;
      break;
      
    // Out to value of in
    case 18: 
      iGAOut = iGAIn; 
      break;      
    
    /* ----------------------------------------------------
        Internal Operations
       ----------------------------------------------------                
    */
    case 19:       // Assignments of iGALast/iGAStep/ServoBonus etc
      iGAIn = iGALast; 
      break;
      
    case 20: 
      // Retired // iGAIn = iServoBonus;
      break;
      
    case 21: 
      iGAPin = iGALast; 
      break;
      // Uses of its own step counter    
    case 22: 
      iGAIn = iGAStep; 
      break;
    case 23: 
      iGAIn = iGAIn % iGAStep; 
      break;
    case 24: 
      iGAOut = iGAStep; 
      break;      
    case 25:           // Increment out by current step
      iGAOut += iGAStep; 
      break;     
      
    case 26:   // Add own score to output
      iGAOut += iGAScore;
      break;

    /* ----------------------------------------------------
        Pin Controls
       ----------------------------------------------------        
    */
    case 27: 
      iGAPin++; 
      break;
    case 28: 
      iGAPin--; 
      break;    
    case 29: 
      iGAPin = iGALast; 
      break;
    case 30: 
      iGAOut++;        // Simple counter in GAOut
      break;
      
  }                            // End big ugly switch  
}                              // End function

