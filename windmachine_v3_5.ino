////////////////////////////////////////////////////////////////////////////////
// 🧠 CNC 4-AXIS FIRMWARE (VERSION 2.2 - OPTIMIZED)
////////////////////////////////////////////////////////////////////////////////

#define NUM_AXES 4

////////////////////////////////////////////////////////////////////////////////
// 🛠️ BLOK 1: PIN DEFINITIES
////////////////////////////////////////////////////////////////////////////////
// X-AS
#define STEP_PIN_X 4
#define DIR_PIN_X  5
#define LIMIT_PIN_X A3

// Y-AS
#define STEP_PIN_Y 6
#define DIR_PIN_Y  7
#define LIMIT_PIN_Y A4

// Z-AS
#define STEP_PIN_Z 8
#define DIR_PIN_Z  9
#define LIMIT_PIN_Z A5

// A-AS
#define STEP_PIN_A 2
#define DIR_PIN_A  3
#define LIMIT_PIN_A A2

////////////////////////////////////////////////////////////////////////////////
// ⚡ DIRECT PORT MANIPULATION DEFINITIES (ATmega328P / Uno)
////////////////////////////////////////////////////////////////////////////////
// PORT D: Pins 2, 3, 4, 5, 6, 7
#define A_STEP_BIT 2 // PD2
#define A_DIR_BIT  3 // PD3
#define X_STEP_BIT 4 // PD4
#define X_DIR_BIT  5 // PD5
#define Y_STEP_BIT 6 // PD6
#define Y_DIR_BIT  7 // PD7

// PORT B: Pins 8, 9
#define Z_STEP_BIT 0 // PB0
#define Z_DIR_BIT  1 // PB1

// PORT C: Pins A2, A3, A4, A5 (Limit switches)
#define A_LIMIT_BIT 2 // PC2
#define X_LIMIT_BIT 3 // PC3
#define Y_LIMIT_BIT 4 // PC4
#define Z_LIMIT_BIT 5 // PC5

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

AxisConfig axes[4];

void setupConfig() {
  // X-AS
  axes[0].stepPin         = 4;
  axes[0].dirPin          = 5;
  axes[0].limitPin        = A3;
  axes[0].invertDir       = false;
  axes[0].homeDirNegative = true;
  axes[0].isNC            = false;
  axes[0].maxTravelMM     = 275.0;
  axes[0].stepsPerMM      = 266.7;

  // Y-AS
  axes[1].stepPin         = 6;
  axes[1].dirPin          = 7;
  axes[1].limitPin        = A4;
  axes[1].invertDir       = true;
  axes[1].homeDirNegative = true;
  axes[1].isNC            = false;
  axes[1].maxTravelMM     = 65.0;
  axes[1].stepsPerMM      = 266.7;

  // Z-AS
  axes[2].stepPin         = 8;
  axes[2].dirPin          = 9;
  axes[2].limitPin        = A5;
  axes[2].invertDir       = false;
  axes[2].homeDirNegative = true;
  axes[2].isNC            = true;
  axes[2].maxTravelMM     = 15.0;
  axes[2].stepsPerMM      = 400.0;

  // A-AS (Infinite rotation)
  axes[3].stepPin         = 2;
  axes[3].dirPin          = 3;
  axes[3].limitPin        = A2;
  axes[3].invertDir       = true;
  axes[3].homeDirNegative = false;
  axes[3].isNC            = true;
  axes[3].maxTravelMM     = -1.0;
  axes[3].stepsPerMM      = 3.5556;
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

char serialBuffer[128];
int serialIdx = 0;

////////////////////////////////////////////////////////////////////////////////
// 📊 STATUS VARIABELEN
////////////////////////////////////////////////////////////////////////////////
enum MotionState { IDLE, MOVING };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK, HOME_DELAY };

volatile MotionState state = IDLE;
volatile HomeState homeState = HOME_IDLE;
HomeState nextHomeState = HOME_IDLE;
unsigned long homeDelayStart = 0;
unsigned long homeDelayTime = 0;
volatile int homingAxis = -1; 

volatile bool homingSequenceActive = false;
volatile bool limitHitDetected = false;

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

////////////////////////////////////////////////////////////////////////////////
// ⚡ LIMIT SWITCH CHECK
////////////////////////////////////////////////////////////////////////////////
inline bool limitTriggeredRaw(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) return false;
  bool raw = false;
  uint8_t pinVal = PINC;
  switch(axisIndex) {
    case 0: raw = !(pinVal & (1 << X_LIMIT_BIT)); break;
    case 1: raw = !(pinVal & (1 << Y_LIMIT_BIT)); break;
    case 2: raw = !(pinVal & (1 << Z_LIMIT_BIT)); break;
    case 3: raw = !(pinVal & (1 << A_LIMIT_BIT)); break;
  }
  return axes[axisIndex].isNC ? raw : !raw;
}

