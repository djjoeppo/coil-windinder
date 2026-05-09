////////////////////////////////////////////////////////////////////////////////
// 🧠 CNC 4-AXIS FIRMWARE (VERSION 3.6 - SMOOTH MOTION & FORCE MODE)
// Gebaseerd op Windmachine v3.5 - Geoptimaliseerd door Jules
////////////////////////////////////////////////////////////////////////////////

#define NUM_AXES 4

////////////////////////////////////////////////////////////////////////////////
// 🛠️ BLOK 1: CENTRALE CONFIGURATIE (Pas hier de hardware aan)
////////////////////////////////////////////////////////////////////////////////

// --- PIN DEFINITIES (Direct Port Mapping voor ATmega328P) ---
// X-AS: Step=4 (PD4), Dir=5 (PD5), Limit=A3 (PC3)
// Y-AS: Step=6 (PD6), Dir=7 (PD7), Limit=A4 (PC4)
// Z-AS: Step=8 (PB0), Dir=9 (PB1), Limit=A5 (PC5)
// A-AS: Step=2 (PD2), Dir=3 (PD3), Limit=A2 (PC2)
#define STEP_PIN_X  4
#define DIR_PIN_X   5
#define LIMIT_PIN_X A3

#define STEP_PIN_Y  6
#define DIR_PIN_Y   7
#define LIMIT_PIN_Y A4

#define STEP_PIN_Z  8
#define DIR_PIN_Z   9
#define LIMIT_PIN_Z A5

#define STEP_PIN_A  2
#define DIR_PIN_A   3
#define LIMIT_PIN_A A2

// HX711 Loadcell Pinnen (A0=SCK, A1=DT)
#define HX711_SCK   A0 // PC0
#define HX711_DT    A1 // PC1

// --- MACHINE LIMITS ---
#define MAX_TRAVEL_X  275.0
#define MAX_TRAVEL_Y  65.0
#define MAX_TRAVEL_Z  15.0
#define HARD_LIMIT_KG 5.0 // Machine stopt boven dit gewicht in Force Mode

// --- BEWEGING ---
#define STEPS_PER_MM_X  266.7
#define STEPS_PER_MM_Y  266.7
#define STEPS_PER_MM_Z  400.0
#define STEPS_PER_DEG_A 3.5556

#define DEFAULT_ACCEL     1500.0 // Stappen per seconde^2
#define DEFAULT_MAX_SPEED 30000.0 // Maximale stappen per seconde

// --- HX711 KALIBRATIE ---
float loadcell_divider = 228.0;
long loadcell_offset   = 0;

////////////////////////////////////////////////////////////////////////////////
// ⚙️ BLOK 2: AS-CONFIGURATIE & STATUS
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

// Initialiseer de assen met de waarden uit BLOK 1
void setupConfig() {
  // X-AS (Index 0)
  axes[0] = {STEP_PIN_X, DIR_PIN_X, LIMIT_PIN_X, false, true, false, MAX_TRAVEL_X, STEPS_PER_MM_X};
  // Y-AS (Index 1)
  axes[1] = {STEP_PIN_Y, DIR_PIN_Y, LIMIT_PIN_Y, true, true, false, MAX_TRAVEL_Y, STEPS_PER_MM_Y};
  // Z-AS (Index 2)
  axes[2] = {STEP_PIN_Z, DIR_PIN_Z, LIMIT_PIN_Z, false, true, true, MAX_TRAVEL_Z, STEPS_PER_MM_Z};
  // A-AS (Index 3) - Oneindige rotatie
  axes[3] = {STEP_PIN_A, DIR_PIN_A, LIMIT_PIN_A, true, false, true, -1.0, STEPS_PER_DEG_A};
}

enum MotionState { IDLE, MOVING };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK };

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
float targetSpeed = 0;
volatile float currentExitSpeed = 0; // Snelheid aan einde van huidig segment
float accel = DEFAULT_ACCEL;
float maxSpeed = DEFAULT_MAX_SPEED;

bool absoluteMode = true;
bool forceMode = false;

unsigned long lastRampMicros = 0;
unsigned long homingStart = 0;
unsigned long homeStateStart = 0;

float homeFastSpeed = 3000;
float homeSlowSpeed = 120;
long homeBackoffSteps = 1000;

////////////////////////////////////////////////////////////////////////////////
// 🚀 BLOK 3: BUFFER & LOOK-AHEAD STRUCT
////////////////////////////////////////////////////////////////////////////////

