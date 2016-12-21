/* ------------------------------------------------------------------------
 IQ Zero - Procedurally-Generated Genetic Algorithm Autonomous Micro Robot
 For ATTiny85 and the Arduino environment
 Winner of the Adafruit-Hackaday Pi Zero Contest!
 2016 @diemastermonkey (with special thanks to Charles 'Chucky D' Darwin)   
 ------------------------------------------------------------------------
 Memo
 
 v ng_001  Completely new, with focus on simplified instruction set
           Note: Currently, may treat digital pins as analog/vice versa...
           So far, not a problem but watch yer servos k. 
           
           Not yet implemented in this version:
             Timed fitness runs (it's cycles, now)
             Penalties/cost for writes

 Future plans: Introduce curves, re-visiting Karl Simms?
 
 ------------------------------------------------------------------------
 Serving Suggestion
 
  Ground ---  Gnd  |        | Pin 0 ---- Signal | Servo 1 
                   | Atmel  | Pin 1 ---- Signal | Passive IR Sensor (PIN 6, io 1)
                   |ATTiny85| Pin 2 ---- Signal | Servo 2 or ???
                   |        | Pin 3 ---- Pwr    | LED (d4)
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
int iGACount = 5;                  // Generation size, MUST match array -v
unsigned long lRunTimeMS =  2000;  // Lifetime of each GA in ms
long lRootSeed = 1088;             // Initial PRNG starting point
// Set bonus (or penalty) for physical interactions
int iServoCost = 1;                // Cost (from bonus) of writing to servo
int iAnalogCost = 1;               // Cost for digital ops
int iSensorReward = 500;           // Reward for *changing* value of sensor
int iContactReward = -1;           // Example: penalty for triggering contact
long lRandMax = 65536;             // Range of most rand outs

/* 
  Internal
  Important: Pins are listed in array, referred to by index by the GA,
  and internally. Yes, pin 2 is also iPins[2] but iPins[5] is not pin 5!
*/
// Pins available to the GA
// For ATMega 2560 // int iPins[] = {0, 1, 2, 3, 4, 5, 6, 7};
// For ATTiny85
int iPins[] = {0, 1, 2, 3, 4, A1, A2, A3};
int iPinsTotal = 8;               // Provide for cpu, you're cheaper
int iSensorPin = 2;               // For Arduino Uno // int iSensorPin = 2;
int iLedPin = 4;                  // For diags only
int iContactPin = 4;              // Trigger switch or contact sensor pin
float fSensorLast = 0;            // Most recent reading from sensor
int iTestIndex = 0;               // Index of GA currently in test
int iHighIndex = 0;               // Index of current winner
long lHighScore = -1;             // High during each test, not forever
long PRN = 0;
const int iGAFields = 12;         // MUST be gte num actual fields in GA
int iSample = 0;                  // Latest or recent output from prng
unsigned long lLastTimeMS = 0;    // When (sys run time) current test ends
unsigned long lNowTimeMS = 0;

// Temporary static cycle count test time
int iCycles = 0; int iTestLen = 990000;
 
/* ------------------------------------------------------------------------
   GA Structure
   Almost all fields are self-modifiable at runtime
   ------------------------------------------------------------------------
*/
// Fields stored in simple array, allowing GA to treat them like a very fat
// bit field - symbolic names are mapped via consts only so code can refer 
// to them as GA.Field[SEED] instead of GA.Field[0]. According to wizards, 
// const char is smarter on Atmel
const char SEED = 0;       // The GAs own seed for the prng
const char JUNK = 1;       // Maybe a backdoor?
const char RANGE = 2;      // Range of random values 
const char DIGITAL = 3;    // If this mod 2 eq 0, read/writ digital, else analog
const char INPIN = 4;      // Index to pin # reading from/writing to
const char OUTPIN = 5;
const char INPINMOD = 6;   // If prn mod this eq 0, inpin/outpin incremented
const char OUTPINMOD = 7;  // ...note that increments 'wrap around'
const char SKIPMOD = 8;    // If prn mod this, next prn discarded
const char READMOD = 9;    // If prn mod this eq 0, inpin read to buffer
const char WRITEMOD = 10;  // Same but write to outpin, get it?
const char MODEMOD = 11;   // If modzero'd, DIGITAL mode incremented
 
