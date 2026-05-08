////////////////////////////////////////////////////////////////////////////////
// 🧠 CNC 4-AXIS FIRMWARE (VERSION 3.5 - OPTIMIZED & FORCE MODE)
////////////////////////////////////////////////////////////////////////////////

#define NUM_AXES 4

////////////////////////////////////////////////////////////////////////////////
// 🛠️ BLOK 1: PIN DEFINITIES & DIRECT PORT MAPPING (ATmega328P)
////////////////////////////////////////////////////////////////////////////////
// HX711 (Loadcell): SCK=D12 (PB4), DT=D13 (PB5)
#define HX711_SCK_BIT 4
#define HX711_DT_BIT  5
#define HX711_PORT    PORTB
#define HX711_PIN_REG PINB
#define HX711_DDR     DDRB

// X-AS: Step=D4 (PD4), Dir=D5 (PD5), Limit=A3 (PC3)
#define STEP_BIT_X 4
#define DIR_BIT_X  5
#define LIMIT_BIT_X 3
#define LIMIT_PIN_REG_X PINC

// Y-AS: Step=D6 (PD6), Dir=D7 (PD7), Limit=A4 (PC4)
#define STEP_BIT_Y 6
#define DIR_BIT_Y  7
#define LIMIT_BIT_Y 4
#define LIMIT_PIN_REG_Y PINC

// Z-AS: Step=D8 (PB0), Dir=D9 (PB1), Limit=A5 (PC5)
#define STEP_BIT_Z 0
#define DIR_BIT_Z  1
#define LIMIT_BIT_Z 5
#define LIMIT_PIN_REG_Z PINC

// A-AS: Step=D2 (PD2), Dir=D3 (PD3), Limit=A2 (PC2)
#define STEP_BIT_A 2
#define DIR_BIT_A  3
#define LIMIT_BIT_A 2
#define LIMIT_PIN_REG_A PINC

// Original Pin Definitions for setup()
#define STEP_PIN_X 4
#define DIR_PIN_X  5
#define LIMIT_PIN_X A3
#define STEP_PIN_Y 6
#define DIR_PIN_Y  7
#define LIMIT_PIN_Y A4
#define STEP_PIN_Z 8
#define DIR_PIN_Z  9
#define LIMIT_PIN_Z A5
#define STEP_PIN_A 2
#define DIR_PIN_A  3
#define LIMIT_PIN_A A2

////////////////////////////////////////////////////////////////////////////////
// ⚙️ BLOK 2: AS-CONFIGURATIE (Aangepast voor oneindige A-as)
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
  // --- X-AS (Index 0) ---
  axes[0].stepPin         = STEP_PIN_X;
  axes[0].dirPin          = DIR_PIN_X;
  axes[0].limitPin        = LIMIT_PIN_X;
  axes[0].invertDir       = false;
  axes[0].homeDirNegative = true;
  axes[0].isNC            = false;
  axes[0].maxTravelMM     = 275.0;
  axes[0].stepsPerMM      = 266.66666667;

  // --- Y-AS (Index 1) ---
  axes[1].stepPin         = STEP_PIN_Y;
  axes[1].dirPin          = DIR_PIN_Y;
  axes[1].limitPin        = LIMIT_PIN_Y;
  axes[1].invertDir       = true;
  axes[1].homeDirNegative = true;
  axes[1].isNC            = false;
  axes[1].maxTravelMM     = 65.0;
  axes[1].stepsPerMM      = 266.66666667;

  // --- Z-AS (Index 2) ---
  axes[2].stepPin         = STEP_PIN_Z;
  axes[2].dirPin          = DIR_PIN_Z;
  axes[2].limitPin        = LIMIT_PIN_Z;
  axes[2].invertDir       = false;
  axes[2].homeDirNegative = true;
  axes[2].isNC            = true;
  axes[2].maxTravelMM     = 15.0;
  axes[2].stepsPerMM      = 400.0;

  // --- A-AS (Index 3) - Oneindige Draai-as ---
  axes[3].stepPin         = STEP_PIN_A;
  axes[3].dirPin          = DIR_PIN_A;
  axes[3].limitPin        = LIMIT_PIN_A;
  axes[3].invertDir       = true;
  axes[3].homeDirNegative = false;
  axes[3].isNC            = true;
  axes[3].maxTravelMM     = -1.0; // Oneindige travel
  axes[3].stepsPerMM      = 3.555555556; // Stappen per graad
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
enum MotionState { IDLE, MOVING, ALARM };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK_WAIT, HOME_SLOW_SEEK };

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

