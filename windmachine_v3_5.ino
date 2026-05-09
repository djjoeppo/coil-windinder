////////////////////////////////////////////////////////////////////////////////
// 🧠 CNC 4-AXIS FIRMWARE (VERSION 3.6 - OPTIMIZED)
////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_AXES 4

////////////////////////////////////////////////////////////////////////////////
// 🛠️ BLOK 1: PIN DEFINITIES & REGISTER MASKS
////////////////////////////////////////////////////////////////////////////////
// X (Index 0): Step 4 (PD4), Dir 5 (PD5), Limit A3 (PC3)
// Y (Index 1): Step 6 (PD6), Dir 7 (PD7), Limit A4 (PC4)
// Z (Index 2): Step 8 (PB0), Dir 9 (PB1), Limit A5 (PC5)
// A (Index 3): Step 2 (PD2), Dir 3 (PD3), Limit A2 (PC2)

// Port D masks (Step/Dir for X, Y, A)
#define MASK_STEP_X (1 << 4)
#define MASK_DIR_X  (1 << 5)
#define MASK_STEP_Y (1 << 6)
#define MASK_DIR_Y  (1 << 7)
#define MASK_STEP_A (1 << 2)
#define MASK_DIR_A  (1 << 3)

// Port B masks (Step/Dir for Z)
#define MASK_STEP_Z (1 << 0)
#define MASK_DIR_Z  (1 << 1)

// Port C masks (Limits for X, Y, Z, A)
#define MASK_LIMIT_X (1 << 3)
#define MASK_LIMIT_Y (1 << 4)
#define MASK_LIMIT_Z (1 << 5)
#define MASK_LIMIT_A (1 << 2)

const uint8_t limitMasks[NUM_AXES] = {MASK_LIMIT_X, MASK_LIMIT_Y, MASK_LIMIT_Z, MASK_LIMIT_A};

////////////////////////////////////////////////////////////////////////////////
// ⚙️ BLOK 2: AS-CONFIGURATIE
////////////////////////////////////////////////////////////////////////////////

struct AxisConfig {
  int stepPin;
  int dirPin;
  int limitPin;
  bool invertDir;
  bool homeDirNegative;
  bool isNC;
  float maxTravelMM;
  float stepsPerMM;
};

AxisConfig axes[NUM_AXES];

void setupConfig() {
  // --- X-AS ---
  axes[0].stepPin         = 4;
  axes[0].dirPin          = 5;
  axes[0].limitPin        = A3;
  axes[0].invertDir       = false;
  axes[0].homeDirNegative = true;
  axes[0].isNC            = false;
  axes[0].maxTravelMM     = 275.0;
  axes[0].stepsPerMM      = 266.7;

  // --- Y-AS ---
  axes[1].stepPin         = 6;
  axes[1].dirPin          = 7;
  axes[1].limitPin        = A4;
  axes[1].invertDir       = true;
  axes[1].homeDirNegative = true;
  axes[1].isNC            = false;
  axes[1].maxTravelMM     = 65.0;
  axes[1].stepsPerMM      = 266.7;

  // --- Z-AS ---
  axes[2].stepPin         = 8;
  axes[2].dirPin          = 9;
  axes[2].limitPin        = A5;
  axes[2].invertDir       = false;
  axes[2].homeDirNegative = true;
  axes[2].isNC            = true;
  axes[2].maxTravelMM     = 15.0;
  axes[2].stepsPerMM      = 400.0;

  // --- A-AS ---
  axes[3].stepPin         = 2;
  axes[3].dirPin          = 3;
  axes[3].limitPin        = A2;
  axes[3].invertDir       = true;
  axes[3].homeDirNegative = false;
  axes[3].isNC            = true;
  axes[3].maxTravelMM     = -1.0; // Infinite travel
  axes[3].stepsPerMM      = 3.5556; // Steps per degree
}

////////////////////////////////////////////////////////////////////////////////
// 🚀 BLOK 3: ALGEMENE INSTELLINGEN & BUFFER
////////////////////////////////////////////////////////////////////////////////
float maxSpeed    = 30000;
float accel       = 1000;
float targetSpeed = 3000;