class ZeroGA {
   public:
     // All of its genes stored in this char array (range 0-256)
     char cFields [iGAFields];     
     // Fields that do no mutate
     long lScore; 
     long lBuffer;    // Q: Should be a FIELD? Then GA can use!

     void Init () {
       randomSeed (cFields[SEED]);  
       lScore = 0;
     }
     
     // Fetch AND EXECUTE next 'command' from prn (applying current range)
     // Importantly, a cmd can match *more than one* instruction!
     int Command () {
       char Cmd = int(random(cFields[RANGE]));
       
       // Noop discards this *and next* prn, then shunts
       if (Cmd % cFields[SKIPMOD] == 0) {
         Cmd = random(cFields[RANGE]); 
         return (int(Cmd));
       }

       // Pin Mods simply wrap, they are constrained to pin range in Write/Read  
       // Note processing may continue, this cycle!
       if (Cmd % cFields[INPINMOD] == 0) { cFields[INPIN] += 1; }         
       if (Cmd % cFields[OUTPINMOD] == 0) { cFields[OUTPIN] += 1; }         
       // Mode mod toggles digital/analog by adding one (as modzero 2 means digital)
       if (Cmd % cFields[MODEMOD] == 0) { cFields[DIGITAL] += 1; }                
       
       // Reads, writes directly to/from buffers
       if (Cmd % cFields[READMOD] == 0) { Read(); }         
       if (Cmd % cFields[WRITEMOD] == 0) { Write(); }       
     
       // Still here? Return sample to caller
       return (int(Cmd));
     }    
     
     // To do: SEED TO SOMETHING ELSE before you run this!
     void Mutate () {
       int iMutations = int(random(iGAFields));     // None to many mutations!
       for (int i=0; i < iMutations; i++) {
         // Smudge -1, 0 or +1 to random field. Q: iFields.size()? (A: No)
         cFields[int(random(iGAFields))] += int(random(3)) - 1; 
       }
       
       // Single completely random mutation, in global prn range
       cFields[int(random(iGAFields))] = int(random(lRandMax));
       
       // Mandatory safety checks: Divisors/mods must not be zero
       if (cFields[RANGE] < 1) { cFields[RANGE] = 1; }
       if (cFields[INPINMOD] < 1) { cFields[INPINMOD] = 1; }       
       if (cFields[OUTPINMOD] < 1) { cFields[OUTPINMOD] = 1; }              
       if (cFields[SKIPMOD] < 1) { cFields[SKIPMOD] = 1; }              
       if (cFields[READMOD] < 1) { cFields[READMOD] = 1; }              
       if (cFields[WRITEMOD] < 1) { cFields[WRITEMOD] = 1; }                     
       cFields[SEED] = abs(cFields[SEED]);         // PRNG no like LTZ
     }                                             // End Mutate
     
     // Clone (arg) other GA (literally dupe its fields!)
     // No, you can't cFields=argGA.cFields, assignins a pointer ;)
     void Clone (ZeroGA argGA) {       
       for (int i=0; i < iGAFields; i++) {
         cFields[i] = argGA.cFields[i];
       }
       lBuffer = argGA.lBuffer;       // Inherits contents of stomach? Hmm
     }
     
     // Mode-aware reads/writes (remember, it's an *array* of pins!)
     void Read () {
       int iPin = abs(cFields[INPIN] % iPinsTotal); // Constrain to pin range!
       pinMode (iPins[iPin], INPUT);               // Set inpin for read
       if (cFields[DIGITAL] % 2 == 0) {            // Handle digitally...
         lBuffer = digitalRead (iPins[iPin]);
       } else {                                    // Else as analog
         lBuffer = analogRead (iPins[iPin]);    
       }
     }                                             // End Read

     // For digital writes, upper half of values i.e. > 128 treated as HIGH
     void Write () {
       int iPin = abs(cFields[OUTPIN] % iPinsTotal); // Constrain to pin range
       if (iPin == iLedPin) {                      // No writing to diag LED allowed
         return;
       }
       pinMode (iPins[iPin], OUTPUT);              // Set inpin for read
       if (cFields[DIGITAL] % 2 == 0) {            // Handle digitally...         
         if (lBuffer < 128) {
           digitalWrite (iPins[iPin], LOW);           
         } else {
           digitalWrite (iPins[iPin], HIGH);
         }           
       } else {                                    // Else as analog
         analogWrite (iPins[iPin], lBuffer);
       }
     }                                             // End Write
     
};