struct Move {
  long steps[NUM_AXES];
  float feed;
  float exitSpeed; // Berekende junction speed naar volgend segment
};

#define BUFFER_SIZE 16
Move moveBuffer[BUFFER_SIZE];
volatile int head = 0;
volatile int tail = 0;

char cmdBuffer[96];
int cmdIdx = 0;

////////////////////////////////////////////////////////////////////////////////
// ⚖️ BLOK 4: HX711 LOADCELL LOGICA (Direct Port Manipulation)
////////////////////////////////////////////////////////////////////////////////

#define HX711_SCK_HIGH() PORTC |= (1 << 0)
#define HX711_SCK_LOW()  PORTC &= ~(1 << 0)
#define HX711_DT_READ()  (PINC & (1 << 1))

long readHX711() {
  unsigned long timeout = millis();
  while (HX711_DT_READ()) {
    if (millis() - timeout > 250) return 0; // Timeout na 250ms
  }

  long value = 0;
  cli(); // Interrupts uit voor exacte bit-bang timing
  for (int i = 0; i < 24; i++) {
    HX711_SCK_HIGH();
    delayMicroseconds(1);
    value = (value << 1) | (HX711_DT_READ() ? 1 : 0);
    HX711_SCK_LOW();
    delayMicroseconds(1);
  }
  // Extra pulse voor gain 128 (kanaal A)
  HX711_SCK_HIGH(); delayMicroseconds(1); HX711_SCK_LOW();
  sei();

  if (value & 0x800000) value |= 0xFF000000; // Sign extension voor 24-bit naar 32-bit long
  return value;
}

float getWeightKG() {
  long raw = readHX711();
  if (raw == 0) return 0.0;
  return (float)(raw - loadcell_offset) / loadcell_divider / 1000.0;
}

////////////////////////////////////////////////////////////////////////////////
// ⚡ BLOK 5: LIMIT SWITCH & HOMING
////////////////////////////////////////////////////////////////////////////////

inline bool limitTriggered(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) return false;
  int activeCount = 0;
  // Multi-sampling voor stabiliteit (software debounce)
  for (int i = 0; i < 5; i++) {
    bool raw;
    switch(axisIndex) {
      case 0: raw = !(PINC & (1 << 3)); break;
      case 1: raw = !(PINC & (1 << 4)); break;
      case 2: raw = !(PINC & (1 << 5)); break;
      case 3: raw = !(PINC & (1 << 2)); break;
      default: return false;
    }
    if (axes[axisIndex].isNC ? raw : !raw) activeCount++;
  }
  return (activeCount >= 3);
}

void startMove(long stepsX, long stepsY, long stepsZ, long stepsA, float feed, float exitSpd) {
  long steps[NUM_AXES] = {stepsX, stepsY, stepsZ, stepsA};
  dominantStepsTotal = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = labs(steps[i]);
    if (stepCount[i] > dominantStepsTotal) dominantStepsTotal = stepCount[i];
    err[i] = 0;
    doStep[i] = false;
    moveDir[i] = 0;
    if (steps[i] != 0) {
      bool dir = (steps[i] > 0) ^ axes[i].invertDir;
      // Direct Port Manipulation voor DIR pinnen
      switch(i) {
        case 0: if (dir) PORTD |= (1 << 5); else PORTD &= ~(1 << 5); break;
        case 1: if (dir) PORTD |= (1 << 7); else PORTD &= ~(1 << 7); break;
        case 2: if (dir) PORTB |= (1 << 1); else PORTB &= ~(1 << 1); break;
        case 3: if (dir) PORTD |= (1 << 3); else PORTD &= ~(1 << 3); break;
      }
      moveDir[i] = (steps[i] > 0) ? 1 : -1;
    }
  }
  if (dominantStepsTotal == 0) { state = IDLE; return; }

  // Bresenham initialisatie
  for (int i = 0; i < NUM_AXES; i++) {
    err[i] = dominantStepsTotal / 2;
    if (stepCount[i] > 0) {
      err[i] -= stepCount[i];
      if (err[i] < 0) { doStep[i] = true; err[i] += dominantStepsTotal; }
    }
  }
  dominantStepsRemaining = dominantStepsTotal;
  targetSpeed = constrain(feed, 50, maxSpeed);
  currentExitSpeed = exitSpd;

  // Momentum behoud: start niet vanaf 0 als we al bewegen!
  if (currentSpeed < 50) currentSpeed = 50;
  lastRampMicros = micros();
  state = MOVING;
}