inline bool limitTriggered(int axisIndex) {
  return limitTriggeredRaw(axisIndex);
}

void startMove(long stepsX, long stepsY, long stepsZ, long stepsA, float feed) {
  long steps[NUM_AXES] = {stepsX, stepsY, stepsZ, stepsA};

  dominantStepsTotal = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = 0;
    err[i] = 0;
    doStep[i] = false;
    moveDir[i] = 0;
  }

  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = labs(steps[i]);
    if (stepCount[i] > dominantStepsTotal) {
      dominantStepsTotal = stepCount[i];
    }
    if (steps[i] != 0) {
      bool dir = (steps[i] > 0);
      if (axes[i].invertDir) dir = !dir;
      if (i == 0) { if (dir) PORTD |= (1 << X_DIR_BIT); else PORTD &= ~(1 << X_DIR_BIT); }
      else if (i == 1) { if (dir) PORTD |= (1 << Y_DIR_BIT); else PORTD &= ~(1 << Y_DIR_BIT); }
      else if (i == 2) { if (dir) PORTB |= (1 << Z_DIR_BIT); else PORTB &= ~(1 << Z_DIR_BIT); }
      else if (i == 3) { if (dir) PORTD |= (1 << A_DIR_BIT); else PORTD &= ~(1 << A_DIR_BIT); }
      moveDir[i] = (steps[i] > 0) ? 1 : -1;
    }
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
void startHomingSequence() {
  head = 0;
  tail = 0;
  limitHitDetected = false;
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
  limitHitDetected = false;
  Serial.print(F("AXIS ")); Serial.print(axisIndex);

  long backoffAmt = axes[axisIndex].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;
  if (limitTriggered(axisIndex)) {
    Serial.println(F(": ALREADY ON LIMIT -> STARTING BACKOFF"));
    homeState = HOME_BACKOFF;
    startMove((axisIndex == 0 ? backoffAmt : 0), (axisIndex == 1 ? backoffAmt : 0), (axisIndex == 2 ? backoffAmt : 0), (axisIndex == 3 ? backoffAmt : 0), homeSlowSpeed);
  } else {
    Serial.println(F(": FAST SEEK"));
    homeState = HOME_FAST_SEEK;
    long bigMove = axes[axisIndex].homeDirNegative ? -1000000 : 1000000;
    startMove((axisIndex == 0 ? bigMove : 0), (axisIndex == 1 ? bigMove : 0), (axisIndex == 2 ? bigMove : 0), (axisIndex == 3 ? bigMove : 0), homeFastSpeed);
  }
}