// HX711 Force Mode Variables
bool forceModeActive = false;
float targetWeightKG = 0;
float loadcell_offset = 0;
float loadcell_divider = 1.0;
float hardLimitKG = 50.0;
float currentWeightKG = 0;
bool isSeekingForce = false;

unsigned long lastRampMicros = 0;
unsigned long homingStart = 0;

////////////////////////////////////////////////////////////////////////////////
// ⚖️ LOADCELL (HX711) READING
////////////////////////////////////////////////////////////////////////////////
long readHX711() {
  unsigned long start = micros();
  while (HX711_PIN_REG & (1 << HX711_DT_BIT)) {
    if (micros() - start > 1000) return 0; // Timeout 1ms
  }

  long data = 0;
  for (int i = 0; i < 24; i++) {
    HX711_PORT |= (1 << HX711_SCK_BIT);
    delayMicroseconds(1);
    data <<= 1;
    if (HX711_PIN_REG & (1 << HX711_DT_BIT)) data++;
    HX711_PORT &= ~(1 << HX711_SCK_BIT);
    delayMicroseconds(1);
  }

  HX711_PORT |= (1 << HX711_SCK_BIT);
  delayMicroseconds(1);
  HX711_PORT &= ~(1 << HX711_SCK_BIT);
  delayMicroseconds(1);

  if (data & 0x800000) data |= 0xFF000000;
  return data;
}

////////////////////////////////////////////////////////////////////////////////
// ⚡ LIMIT SWITCH CHECK (Direct Port Manipulation & Debouncing)
////////////////////////////////////////////////////////////////////////////////
inline bool limitTriggered(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) return false;
  uint8_t triggeredCount = 0;
  for (uint8_t i = 0; i < 5; i++) {
    bool low = false;
    if (axisIndex == 0) low = !(LIMIT_PIN_REG_X & (1 << LIMIT_BIT_X));
    else if (axisIndex == 1) low = !(LIMIT_PIN_REG_Y & (1 << LIMIT_BIT_Y));
    else if (axisIndex == 2) low = !(LIMIT_PIN_REG_Z & (1 << LIMIT_BIT_Z));
    else if (axisIndex == 3) low = !(LIMIT_PIN_REG_A & (1 << LIMIT_BIT_A));
    bool currentTriggered = axes[axisIndex].isNC ? low : !low;
    if (currentTriggered) triggeredCount++;
    delayMicroseconds(10); // Added slight delay for more realistic debounce
  }
  return (triggeredCount == 5);
}

void startMove(long stepsX, long stepsY, long stepsZ, long stepsA, float feed) {
  if (state == ALARM) return;
  long steps[NUM_AXES] = {stepsX, stepsY, stepsZ, stepsA};
  dominantStepsTotal = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = 0; err[i] = 0; doStep[i] = false; moveDir[i] = 0;
  }

  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = labs(steps[i]);
    if (stepCount[i] > dominantStepsTotal) dominantStepsTotal = stepCount[i];
    if (steps[i] != 0) {
      bool dir = (steps[i] > 0);
      if (axes[i].invertDir) dir = !dir;
      if (i == 0) { if (dir) PORTD |= (1 << DIR_BIT_X); else PORTD &= ~(1 << DIR_BIT_X); }
      else if (i == 1) { if (dir) PORTD |= (1 << DIR_BIT_Y); else PORTD &= ~(1 << DIR_BIT_Y); }
      else if (i == 2) { if (dir) PORTB |= (1 << DIR_BIT_Z); else PORTB &= ~(1 << DIR_BIT_Z); }
      else if (i == 3) { if (dir) PORTD |= (1 << DIR_BIT_A); else PORTD &= ~(1 << DIR_BIT_A); }
      moveDir[i] = (steps[i] > 0) ? 1 : -1;
    }
  }

  if (dominantStepsTotal == 0) { state = IDLE; return; }
  for (int i = 0; i < NUM_AXES; i++) {
    err[i] = dominantStepsTotal / 2;
    if (stepCount[i] > 0) {
      err[i] -= stepCount[i];
      if (err[i] < 0) { doStep[i] = true; err[i] += dominantStepsTotal; }
    }
  }
  dominantStepsRemaining = dominantStepsTotal;
  targetSpeed = constrain(feed, 50, maxSpeed);
  currentSpeed = 0; lastRampMicros = micros();
  state = MOVING;
}