float homeFastSpeed     = 3000;
float homeSlowSpeed     = 120;
long homeBackoffSteps   = 1000;

struct Move {
  long steps[NUM_AXES];
  float feed;
};

const int BUFFER_SIZE = 16;
Move moveBuffer[BUFFER_SIZE];
volatile int head = 0;
volatile int tail = 0;

////////////////////////////////////////////////////////////////////////////////
// 📊 STATUS VARIABELEN
////////////////////////////////////////////////////////////////////////////////
enum MotionState { IDLE, MOVING };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF_MOVE, HOME_BACKOFF_WAIT, HOME_SLOW_SEEK, HOME_SETTLE };

volatile MotionState state = IDLE;
volatile HomeState homeState = HOME_IDLE;
volatile int homingAxis = -1; 

volatile long currentPosition[NUM_AXES] = {0, 0, 0, 0};
volatile long stepCount[NUM_AXES] = {0, 0, 0, 0};
volatile long err[NUM_AXES] = {0, 0, 0, 0};
volatile long moveDir[NUM_AXES] = {0, 0, 0, 0}; 
volatile bool doStep[NUM_AXES] = {false, false, false, false};

volatile long dominantStepsTotal = 0;
volatile long dominantStepsRemaining = 0;

volatile bool homed[NUM_AXES] = {false, false, false, false};
volatile float currentSpeed = 0;
bool absoluteMode = true;

unsigned long lastRampMicros = 0;
unsigned long homingStart = 0;
unsigned long homingTimer = 0;

char serialBuffer[128];
int serialIdx = 0;

////////////////////////////////////////////////////////////////////////////////
// ⚡ LIMIT SWITCH CHECK (With direct port manipulation and debounce)
////////////////////////////////////////////////////////////////////////////////
inline bool limitTriggered(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) return false;

  uint8_t mask = limitMasks[axisIndex];
  int samples = 0;
  for(int i = 0; i < 5; i++) {
    if (!(PINC & mask)) samples++;
  }
  bool raw = (samples > 2);
  return axes[axisIndex].isNC ? raw : !raw;
}

void startMove(long stepsX, long stepsY, long stepsZ, long stepsA, float feed) {
  long steps[NUM_AXES] = {stepsX, stepsY, stepsZ, stepsA};

  dominantStepsTotal = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = labs(steps[i]);
    if (stepCount[i] > dominantStepsTotal) {
      dominantStepsTotal = stepCount[i];
    }

    if (steps[i] != 0) {
      bool dir = (steps[i] > 0);
      if (axes[i].invertDir) dir = !dir;

      // Direct Port Manipulation for Dir Pins
      if (i == 0) { // X - PD5
        if (dir) PORTD |= MASK_DIR_X; else PORTD &= ~MASK_DIR_X;
      } else if (i == 1) { // Y - PD7
        if (dir) PORTD |= MASK_DIR_Y; else PORTD &= ~MASK_DIR_Y;
      } else if (i == 2) { // Z - PB1
        if (dir) PORTB |= MASK_DIR_Z; else PORTB &= ~MASK_DIR_Z;
      } else if (i == 3) { // A - PD3
        if (dir) PORTD |= MASK_DIR_A; else PORTD &= ~MASK_DIR_A;
      }
      moveDir[i] = (steps[i] > 0) ? 1 : -1;
    } else {
      moveDir[i] = 0;
    }
    err[i] = 0;
    doStep[i] = false;
  }

  if (dominantStepsTotal == 0) {
    state = IDLE;
    return;
  }

  for (int i = 0; i < NUM_AXES; i++) {
    err[i] = dominantStepsTotal / 2;
    if (stepCount[i] > 0) {
      err[i] -= stepCount[i];
      if (err[i] < 0) {
        doStep[i] = true;
        err[i] += dominantStepsTotal;
      }
    }
  }

  dominantStepsRemaining = dominantStepsTotal;
  targetSpeed = constrain(feed, 50, maxSpeed);
  currentSpeed = 0;
  lastRampMicros = micros();
  state = MOVING;
}

////////////////////////////////////////////////////////////////////////////////
// 🚀 HOMING SEQUENCE
////////////////////////////////////////////////////////////////////////////////
void startHomingAxis(int axisIndex);