ZeroGA GA[5];           // Array of GAs (MUST match iGACount)

/* ------------------------------------------------------------------------
   Run-Once Setup
   ------------------------------------------------------------------------
*/
void setup () {
  fnHardwareTest();
  
  // First generation
  pinMode (A1, INPUT);
  randomSeed (lRootSeed + analogRead (A1));   
  
  // Init test subject zero
  GA[0].Init(); GA[1].Init(); GA[2].Init();  // quick dev kludge
  GA[0].Mutate(); GA[1].Mutate(); GA[2].Mutate();
  // Test begins...  
}

/* ------------------------------------------------------------------------
   Main
   ------------------------------------------------------------------------
*/
void loop () {  
  // Current GA, take your next PRN and run it
  iSample = GA[iTestIndex].Command();
  // Dev monitoring: Show sample on LED
  pinMode (iLedPin, OUTPUT);
  analogWrite (iLedPin, iSample);

  // Always keep score during test
  fnScoring(iTestIndex);   
  
  // TO DO: If time now minus start of last test, THEN...
  iCycles++;
  if (iCycles > iTestLen) { 
    
    // Your life is over, take high score if so
    if (GA[iTestIndex].lScore > lHighScore) {
      iHighIndex = iTestIndex;
      lHighScore = GA[iTestIndex].lScore;
    }
    
    // Move on to next subject...
    iTestIndex++;
    // ...unless that was the last
    if (iTestIndex > iGACount) {
      // Generation complete, clone winner, mutate others
      for (int i=0; i < iGACount; i++) {
        if (i != iHighIndex) {
          GA[i].Clone (GA[iHighIndex]);
          GA[i].Mutate();              // Optional, mutate all
        }                              // End if don't mutate winner
      }                                // End for i clone/mutate all
      
      // Reset test for new generation
      lHighScore = 0;
      iHighIndex = 0;      
      iTestIndex = 0;
      fnFlash (iHighIndex, 10);    // Simple diags flash winner
    }                              // End if end generation
    
    iCycles = 0;                   // Reset for next test
    GA[iTestIndex].Init();         // Prepare next victim
  }                                // End if end test/lifetime
  
}                                  // End main loop

/* ------------------------------------------------------------------------
   fnHardwareTest : Quick blind hardware test for startup diags
   ------------------------------------------------------------------------
*/
void fnHardwareTest () {
  for (int i=0; i < iPinsTotal; i++) {
    pinMode(i, OUTPUT);
    for (int s=0; s < 16334; s += 128) {
      digitalWrite (i, HIGH);
      delayMicroseconds (s);
      digitalWrite (i, LOW);
      delayMicroseconds (2000);
      
      analogWrite (i, int(random(lRandMax)));
      delayMicroseconds (s);
      analogWrite (i, 0);      
    }  
    digitalWrite (i, HIGH);
    delayMicroseconds (90);
    digitalWrite (i, LOW);
    delayMicroseconds (500);    
  }  
}

 
/*
  Scoring: This part of the 'supervisor' must know which pin the sensor is on
*/
void fnScoring (int iArgIndex) {
  pinMode (iPins[iSensorPin], INPUT);    // PIR reward or penalty
  if (digitalRead (iPins[iSensorPin]) == HIGH) {
    GA[iArgIndex].lScore += iSensorReward;        
    pinMode (iLedPin, OUTPUT);    // LED on for info
    digitalWrite (iLedPin, HIGH);        
  } else {
    pinMode (iLedPin, OUTPUT);    // LED off for info
    digitalWrite (iLedPin, LOW);                
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
  
  pinMode (iLedPin, OUTPUT);  
  digitalWrite (iLedPin, LOW);      // Start off
  // delayMicroseconds (iDelay);  
  for (int i=0; i < iTimes; i++) {
    digitalWrite (iLedPin, HIGH);
    delay (iDelaySeconds);
    digitalWrite (iLedPin, LOW);
    delay (iDelaySeconds);
  }
}