void startHomingSequence() {
  head = 0; tail = 0;
  Serial.println(F("--- STARTING HOMING SEQUENCE (Z, Y, X, A) ---"));
  startHomingAxis(2); // Z eerst
}

void startHomingAxis(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) {
    homingAxis = -1; homeState = HOME_IDLE; state = IDLE;
    Serial.println(F(">>> ALL AXES HOMED SUCCESSFULLY."));
    return;
  }
  homingAxis = axisIndex; homingStart = millis();
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
  if (millis() - homingStart > 30000) {
    Serial.println(F("ALARM: Homing timeout!"));
    state = IDLE; homeState = HOME_IDLE; homingAxis = -1; return;
  }

  static unsigned long stateTimer = 0;
  bool hit = limitTriggered(homingAxis);

  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) { state = IDLE; stateTimer = millis(); homeState = HOME_BACKOFF; Serial.println(F(" -> Hit, waiting to backoff...")); }
      break;
    case HOME_BACKOFF:
      if (state == IDLE && millis() - stateTimer > 200) {
        if (limitTriggered(homingAxis)) {
          long backoffStep = axes[homingAxis].homeDirNegative ? 500 : -500;
          startMove((homingAxis == 0 ? backoffStep : 0), (homingAxis == 1 ? backoffStep : 0), (homingAxis == 2 ? backoffStep : 0), (homingAxis == 3 ? backoffStep : 0), homeSlowSpeed);
          Serial.println(F(" -> Sensor active, backing off..."));
        } else { stateTimer = millis(); homeState = HOME_SLOW_SEEK_WAIT; Serial.println(F(" -> Sensor free, waiting to seek slow...")); }
      }
      break;
    case HOME_SLOW_SEEK_WAIT:
      if (state == IDLE && millis() - stateTimer > 200) {
        long slowSeek = axes[homingAxis].homeDirNegative ? -100000 : 100000;
        startMove((homingAxis == 0 ? slowSeek : 0), (homingAxis == 1 ? slowSeek : 0), (homingAxis == 2 ? slowSeek : 0), (homingAxis == 3 ? slowSeek : 0), homeSlowSpeed);
        homeState = HOME_SLOW_SEEK;
      }
      break;
    case HOME_SLOW_SEEK:
      if (state == MOVING && hit) {
         state = IDLE; currentPosition[homingAxis] = 0; homed[homingAxis] = true;
         Serial.print(F("AXIS ")); Serial.print(homingAxis); Serial.println(F(" HOMED OK."));
         int next = -1;
         if (homingAxis == 2) next = 1; else if (homingAxis == 1) next = 0; else if (homingAxis == 0) next = 3;
         stateTimer = millis(); homingAxis = next;
         if (next != -1) homeState = HOME_FAST_SEEK; else { homeState = HOME_IDLE; Serial.println(F(">>> ALL AXES HOMED SUCCESSFULLY.")); }
      }
      break;
  }
  if (homeState == HOME_FAST_SEEK && state == IDLE && homingAxis != -1 && millis() - stateTimer > 300) startHomingAxis(homingAxis);
}

void updateRamp() {
  if (state != MOVING) { currentSpeed = 0; return; }
  unsigned long now = micros();
  float dt = (now - lastRampMicros) / 1000000.0;
  lastRampMicros = now;
  if (currentSpeed < targetSpeed) { currentSpeed += accel * dt; if (currentSpeed > targetSpeed) currentSpeed = targetSpeed; }
  else if (currentSpeed > targetSpeed) { currentSpeed -= accel * dt; if (currentSpeed < targetSpeed) currentSpeed = targetSpeed; }
  float safeSpeed = max(currentSpeed, 10.0);
  OCR1A = (2000000.0 / safeSpeed);
}