void startHomingSequence() {
  head = 0;
  tail = 0;
  Serial.println(F("--- STARTING HOMING SEQUENCE (Z, Y, X, A) ---"));
  startHomingAxis(2); // Z first
}

void startHomingAxis(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) {
    homingAxis = -1; homeState = HOME_IDLE; state = IDLE;
    Serial.println(F(">>> ALL AXES HOMED SUCCESSFULLY."));
    return;
  }

  homingAxis = axisIndex;
  homingStart = millis();
  Serial.print(F("AXIS ")); Serial.print(axisIndex);

  long backoffAmt = axes[axisIndex].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;

  if (limitTriggered(axisIndex)) {
    Serial.println(F(": ALREADY ON LIMIT -> STARTING BACKOFF"));
    homeState = HOME_BACKOFF_MOVE;
    startMove(
      (axisIndex == 0 ? backoffAmt : 0),
      (axisIndex == 1 ? backoffAmt : 0),
      (axisIndex == 2 ? backoffAmt : 0),
      (axisIndex == 3 ? backoffAmt : 0),
      homeSlowSpeed
    );
  } else {
    Serial.println(F(": FAST SEEK"));
    homeState = HOME_FAST_SEEK;
    long bigMove = axes[axisIndex].homeDirNegative ? -1000000 : 1000000;
    startMove(
      (axisIndex == 0 ? bigMove : 0),
      (axisIndex == 1 ? bigMove : 0),
      (axisIndex == 2 ? bigMove : 0),
      (axisIndex == 3 ? bigMove : 0),
      homeFastSpeed
    );
  }
}