void handleHoming() {
  if (homeState == HOME_IDLE || homingAxis == -1) return;

  if (homeState == HOME_DELAY) {
    if (millis() - homeDelayStart >= homeDelayTime) {
      homeState = nextHomeState;
      if (homeState == HOME_BACKOFF) {
        limitHitDetected = false;
        long backoffAmt = axes[homingAxis].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;
        startMove((homingAxis == 0 ? backoffAmt : 0), (homingAxis == 1 ? backoffAmt : 0), (homingAxis == 2 ? backoffAmt : 0), (homingAxis == 3 ? backoffAmt : 0), homeSlowSpeed);
        Serial.println(F(" -> Hit, backing off..."));
      } else if (homeState == HOME_SLOW_SEEK) {
        limitHitDetected = false;
        long slowSeek = axes[homingAxis].homeDirNegative ? -100000 : 100000;
        startMove((homingAxis == 0 ? slowSeek : 0), (homingAxis == 1 ? slowSeek : 0), (homingAxis == 2 ? slowSeek : 0), (homingAxis == 3 ? slowSeek : 0), homeSlowSpeed);
        Serial.println(F(" -> Sensor vrij, seeking slow..."));
      } else if (homeState == HOME_IDLE) {
          int next = -1;
          if (homingAxis == 2) next = 1;
          else if (homingAxis == 1) next = 0;
          else if (homingAxis == 0) next = 3;
          else if (homingAxis == 3) next = -1;
          if (next != -1) startHomingAxis(next);
          else { homingAxis = -1; Serial.println(F(">>> ALL AXES HOMED SUCCESSFULLY.")); }
      }
    }
    return;
  }

  if (millis() - homingStart > 20000) {
    Serial.println(F("ALARM: Homing timeout!"));
    state = IDLE; homeState = HOME_IDLE; homingAxis = -1;
    return;
  }

  bool hit = limitHitDetected || limitTriggered(homingAxis);

  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) {
        state = IDLE;
        limitHitDetected = false;
        homeDelayStart = millis(); homeDelayTime = 150; homeState = HOME_DELAY; nextHomeState = HOME_BACKOFF;
      }
      break;
    case HOME_BACKOFF:
      if (!limitTriggered(homingAxis)) {
        state = IDLE;
        homeDelayStart = millis(); homeDelayTime = 150; homeState = HOME_DELAY; nextHomeState = HOME_SLOW_SEEK;
      } else {
        if (state == IDLE) {
          long backoffStep = axes[homingAxis].homeDirNegative ? 300 : -300;
          startMove((homingAxis == 0 ? backoffStep : 0), (homingAxis == 1 ? backoffStep : 0), (homingAxis == 2 ? backoffStep : 0), (homingAxis == 3 ? backoffStep : 0), homeSlowSpeed);
          Serial.println(F(" -> Sensor ingedrukt, rij nu actief terug..."));
        }
      }
      break;
    case HOME_SLOW_SEEK:
      if (hit) {
        state = IDLE;
        limitHitDetected = false;
        currentPosition[homingAxis] = 0; 
        homed[homingAxis] = true;
        Serial.print(F("AXIS ")); Serial.print(homingAxis); Serial.println(F(" HOMED OK."));
        homeDelayStart = millis(); homeDelayTime = 200; homeState = HOME_DELAY; nextHomeState = HOME_IDLE;
      }
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// 🔄 RAMPING & SPEED CONTROL
////////////////////////////////////////////////////////////////////////////////
void updateRamp() {
  if (state != MOVING) { currentSpeed = 0; return; }
  unsigned long now = micros();
  float dt = (now - lastRampMicros) / 1000000.0; 
  lastRampMicros = now;
  if (currentSpeed < targetSpeed) { currentSpeed += accel * dt; if (currentSpeed > targetSpeed) currentSpeed = targetSpeed; }
  else if (currentSpeed > targetSpeed) { currentSpeed -= accel * dt; if (currentSpeed < targetSpeed) currentSpeed = targetSpeed; }
  float safeSpeed = max(currentSpeed, 10.0);
  long ocrVal = (2000000.0 / safeSpeed);
  if (ocrVal < 100) ocrVal = 100;
  OCR1A = ocrVal;
}

////////////////////////////////////////////////////////////////////////////////
// 💬 G-CODE PARSER
////////////////////////////////////////////////////////////////////////////////
void sendStatusMessage(const __FlashStringHelper* title, const __FlashStringHelper* message) {
  Serial.println(F("\n--------------------------------------------------"));
  Serial.print(F(" >> ")); Serial.println(title);
  if (message != (const __FlashStringHelper*)NULL) { Serial.print(F("    ")); Serial.println(message); }
  Serial.println(F("--------------------------------------------------"));
}

float getAxisValue(const char* line, char axisLetter, float defaultVal, bool &found) {
  const char* ptr = strchr(line, axisLetter);
  if (ptr != NULL) { found = true; return atof(ptr + 1); }
  found = false;
  return defaultVal;
}