////////////////////////////////////////////////////////////////////////////////
// 💬 G-CODE PARSER & COMMUNICATIE (Optimized: No String class)
////////////////////////////////////////////////////////////////////////////////
void sendStatusMessage(const char* title, const char* message) {
  Serial.println(F("\n--------------------------------------------------"));
  Serial.print(F(" >> ")); Serial.println(title);
  if (message != NULL && message[0] != '\0') { Serial.print(F("    ")); Serial.println(message); }
  Serial.println(F("--------------------------------------------------"));
}

float getAxisValue(const char* line, char axisLetter, float defaultVal, bool &found) {
  const char* ptr = strchr(line, axisLetter);
  if (ptr != NULL) { found = true; return strtof(ptr + 1, NULL); }
  found = false; return defaultVal;
}

void processCommand(char* line) {
  char* src = line;
  while (*src == ' ' || *src == '\t') src++;
  if (*src == '\0') return;
  for (char* p = src; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;

  if (strcmp(src, "G28") == 0 || strcmp(src, "HOME") == 0) { startHomingSequence(); return; }
  if (strcmp(src, "G90") == 0) { absoluteMode = true; sendStatusMessage("MODUS: ABSOLUUT (G90)", "Posities MM."); Serial.println(F("ok")); return; }
  if (strcmp(src, "G91") == 0) { absoluteMode = false; sendStatusMessage("MODUS: RELATIEF (G91)", "Posities MM."); Serial.println(F("ok")); return; }
  if (strcmp(src, "M400") == 0) { forceModeActive = true; sendStatusMessage("MODUS: FORCE (M400)", "Z parameter is KG."); Serial.println(F("ok")); return; }
  if (strcmp(src, "M401") == 0) { forceModeActive = false; isSeekingForce = false; sendStatusMessage("MODUS: POSITIE (M401)", "Z parameter is MM."); Serial.println(F("ok")); return; }

  if (strcmp(src, "M114") == 0) {
    Serial.println(F("\n--- ACTUELE MACHINE POSITIE ---"));
    const char labels[] = {'X', 'Y', 'Z', 'A'};
    for (int i = 0; i < NUM_AXES; i++) {
      float pos = (float)currentPosition[i] / axes[i].stepsPerMM;
      Serial.print(labels[i]); Serial.print(F(": ")); Serial.print(pos);
      if (i == 3) Serial.print(F("°  ")); else Serial.print(F("mm  "));
    }
    Serial.println(F("\nok")); return;
  }
 
  if (src[0] == 'G') {
    for(int i = 0; i < NUM_AXES; i++) if (!homed[i]) { Serial.println(F("ERROR: Voer G28 uit!")); return; }
    bool fFound; float feed = getAxisValue(src, 'F', targetSpeed, fFound);
    long stepsToMove[NUM_AXES] = {0, 0, 0, 0};
    bool axisIncluded[NUM_AXES] = {false, false, false, false};
    const char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'};
    bool hasMovement = false;

    for (int i = 0; i < NUM_AXES; i++) {
      float val = getAxisValue(src, axisChars[i], 0.0, axisIncluded[i]);
      if (axisIncluded[i]) {
        if (i == 2 && forceModeActive) { targetWeightKG = val; isSeekingForce = true; axisIncluded[i] = false; }
        else {
          float cur = (float)currentPosition[i] / axes[i].stepsPerMM;
          for (int b = tail; b != head; b = (b + 1) % BUFFER_SIZE) cur += (moveBuffer[b].steps[i] / axes[i].stepsPerMM);
          float delta = absoluteMode ? (val - cur) : val;
          float targetPos = cur + delta;
          if (i != 3) if (targetPos < -0.01 || targetPos > axes[i].maxTravelMM + 0.01) { Serial.print(F("ALARM: Limit ")); Serial.println(axisChars[i]); return; }
          stepsToMove[i] = (long)round(delta * axes[i].stepsPerMM);
          if (stepsToMove[i] != 0) hasMovement = true;
        }
      }
    }
    if (hasMovement || isSeekingForce) {
      int nextHead = (head + 1) % BUFFER_SIZE;
      if (nextHead != tail) {
        for (int i = 0; i < NUM_AXES; i++) moveBuffer[head].steps[i] = stepsToMove[i];
        moveBuffer[head].feed = feed; head = nextHead; Serial.println(F("ok"));
      } else Serial.println(F("ERROR: Buffer vol!"));
    } else Serial.println(F("ok"));
  }
}

ISR(TIMER1_COMPA_vect) {
  if (state == ALARM) return;
  if (homeState != HOME_IDLE && homingAxis >= 0 && limitTriggered(homingAxis) && homeState != HOME_BACKOFF) {
    PORTD &= ~((1 << STEP_BIT_X) | (1 << STEP_BIT_Y) | (1 << STEP_BIT_A));
    PORTB &= ~(1 << STEP_BIT_Z);
    state = IDLE; dominantStepsRemaining = 0; return;
  }
  if (state != MOVING || dominantStepsRemaining <= 0) { state = IDLE; return; }

  static bool phase = false;
  phase = !phase; 
  if (phase) {
    if (doStep[0]) PORTD |= (1 << STEP_BIT_X);
    if (doStep[1]) PORTD |= (1 << STEP_BIT_Y);
    if (doStep[2]) PORTB |= (1 << STEP_BIT_Z);
    if (doStep[3]) PORTD |= (1 << STEP_BIT_A);
  } else {
    PORTD &= ~((1 << STEP_BIT_X) | (1 << STEP_BIT_Y) | (1 << STEP_BIT_A));
    PORTB &= ~(1 << STEP_BIT_Z);
    for (int i = 0; i < NUM_AXES; i++) {
      if (doStep[i]) currentPosition[i] += moveDir[i];
      if (dominantStepsRemaining > 1) { 
        err[i] -= stepCount[i];
        if (err[i] < 0) { doStep[i] = true; err[i] += dominantStepsTotal; } else doStep[i] = false;
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
  HX711_DDR |= (1 << HX711_SCK_BIT);
  HX711_DDR &= ~(1 << HX711_DT_BIT);
  HX711_PORT &= ~(1 << HX711_SCK_BIT);
  
  cli();
  TCCR1A = 0; TCCR1B = 0; OCR1A = 2000; 
  TCCR1B |= (1 << WGM12) | (1 << CS11);
  TIMSK1 |= (1 << OCIE1A);
  sei();
  Serial.println(F("--- READY ---"));
}

void loop() {
  static char cmdBuffer[96];
  static uint8_t cmdIdx = 0;
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdIdx > 0) { cmdBuffer[cmdIdx] = '\0'; processCommand(cmdBuffer); cmdIdx = 0; }
    } else if (cmdIdx < (uint8_t)(sizeof(cmdBuffer) - 1)) cmdBuffer[cmdIdx++] = c;
  }

  // Safety & Force Monitoring
  long raw = readHX711();
  if (raw != 0) {
    currentWeightKG = (float)(raw - (long)loadcell_offset) / loadcell_divider;
    if (currentWeightKG > hardLimitKG) {
      state = ALARM;
      PORTD &= ~((1 << STEP_BIT_X) | (1 << STEP_BIT_Y) | (1 << STEP_BIT_A));
      PORTB &= ~(1 << STEP_BIT_Z);
      Serial.println(F("ALARM: HARD LIMIT KG EXCEEDED!"));
    }
  }

  if (isSeekingForce && state == IDLE) {
    if (abs(currentWeightKG - targetWeightKG) < 0.1) {
      isSeekingForce = false;
      Serial.println(F("Target force reached."));
    } else {
      float diff = targetWeightKG - currentWeightKG;
      long steps = (diff > 0) ? 10 : -10; // Simple seek
      // Check Z soft limit
      float curZ = (float)currentPosition[2] / axes[2].stepsPerMM;
      float nextZ = curZ + (float)steps / axes[2].stepsPerMM;
      if (nextZ >= 0 && nextZ <= axes[2].maxTravelMM) {
        startMove(0, 0, steps, 0, 500);
      } else {
        isSeekingForce = false;
        Serial.println(F("Force seek stopped: Z limit."));
      }
    }
  }
  
  if (state == IDLE && head != tail && homeState == HOME_IDLE && !isSeekingForce) {
    startMove(moveBuffer[tail].steps[0], moveBuffer[tail].steps[1], moveBuffer[tail].steps[2], moveBuffer[tail].steps[3], moveBuffer[tail].feed);
    tail = (tail + 1) % BUFFER_SIZE;
  }
  updateRamp();   
  handleHoming(); 
}