void handleHoming() {
  if (homeState == HOME_IDLE || homingAxis == -1) return;

  if (millis() - homingStart > 20000) {
    Serial.println(F("ALARM: Homing timeout!"));
    state = IDLE; homeState = HOME_IDLE; homingAxis = -1;
    return;
  }

  bool hit = limitTriggered(homingAxis);

  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) {
        state = IDLE;
        homingTimer = millis();
        homeState = HOME_BACKOFF_MOVE;
        Serial.println(F(" -> Hit, backing off..."));
      }
      break;

    case HOME_BACKOFF_MOVE:
      if (state == IDLE) {
          if (millis() - homingTimer > 150) {
              if (limitTriggered(homingAxis)) {
                  long backoffStep = axes[homingAxis].homeDirNegative ? 300 : -300;
                  startMove(
                    (homingAxis == 0 ? backoffStep : 0),
                    (homingAxis == 1 ? backoffStep : 0),
                    (homingAxis == 2 ? backoffStep : 0),
                    (homingAxis == 3 ? backoffStep : 0),
                    homeSlowSpeed
                  );
                  homingTimer = millis();
              } else {
                  homeState = HOME_BACKOFF_WAIT;
                  homingTimer = millis();
              }
          }
      }
      break;

    case HOME_BACKOFF_WAIT:
      if (millis() - homingTimer > 150) {
          homeState = HOME_SLOW_SEEK;
          long slowSeek = axes[homingAxis].homeDirNegative ? -100000 : 100000;
          startMove(
            (homingAxis == 0 ? slowSeek : 0),
            (homingAxis == 1 ? slowSeek : 0),
            (homingAxis == 2 ? slowSeek : 0),
            (homingAxis == 3 ? slowSeek : 0),
            homeSlowSpeed
          );
          Serial.println(F(" -> Sensor vrij, seeking slow..."));
      }
      break;

    case HOME_SLOW_SEEK:
      if (hit) {
        state = IDLE;
        currentPosition[homingAxis] = 0; 
        homed[homingAxis] = true;
        Serial.print(F("AXIS ")); Serial.print(homingAxis); Serial.println(F(" HOMED OK."));
        homingTimer = millis();
        homeState = HOME_SETTLE;
      }
      break;

    case HOME_SETTLE:
      if (millis() - homingTimer > 200) {
          int next = -1;
          if (homingAxis == 2) next = 1;      // After Z -> Y
          else if (homingAxis == 1) next = 0; // After Y -> X
          else if (homingAxis == 0) next = 3; // After X -> A
          startHomingAxis(next);
      }
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// 🔄 RAMPING & SPEED CONTROL
////////////////////////////////////////////////////////////////////////////////
void updateRamp() {
  if (state != MOVING) {
    currentSpeed = 0;
    return;
  }

  unsigned long now = micros();
  float dt = (now - lastRampMicros) / 1000000.0; 
  lastRampMicros = now;
  
  if (currentSpeed < targetSpeed) {
    currentSpeed += accel * dt; 
    if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= accel * dt;
    if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  float safeSpeed = max(currentSpeed, 10.0);
  OCR1A = (2000000.0 / safeSpeed);
}

////////////////////////////////////////////////////////////////////////////////
// 💬 G-CODE PARSER & COMMUNICATIE
////////////////////////////////////////////////////////////////////////////////
void sendStatusMessage(const char* title, const char* message) {
  Serial.println(F("\n--------------------------------------------------"));
  Serial.print(F(" >> ")); Serial.println(title);
  if (message && message[0] != '\0') {
    Serial.print(F("    ")); Serial.println(message);
  }
  Serial.println(F("--------------------------------------------------"));
}

float getAxisValue(const char* line, char axisLetter, float defaultVal, bool &found) {
  const char* ptr = strchr(line, axisLetter);
  if (ptr) {
    found = true;
    return strtof(ptr + 1, NULL);
  }
  found = false;
  return defaultVal;
}

void processCommand(char* line) {
  // Trim leading whitespace
  while(*line == ' ') line++;

  // Trim trailing whitespace
  int len = strlen(line);
  while(len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' ')) {
      line[--len] = '\0';
  }
  if (len == 0) return;

  // Convert to uppercase
  for(int i=0; i<len; i++) line[i] = toupper(line[i]);

  if (strcmp(line, "G28") == 0 || strcmp(line, "HOME") == 0) {
    startHomingSequence();
    return;
  }

  if (strcmp(line, "G90") == 0) {
    absoluteMode = true; 
    sendStatusMessage("MODUS: ABSOLUUT (G90)", "Posities berekend vanaf machine-nulpunt.");
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(line, "G91") == 0) {
    absoluteMode = false; 
    sendStatusMessage("MODUS: RELATIEF (G91)", "Posities berekend vanaf huidige locatie.");
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(line, "M114") == 0) {
    Serial.println(F("\n--- ACTUELE MACHINE POSITIE ---"));
    const char labels[] = {'X', 'Y', 'Z', 'A'};
    for (int i = 0; i < NUM_AXES; i++) {
      float pos = (float)currentPosition[i] / axes[i].stepsPerMM;
      Serial.print(labels[i]); Serial.print(F(": "));
      Serial.print(pos); 
      if (i == 3) Serial.print(F("°  "));
      else Serial.print(F("mm  "));
    }
    Serial.println(F("\n-------------------------------"));
    Serial.println(F("ok"));
    return;
  }
 
  if (line[0] == 'G') {
    for(int i = 0; i < NUM_AXES; i++) {
        if (!homed[i]) { 
          Serial.println(F("ERROR: Systeem niet veilig. Voer eerst G28 uit!"));
          return;
        }
    }

    bool fFound;
    float feed = getAxisValue(line, 'F', targetSpeed, fFound);

    long stepsToMove[NUM_AXES] = {0, 0, 0, 0};
    bool axisIncluded[NUM_AXES] = {false, false, false, false};
    const char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'};
    bool hasMovement = false;

    for (int i = 0; i < NUM_AXES; i++) {
      float val = getAxisValue(line, axisChars[i], 0.0, axisIncluded[i]);
      if (axisIncluded[i]) {
        float currentPosUnit = (float)currentPosition[i] / axes[i].stepsPerMM;
        
        for (int b = tail; b != head; b = (b + 1) % BUFFER_SIZE) {
          currentPosUnit += (moveBuffer[b].steps[i] / axes[i].stepsPerMM);
        }

        float deltaUnit = absoluteMode ? (val - currentPosUnit) : val;
        float newPosUnit = currentPosUnit + deltaUnit;

        // Soft limit check (exclude A-axis i=3)
        if (i != 3 && axes[i].maxTravelMM > 0) {
          if (newPosUnit < -0.01 || newPosUnit > axes[i].maxTravelMM + 0.01) {
            Serial.print(F("ALARM: Soft limit bereikt op ")); Serial.print(axisChars[i]);
            Serial.print(F(" (Max: ")); Serial.print(axes[i].maxTravelMM); Serial.println(F("mm)"));
            return;
          }
        }

        stepsToMove[i] = (long)round(deltaUnit * axes[i].stepsPerMM);
        if (stepsToMove[i] != 0) hasMovement = true;
      }
    }

    if (hasMovement) {
      int nextHead = (head + 1) % BUFFER_SIZE;
      if (nextHead != tail) {
        for (int i = 0; i < NUM_AXES; i++) {
          moveBuffer[head].steps[i] = stepsToMove[i];
        }
        moveBuffer[head].feed = feed;
        head = nextHead;
        Serial.println(F("ok"));
      } else {
        Serial.println(F("ERROR: Planner buffer vol! Wacht even..."));
      }
    } else {
      Serial.println(F("ok"));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ⏱️ TIMER ISR (Optimized with Direct Port Manipulation)
////////////////////////////////////////////////////////////////////////////////
ISR(TIMER1_COMPA_vect) {
  if (homeState != HOME_IDLE && homingAxis >= 0 && limitTriggered(homingAxis) && homeState != HOME_BACKOFF_MOVE && homeState != HOME_BACKOFF_WAIT) {
    // Stop movement on limit hit
    PORTD &= ~(MASK_STEP_X | MASK_STEP_Y | MASK_STEP_A);
    PORTB &= ~MASK_STEP_Z;
    state = IDLE; 
    dominantStepsRemaining = 0;
    return;
  }

  if (state != MOVING || dominantStepsRemaining <= 0) {
    state = IDLE; 
    return;
  }

  static bool phase = false;
  phase = !phase; 

  if (phase) {
    // Step HIGH
    uint8_t pd_steps = 0;
    if (doStep[0]) pd_steps |= MASK_STEP_X;
    if (doStep[1]) pd_steps |= MASK_STEP_Y;
    if (doStep[3]) pd_steps |= MASK_STEP_A;
    PORTD |= pd_steps;
    if (doStep[2]) PORTB |= MASK_STEP_Z;
  } else {
    // Step LOW and update positions
    PORTD &= ~(MASK_STEP_X | MASK_STEP_Y | MASK_STEP_A);
    PORTB &= ~MASK_STEP_Z;

    for (int i = 0; i < NUM_AXES; i++) {
      if (doStep[i]) {
        currentPosition[i] += moveDir[i];
      }
      if (dominantStepsRemaining > 1) { 
        err[i] -= stepCount[i];
        if (err[i] < 0) {
          doStep[i] = true;
          err[i] += dominantStepsTotal;
        } else {
          doStep[i] = false;
        }
      }
    }
    dominantStepsRemaining--;
  }
}

void setup() {
  Serial.begin(115200);
  setupConfig();
  
  for (int i = 0; i < NUM_AXES; i++) {
    pinMode(axes[i].stepPin, OUTPUT);
    pinMode(axes[i].dirPin, OUTPUT);
    pinMode(axes[i].limitPin, INPUT_PULLUP);
  }
  
  cli();
  TCCR1A = 0; TCCR1B = 0; OCR1A = 2000; 
  TCCR1B |= (1 << WGM12); 
  TCCR1B |= (1 << CS11); 
  TIMSK1 |= (1 << OCIE1A);
  sei();
  
  Serial.println(F("--- READY ---"));
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
        serialBuffer[serialIdx] = '\0';
        if (serialIdx > 0) {
            processCommand(serialBuffer);
        }
        serialIdx = 0;
    } else if (serialIdx < (int)sizeof(serialBuffer) - 1) {
        serialBuffer[serialIdx++] = c;
    }
  }
  
  if (state == IDLE && head != tail && homeState == HOME_IDLE) {
    startMove(
      moveBuffer[tail].steps[0], moveBuffer[tail].steps[1], 
      moveBuffer[tail].steps[2], moveBuffer[tail].steps[3], 
      moveBuffer[tail].feed
    );
    tail = (tail + 1) % BUFFER_SIZE;
  }

  updateRamp();   
  handleHoming(); 
}