void startHomingAxis(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) {
    homingAxis = -1; homeState = HOME_IDLE; state = IDLE;
    Serial.println(F(">>> ALL AXES HOMED."));
    return;
  }
  homingAxis = axisIndex;
  homingStart = millis();
  long backoffAmt = axes[axisIndex].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;
  if (limitTriggered(axisIndex)) {
    homeState = HOME_BACKOFF;
    homeStateStart = millis();
    startMove(axisIndex==0?backoffAmt:0, axisIndex==1?backoffAmt:0, axisIndex==2?backoffAmt:0, axisIndex==3?backoffAmt:0, homeSlowSpeed, 0);
  } else {
    homeState = HOME_FAST_SEEK;
    long bigMove = axes[axisIndex].homeDirNegative ? -1000000 : 1000000;
    startMove(axisIndex==0?bigMove:0, axisIndex==1?bigMove:0, axisIndex==2?bigMove:0, axisIndex==3?bigMove:0, homeFastSpeed, 0);
  }
}

void handleHoming() {
  if (homeState == HOME_IDLE) return;
  if (millis() - homingStart > 30000) {
    Serial.println(F("ALARM: Homing timeout!"));
    state = IDLE; homeState = HOME_IDLE; return;
  }
  bool hit = limitTriggered(homingAxis);
  unsigned long now = millis();
  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) { state = IDLE; homeStateStart = now; homeState = HOME_BACKOFF; }
      break;
    case HOME_BACKOFF:
      if (state == IDLE && now - homeStateStart > 200) {
        if (limitTriggered(homingAxis)) {
          long b = axes[homingAxis].homeDirNegative ? 300 : -300;
          startMove(homingAxis==0?b:0, homingAxis==1?b:0, homingAxis==2?b:0, homingAxis==3?b:0, homeSlowSpeed, 0);
        } else {
          homeStateStart = now; homeState = HOME_SLOW_SEEK;
        }
      }
      break;
    case HOME_SLOW_SEEK:
      if (state == IDLE && now - homeStateStart > 200) {
        if (hit) {
          currentPosition[homingAxis] = 0; homed[homingAxis] = true;
          int n = (homingAxis==2?1 : (homingAxis==1?0 : (homingAxis==0?3 : -1)));
          homeState = HOME_IDLE; startHomingAxis(n);
        } else {
          long s = axes[homingAxis].homeDirNegative ? -1000 : 1000;
          startMove(homingAxis==0?s:0, homingAxis==1?s:0, homingAxis==2?s:0, homingAxis==3?s:0, homeSlowSpeed, 0);
        }
      }
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// 🔄 BLOK 6: VLOEIENDE MOTION PLANNER (Look-ahead & Ramping)
////////////////////////////////////////////////////////////////////////////////

// Wiskunde achter momentum: Bereken junction speed op basis van hoek tussen segmenten via dot product.
// Als de hoek klein is (m1 en m2 liggen in elkaars verlengde), blijft de snelheid hoog.
float calculateJunctionSpeed(Move &m1, Move &m2) {
  float dot = 0, mag1 = 0, mag2 = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    dot += (float)m1.steps[i] * m2.steps[i];
    mag1 += (float)m1.steps[i] * m1.steps[i];
    mag2 += (float)m2.steps[i] * m2.steps[i];
  }
  if (mag1 < 1 || mag2 < 1) return 0;
  float cosine = dot / (sqrt(mag1) * sqrt(mag2));
  // junctionFactor: 1.0 = rechte lijn (vol momentum), 0.0 = 180 graden bocht (volledige stop)
  float junctionFactor = constrain((cosine + 1.0) / 2.0, 0.0, 1.0);
  return min(m1.feed, m2.feed) * junctionFactor;
}