void processCommand(char* line) {
  int len = strlen(line);
  while(len > 0 && (line[len-1] == ' ' || line[len-1] == '\r' || line[len-1] == '\n')) { line[--len] = '\0'; }
  char* p = line;
  while(*p) { if(*p >= 'a' && *p <= 'z') *p -= 32; p++; }
  if (len == 0) return;

  if (strcmp(line, "G28") == 0 || strcmp(line, "HOME") == 0) { startHomingSequence(); return; }
  if (strcmp(line, "G90") == 0) { absoluteMode = true; sendStatusMessage(F("MODUS: ABSOLUUT (G90)"), F("Machine nulpunt.")); Serial.println(F("ok")); return; }
  if (strcmp(line, "G91") == 0) { absoluteMode = false; sendStatusMessage(F("MODUS: RELATIEF (G91)"), F("Huidige locatie.")); Serial.println(F("ok")); return; }
  if (strcmp(line, "M114") == 0) {
    Serial.println(F("\n--- MACHINE POSITIE ---"));
    char labels[] = {'X', 'Y', 'Z', 'A'};
    for (int i = 0; i < NUM_AXES; i++) {
      noInterrupts(); long posSteps = currentPosition[i]; interrupts();
      float pos = (float)posSteps / axes[i].stepsPerMM;
      Serial.print(labels[i]); Serial.print(F(": ")); Serial.print(pos); Serial.print(i == 3 ? F("°  ") : F("mm  "));
    }
    Serial.println(F("\n-----------------------")); Serial.println(F("ok")); return;
  }
 
  if (line[0] == 'G') {
    for(int i = 0; i < NUM_AXES; i++) { if (!homed[i]) { Serial.println(F("ERROR: Voer eerst G28 uit!")); return; } }
    bool fFound; float feed = getAxisValue(line, 'F', targetSpeed, fFound);
    long stepsToMove[NUM_AXES] = {0, 0, 0, 0}; bool axisIncluded[NUM_AXES] = {false, false, false, false};
    char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'}; bool hasMovement = false;
    for (int i = 0; i < NUM_AXES; i++) {
      float val = getAxisValue(line, axisChars[i], 0.0, axisIncluded[i]);
      if (axisIncluded[i]) {
        float currentPosUnit = (float)currentPosition[i] / axes[i].stepsPerMM;
        for (int b = tail; b != head; b = (b + 1) % BUFFER_SIZE) { currentPosUnit += (moveBuffer[b].steps[i] / axes[i].stepsPerMM); }
        float deltaUnit = absoluteMode ? (val - currentPosUnit) : val;
        float newPosUnit = currentPosUnit + deltaUnit;
        if (i != 3) { if (newPosUnit < -0.001 || newPosUnit > axes[i].maxTravelMM + 0.001) { Serial.print(F("ALARM: Soft limit ")); Serial.println(axisChars[i]); return; } }
        stepsToMove[i] = lround(deltaUnit * axes[i].stepsPerMM);
        if (stepsToMove[i] != 0) hasMovement = true;
      }
    }
    if (hasMovement) {
      int nextHead = (head + 1) % BUFFER_SIZE;
      if (nextHead != tail) { for (int i = 0; i < NUM_AXES; i++) { moveBuffer[head].steps[i] = stepsToMove[i]; } moveBuffer[head].feed = feed; head = nextHead; Serial.println(F("ok")); }
      else { Serial.println(F("ERROR: Buffer vol!")); }
    } else { Serial.println(F("ok")); }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ⏱️ TIMER ISR
////////////////////////////////////////////////////////////////////////////////
ISR(TIMER1_COMPA_vect) {
  static uint8_t limitCount = 0;
  if (homeState != HOME_IDLE && homingAxis >= 0 && homeState != HOME_BACKOFF) {
    if (limitTriggeredRaw(homingAxis)) {
      limitCount++;
      if (limitCount >= 10) {
        limitHitDetected = true;
        PORTD &= ~((1 << X_STEP_BIT) | (1 << Y_STEP_BIT) | (1 << A_STEP_BIT)); PORTB &= ~(1 << Z_STEP_BIT);
        state = IDLE; dominantStepsRemaining = 0; return;
      }
    } else { limitCount = 0; }
  } else { limitCount = 0; }

  if (state != MOVING || dominantStepsRemaining <= 0) { state = IDLE; return; }
  static bool phase = false;
  phase = !phase; 
  if (phase) {
    if (doStep[0]) PORTD |= (1 << X_STEP_BIT);
    if (doStep[1]) PORTD |= (1 << Y_STEP_BIT);
    if (doStep[2]) PORTB |= (1 << Z_STEP_BIT);
    if (doStep[3]) PORTD |= (1 << A_STEP_BIT);
  } else {
    PORTD &= ~((1 << X_STEP_BIT) | (1 << Y_STEP_BIT) | (1 << A_STEP_BIT)); PORTB &= ~(1 << Z_STEP_BIT);
    for (int i = 0; i < NUM_AXES; i++) {
      if (doStep[i]) { currentPosition[i] += moveDir[i]; }
      if (dominantStepsRemaining > 1) { 
        err[i] -= stepCount[i];
        if (err[i] < 0) { doStep[i] = true; err[i] += dominantStepsTotal; }
        else { doStep[i] = false; }
      }
    }
    dominantStepsRemaining--;
  }
}

void setup() {
  Serial.begin(115200);
  setupConfig();
  for (int i = 0; i < NUM_AXES; i++) { pinMode(axes[i].stepPin, OUTPUT); pinMode(axes[i].dirPin, OUTPUT); pinMode(axes[i].limitPin, INPUT_PULLUP); }
  cli(); TCCR1A = 0; TCCR1B = 0; OCR1A = 2000; TCCR1B |= (1 << WGM12); TCCR1B |= (1 << CS11); TIMSK1 |= (1 << OCIE1A); sei();
  Serial.println(F("--- READY ---"));
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') { serialBuffer[serialIdx] = '\0'; processCommand(serialBuffer); serialIdx = 0; }
    else if (c != '\r' && serialIdx < sizeof(serialBuffer) - 1) { serialBuffer[serialIdx++] = c; }
  }
  if (state == IDLE && head != tail && homeState == HOME_IDLE) {
    startMove(moveBuffer[tail].steps[0], moveBuffer[tail].steps[1], moveBuffer[tail].steps[2], moveBuffer[tail].steps[3], moveBuffer[tail].feed);
    tail = (tail + 1) % BUFFER_SIZE;
  }
  updateRamp(); handleHoming();
}