void updateRamp() {
  if (state != MOVING) { currentSpeed = 0; return; }
  unsigned long now = micros();
  float dt = (now - lastRampMicros) / 1000000.0; 
  lastRampMicros = now;

  float endSpeed = currentExitSpeed;
  // Deceleratie-wiskunde (Trapezoidal profile): s = (v^2 - u^2) / (2a)
  // Bepaalt of we moeten beginnen met remmen om op tijd de exitSpeed te bereiken.
  float stepsToBrake = (abs(currentSpeed * currentSpeed - endSpeed * endSpeed)) / (2.0 * accel);

  if (dominantStepsRemaining < stepsToBrake && currentSpeed > endSpeed) {
    currentSpeed -= accel * dt;
    if (currentSpeed < endSpeed) currentSpeed = endSpeed;
  } else if (currentSpeed < targetSpeed) {
    currentSpeed += accel * dt;
    if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= accel * dt;
    if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  float safeSpeed = max(currentSpeed, 10.0);
  OCR1A = (2000000.0 / safeSpeed); // Timer 1 @ 2MHz (Prescaler 8)
}

////////////////////////////////////////////////////////////////////////////////
// 💬 BLOK 7: G-CODE PARSER
////////////////////////////////////////////////////////////////////////////////

void sendStatusMessage(const __FlashStringHelper* title, const __FlashStringHelper* message) {
  Serial.println(F("\n--------------------------------------------------"));
  Serial.print(F(" >> ")); Serial.println(title);
  if (message != (const __FlashStringHelper*)NULL) { Serial.print(F("    ")); Serial.println(message); }
  Serial.println(F("--------------------------------------------------"));
}

float getAxisValue(const char* line, char axisLetter, float defaultVal, bool &found) {
  const char* ptr = strchr(line, axisLetter);
  if (ptr != NULL) { found = true; return strtof(ptr + 1, NULL); }
  found = false; return defaultVal;
}

void processCommand(char* line) {
  int len = strlen(line);
  while (len > 0 && isspace(line[len-1])) line[--len] = '\0';
  char* start = line;
  while (*start && isspace(*start)) start++;
  for (char* p = start; *p; p++) *p = toupper(*p);
  if (*start == '\0') return;

  if (strcmp(start, "G28") == 0 || strcmp(start, "HOME") == 0) {
    for(int i=0; i<NUM_AXES; i++) homed[i]=false;
    head = tail; state = IDLE; startHomingAxis(2); return;
  }
  if (strcmp(start, "G90") == 0) { absoluteMode = true; Serial.println(F("ok")); return; }
  if (strcmp(start, "G91") == 0) { absoluteMode = false; Serial.println(F("ok")); return; }
  if (strcmp(start, "M400") == 0) { forceMode = true; sendStatusMessage(F("FORCE MODE: ON"), F("Stop bij hardLimitKG")); Serial.println(F("ok")); return; }
  if (strcmp(start, "M401") == 0) { forceMode = false; sendStatusMessage(F("FORCE MODE: OFF"), NULL); Serial.println(F("ok")); return; }
  if (strcmp(start, "M402") == 0) { loadcell_offset = readHX711(); sendStatusMessage(F("LOADCELL: TARED"), NULL); Serial.println(F("ok")); return; }

  if (strcmp(start, "M114") == 0) {
    char labels[] = {'X', 'Y', 'Z', 'A'};
    for (int i = 0; i < NUM_AXES; i++) {
      float pos = (float)currentPosition[i] / axes[i].stepsPerMM;
      Serial.print(labels[i]); Serial.print(F(": ")); Serial.print(pos); Serial.print(F(" "));
    }
    Serial.println(F("\nok")); return;
  }
 
  if (start[0] == 'G') {
    for(int i = 0; i < NUM_AXES; i++) if (!homed[i]) { Serial.println(F("ERROR: Voer eerst G28 uit!")); return; }
    bool fFound;
    float feed = getAxisValue(start, 'F', targetSpeed, fFound);
    long stepsToMove[NUM_AXES] = {0, 0, 0, 0};
    bool axisIncluded[NUM_AXES] = {false, false, false, false};
    char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'};
    bool hasMovement = false;

    for (int i = 0; i < NUM_AXES; i++) {
      float val = getAxisValue(start, axisChars[i], 0.0, axisIncluded[i]);
      if (axisIncluded[i]) {
        float cur = (float)currentPosition[i] / axes[i].stepsPerMM;
        for (int b = tail; b != head; b = (b + 1) % BUFFER_SIZE) cur += (moveBuffer[b].steps[i] / axes[i].stepsPerMM);
        float delta = absoluteMode ? (val - cur) : val;
        if (i != 3) {
          float np = cur + delta;
          if (np < -0.01 || np > axes[i].maxTravelMM + 0.01) { Serial.println(F("ALARM: Soft limit!")); return; }
        }
        stepsToMove[i] = lround(delta * axes[i].stepsPerMM);
        if (stepsToMove[i] != 0) hasMovement = true;
      }
    }

    if (hasMovement) {
      int nextHead = (head + 1) % BUFFER_SIZE;
      if (nextHead != tail) {
        for (int i = 0; i < NUM_AXES; i++) moveBuffer[head].steps[i] = stepsToMove[i];
        moveBuffer[head].feed = feed;
        moveBuffer[head].exitSpeed = 0; // Default: stop aan einde move

        // Look-ahead Planner: Update exitSpeed van vorig segment
        int prev = (head == 0) ? BUFFER_SIZE - 1 : head - 1;
        if (head != tail && prev != tail) {
            moveBuffer[prev].exitSpeed = calculateJunctionSpeed(moveBuffer[prev], moveBuffer[head]);
        }
        head = nextHead;
        Serial.println(F("ok"));
      } else Serial.println(F("ERROR: Buffer vol!"));
    } else Serial.println(F("ok"));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ⏱️ BLOK 8: TIMER ISR (Direct Port Control)
////////////////////////////////////////////////////////////////////////////////

ISR(TIMER1_COMPA_vect) {
  // Snelle hardware limit check tijdens homing
  if (homeState != HOME_IDLE && homingAxis >= 0 && limitTriggered(homingAxis) && homeState != HOME_BACKOFF) {
    PORTD &= ~((1 << 4) | (1 << 6) | (1 << 2)); PORTB &= ~(1 << 0);
    state = IDLE; dominantStepsRemaining = 0; return;
  }
  if (state != MOVING || dominantStepsRemaining <= 0) { state = IDLE; return; }

  static bool phase = false; phase = !phase;
  if (phase) {
    // Zet STEP pinnen HIGH
    if (doStep[0]) PORTD |= (1 << 4); if (doStep[1]) PORTD |= (1 << 6);
    if (doStep[2]) PORTB |= (1 << 0); if (doStep[3]) PORTD |= (1 << 2);
  } else {
    // Zet STEP pinnen LOW
    PORTD &= ~((1 << 4) | (1 << 6) | (1 << 2)); PORTB &= ~(1 << 0);
    for (int i = 0; i < NUM_AXES; i++) {
      if (doStep[i]) currentPosition[i] += moveDir[i];
      if (dominantStepsRemaining > 1) { 
        err[i] -= stepCount[i];
        if (err[i] < 0) { doStep[i] = true; err[i] += dominantStepsTotal; }
        else doStep[i] = false;
      }
    }
    dominantStepsRemaining--;
  }
}

////////////////////////////////////////////////////////////////////////////////
// 🔄 BLOK 9: SETUP & LOOP
////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  setupConfig();
  for (int i = 0; i < NUM_AXES; i++) {
    pinMode(axes[i].stepPin, OUTPUT); pinMode(axes[i].dirPin, OUTPUT);
    pinMode(axes[i].limitPin, INPUT_PULLUP);
  }
  pinMode(HX711_SCK, OUTPUT); pinMode(HX711_DT, INPUT);
  
  cli();
  TCCR1A = 0; TCCR1B = 0; OCR1A = 2000; 
  TCCR1B |= (1 << WGM12) | (1 << CS11); // CTC Mode, Prescaler 8
  TIMSK1 |= (1 << OCIE1A);
  sei();
  Serial.println(F("--- FIRMWARE 3.6 READY ---"));
}

void loop() {
  // Non-blocking Serial parser
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') { cmdBuffer[cmdIdx] = '\0'; processCommand(cmdBuffer); cmdIdx = 0; }
    else if (c != '\r' && cmdIdx < (int)sizeof(cmdBuffer) - 1) cmdBuffer[cmdIdx++] = c;
  }
  
  // Planner execution
  if (state == IDLE && head != tail && homeState == HOME_IDLE) {
    startMove(moveBuffer[tail].steps[0], moveBuffer[tail].steps[1],
              moveBuffer[tail].steps[2], moveBuffer[tail].steps[3],
              moveBuffer[tail].feed, moveBuffer[tail].exitSpeed);
    tail = (tail + 1) % BUFFER_SIZE;
  }

  updateRamp();   
  handleHoming(); 

  // Force Mode Beveiliging: Vloeiende stop bij overgewicht
  if (forceMode && state == MOVING) {
    if (getWeightKG() >= HARD_LIMIT_KG) {
      targetSpeed = 50; // Decelereer onmiddellijk naar minimum snelheid
      currentExitSpeed = 0;
      if (currentSpeed <= 100) { state = IDLE; head = tail; dominantStepsRemaining = 0; }
      sendStatusMessage(F("FORCE ALARM"), F("Gewichtslimiet bereikt! Machine gestopt."));
    }
  }
}
