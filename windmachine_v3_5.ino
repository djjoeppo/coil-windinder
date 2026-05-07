////////////////////////////////////////////////////////////////////////////////
// 🧠 CNC 4-AXIS FIRMWARE (VERSION 2.2 - INFINITE A-AXIS)
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

// DIRECT PORT MANIPULATION MACROS (ATmega328P)
// X: 4,5 (PD4, PD5) | Y: 6,7 (PD6, PD7) | Z: 8,9 (PB0, PB1) | A: 2,3 (PD2, PD3)
#define SET_STEP_X(v) (v ? PORTD |= (1 << 4) : PORTD &= ~(1 << 4))
#define SET_DIR_X(v)  (v ? PORTD |= (1 << 5) : PORTD &= ~(1 << 5))
#define SET_STEP_Y(v) (v ? PORTD |= (1 << 6) : PORTD &= ~(1 << 6))
#define SET_DIR_Y(v)  (v ? PORTD |= (1 << 7) : PORTD &= ~(1 << 7))
#define SET_STEP_Z(v) (v ? PORTB |= (1 << 0) : PORTB &= ~(1 << 0))
#define SET_DIR_Z(v)  (v ? PORTB |= (1 << 1) : PORTB &= ~(1 << 1))
#define SET_STEP_A(v) (v ? PORTD |= (1 << 2) : PORTD &= ~(1 << 2))
#define SET_DIR_A(v)  (v ? PORTD |= (1 << 3) : PORTD &= ~(1 << 3))

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
  // ==========================================
  // --- X-AS (Index 0) ---
  // ==========================================
  axes[0].stepPin         = 4;
  axes[0].dirPin          = 5;
  axes[0].limitPin        = A3;
  axes[0].invertDir       = false;
  axes[0].homeDirNegative = true;
  axes[0].isNC            = false;
  axes[0].maxTravelMM     = 275.0;
  axes[0].stepsPerMM      = 266.7;

  // ==========================================
  // --- Y-AS (Index 1) ---
  // ==========================================
  axes[1].stepPin         = 6;
  axes[1].dirPin          = 7;
  axes[1].limitPin        = A4;
  axes[1].invertDir       = true;
  axes[1].homeDirNegative = true;
  axes[1].isNC            = false;
  axes[1].maxTravelMM     = 65.0;
  axes[1].stepsPerMM      = 266.7;

  // ==========================================
  // --- Z-AS (Index 2) ---
  // ==========================================
  axes[2].stepPin         = 8;
  axes[2].dirPin          = 9;
  axes[2].limitPin        = A5;
  axes[2].invertDir       = false;
  axes[2].homeDirNegative = true;
  axes[2].isNC            = true;
  axes[2].maxTravelMM     = 15.0;
  axes[2].stepsPerMM      = 400.0;

  // ==========================================
  // --- A-AS (Index 3) - Oneindige Draai-as ---
  // ==========================================
  axes[3].stepPin         = 2;
  axes[3].dirPin          = 3;
  axes[3].limitPin        = A2;
  axes[3].invertDir       = true;
  axes[3].homeDirNegative = false;
  axes[3].isNC            = true;
  axes[3].maxTravelMM     = -1.0; // Oneindige travel, limieten uitgeschakeld
  axes[3].stepsPerMM      = 3.5556; // Dit zijn nu de stappen per graad
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

char serialBuffer[64];
int serialIndex = 0;

////////////////////////////////////////////////////////////////////////////////
// 📊 STATUS VARIABELEN
////////////////////////////////////////////////////////////////////////////////
enum MotionState { IDLE, MOVING };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK, HOME_DELAY };

volatile MotionState state = IDLE;
volatile HomeState homeState = HOME_IDLE;
volatile int homingAxis = -1; 

// Homing delay variabelen (non-blocking)
unsigned long homeDelayTimer = 0;
unsigned long homeDelayDuration = 0;
HomeState postDelayState = HOME_IDLE;
int nextHomingAxis = -1;

volatile bool homingSequenceActive = false;

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
inline bool limitTriggered(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) return false;

  bool raw;
  // Directe register-uitlezing voor ATmega328P (PINC)
  // X=A3(PC3), Y=A4(PC4), Z=A5(PC5), A=A2(PC2)
  switch(axisIndex) {
    case 0: raw = !(PINC & (1 << 3)); break;
    case 1: raw = !(PINC & (1 << 4)); break;
    case 2: raw = !(PINC & (1 << 5)); break;
    case 3: raw = !(PINC & (1 << 2)); break;
    default: return false;
  }

  bool triggered = axes[axisIndex].isNC ? raw : !raw;
  if (!triggered) return false;

  // Anti-false trigger: bevestig na korte wachtperiode
  delayMicroseconds(20);
  switch(axisIndex) {
    case 0: raw = !(PINC & (1 << 3)); break;
    case 1: raw = !(PINC & (1 << 4)); break;
    case 2: raw = !(PINC & (1 << 5)); break;
    case 3: raw = !(PINC & (1 << 2)); break;
  }
  return axes[axisIndex].isNC ? raw : !raw;
}

void startMove(long stepsX, long stepsY, long stepsZ, long stepsA, float feed) {
  long steps[NUM_AXES] = {stepsX, stepsY, stepsZ, stepsA};

  // 🔴 Reset eerst alles (belangrijk voor stabiliteit)
  dominantStepsTotal = 0;
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = 0;
    err[i] = 0;
    doStep[i] = false;
    moveDir[i] = 0;
  }

  // 🔧 Bepaal step counts + dominante as
  for (int i = 0; i < NUM_AXES; i++) {
    stepCount[i] = labs(steps[i]);

    if (stepCount[i] > dominantStepsTotal) {
      dominantStepsTotal = stepCount[i];
    }

    if (steps[i] != 0) {
      bool dir = (steps[i] > 0);
      if (axes[i].invertDir) dir = !dir;

      switch(i) {
        case 0: SET_DIR_X(dir); break;
        case 1: SET_DIR_Y(dir); break;
        case 2: SET_DIR_Z(dir); break;
        case 3: SET_DIR_A(dir); break;
      }
      moveDir[i] = (steps[i] > 0) ? 1 : -1;
    }
  }

  // 🛑 Geen beweging? Stop hier netjes
  if (dominantStepsTotal == 0) {
    state = IDLE;
    return;
  }

  // 🔁 Bresenham initialisatie
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

  // 🎯 Speed setup (veilig begrensd)
  targetSpeed = constrain(feed, 50, maxSpeed);
  currentSpeed = 0;  // 🔥 belangrijk voor ramp restart

  lastRampMicros = micros();
  state = MOVING;
}

////////////////////////////////////////////////////////////////////////////////
// 🚀 HOMING SEQUENCE START
////////////////////////////////////////////////////////////////////////////////
void startHomingSequence() {
  head = 0;
  tail = 0;
  Serial.println("--- STARTING HOMING SEQUENCE (Z, Y, X, A) ---");
  startHomingAxis(2); // Z eerst
}

void startHomingAxis(int axisIndex) {
  if (axisIndex < 0 || axisIndex >= NUM_AXES) {
    homingAxis = -1; homeState = HOME_IDLE; state = IDLE;
    Serial.println(">>> ALL AXES HOMED SUCCESSFULLY.");
    return;
  }

  homingAxis = axisIndex;
  homingStart = millis();
  Serial.print("AXIS "); Serial.print(axisIndex);

  long backoffAmt = axes[axisIndex].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;

  if (limitTriggered(axisIndex)) {
    Serial.println(": ALREADY ON LIMIT -> STARTING BACKOFF");
    homeState = HOME_BACKOFF;
    startMove(
      (axisIndex == 0 ? backoffAmt : 0),
      (axisIndex == 1 ? backoffAmt : 0),
      (axisIndex == 2 ? backoffAmt : 0),
      (axisIndex == 3 ? backoffAmt : 0),
      homeSlowSpeed
    );
  } else {
    Serial.println(": FAST SEEK");
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

  // Afhandeling van non-blocking delays
  if (homeState == HOME_DELAY) {
    if (millis() - homeDelayTimer >= homeDelayDuration) {
      homeState = postDelayState;
      if (homeState == HOME_BACKOFF) {
        long backoffAmt = axes[homingAxis].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;
        startMove(
          (homingAxis == 0 ? backoffAmt : 0),
          (homingAxis == 1 ? backoffAmt : 0),
          (homingAxis == 2 ? backoffAmt : 0),
          (homingAxis == 3 ? backoffAmt : 0),
          homeSlowSpeed
        );
      } else if (homeState == HOME_SLOW_SEEK) {
        long slowSeek = axes[homingAxis].homeDirNegative ? -100000 : 100000;
        startMove(
          (homingAxis == 0 ? slowSeek : 0),
          (homingAxis == 1 ? slowSeek : 0),
          (homingAxis == 2 ? slowSeek : 0),
          (homingAxis == 3 ? slowSeek : 0),
          homeSlowSpeed
        );
      } else if (homeState == HOME_IDLE) {
        startHomingAxis(nextHomingAxis);
      }
    }
    return;
  }

  bool hit = limitTriggered(homingAxis);

  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) {
        state = IDLE;
        homeDelayTimer = millis();
        homeDelayDuration = 150;
        homeState = HOME_DELAY;
        postDelayState = HOME_BACKOFF;
        Serial.println(F(" -> Hit, waiting to backoff..."));
      }
      break;

    case HOME_BACKOFF:
      if (!hit) {
        state = IDLE;
        homeDelayTimer = millis();
        homeDelayDuration = 150;
        homeState = HOME_DELAY;
        postDelayState = HOME_SLOW_SEEK;
        Serial.println(F(" -> Sensor vrij, seeking slow..."));
      } else if (state == IDLE) {
        long backoffStep = axes[homingAxis].homeDirNegative ? 300 : -300;
        startMove(
          (homingAxis == 0 ? backoffStep : 0),
          (homingAxis == 1 ? backoffStep : 0),
          (homingAxis == 2 ? backoffStep : 0),
          (homingAxis == 3 ? backoffStep : 0),
          homeSlowSpeed
        );
        Serial.println(F(" -> Sensor nog ingedrukt, actieve back-off..."));
      }
      break;

    case HOME_SLOW_SEEK:
      if (hit) {
        state = IDLE;
        currentPosition[homingAxis] = 0; 
        homed[homingAxis] = true;
        Serial.print(F("AXIS ")); Serial.print(homingAxis); Serial.println(F(" HOMED OK."));

        int next = -1;
        if (homingAxis == 2) next = 1;
        else if (homingAxis == 1) next = 0;
        else if (homingAxis == 0) next = 3;

        nextHomingAxis = next;
        homeDelayTimer = millis();
        homeDelayDuration = 200;
        homeState = HOME_DELAY;
        postDelayState = HOME_IDLE;
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
  // Trim trailing whitespace
  int len = strlen(line);
  while (len > 0 && isspace(line[len - 1])) {
    line[--len] = '\0';
  }
  // Trim leading whitespace
  char* start = line;
  while (*start && isspace(*start)) start++;
  if (*start == '\0') return;

  // To uppercase
  for (char* p = start; *p; p++) *p = toupper(*p);

  if (strcmp(start, "G28") == 0 || strcmp(start, "HOME") == 0) {
    startHomingSequence();
    return;
  }

  if (strcmp(start, "G90") == 0) {
    absoluteMode = true; 
    sendStatusMessage("MODUS: ABSOLUUT (G90)", "Posities berekend vanaf machine-nulpunt.");
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(start, "G91") == 0) {
    absoluteMode = false; 
    sendStatusMessage("MODUS: RELATIEF (G91)", "Posities berekend vanaf huidige locatie.");
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(start, "M114") == 0) {
    Serial.println(F("\n--- ACTUELE MACHINE POSITIE ---"));
    const char labels[] = {'X', 'Y', 'Z', 'A'};
    for (int i = 0; i < NUM_AXES; i++) {
      float pos = (float)currentPosition[i] / axes[i].stepsPerMM;
      Serial.print(labels[i]); Serial.print(F(": "));
      Serial.print(pos); 
      if (i == 3) {
        Serial.print(F("°  "));
      } else {
        Serial.print(F("mm  "));
      }
    }
    Serial.println(F("\n-------------------------------"));
    Serial.println(F("ok"));
    return;
  }
 
  if (start[0] == 'G') {
    for(int i = 0; i < NUM_AXES; i++) {
        if (!homed[i]) { 
          Serial.println(F("ERROR: Systeem niet veilig. Voer eerst G28 uit!"));
          return;
        }
    }

    bool fFound;
    float feed = getAxisValue(start, 'F', targetSpeed, fFound);

    long stepsToMove[NUM_AXES] = {0, 0, 0, 0};
    bool axisIncluded[NUM_AXES] = {false, false, false, false};
    const char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'};
    bool hasMovement = false;

    for (int i = 0; i < NUM_AXES; i++) {
      float val = getAxisValue(start, axisChars[i], 0.0, axisIncluded[i]);
      if (axisIncluded[i]) {
        float currentPosUnit = (float)currentPosition[i] / axes[i].stepsPerMM;
        
        for (int b = tail; b != head; b = (b + 1) % BUFFER_SIZE) {
          currentPosUnit += (moveBuffer[b].steps[i] / axes[i].stepsPerMM);
        }

        float deltaUnit = absoluteMode ? (val - currentPosUnit) : val;
        float newPosUnit = currentPosUnit + deltaUnit;

        if (i != 3) {
          if (newPosUnit < 0.0 || newPosUnit > axes[i].maxTravelMM) {
            Serial.print(F("ALARM: Soft limit bereikt op ")); Serial.print(axisChars[i]);
            Serial.print(F(" (Max: ")); Serial.print(axes[i].maxTravelMM); Serial.println(F("mm)"));
            return;
          }
        }

        stepsToMove[i] = lround(deltaUnit * axes[i].stepsPerMM);
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
// ⏱️ TIMER ISR (Veilig gemaakt)
////////////////////////////////////////////////////////////////////////////////
ISR(TIMER1_COMPA_vect) {
  // AANPASSING HIER: '&& homeState != HOME_BACKOFF' is toegevoegd zodat de as mag bewegen tijdens de back-off
  if (homeState != HOME_IDLE && homingAxis >= 0 && limitTriggered(homingAxis) && homeState != HOME_BACKOFF) {
    // Schakel alle stappen uit (Direct Port Manipulation)
    SET_STEP_X(LOW);
    SET_STEP_Y(LOW);
    SET_STEP_Z(LOW);
    SET_STEP_A(LOW);
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
    if (doStep[0]) SET_STEP_X(HIGH);
    if (doStep[1]) SET_STEP_Y(HIGH);
    if (doStep[2]) SET_STEP_Z(HIGH);
    if (doStep[3]) SET_STEP_A(HIGH);
  } else {
    if (doStep[0]) { SET_STEP_X(LOW); currentPosition[0] += moveDir[0]; }
    if (doStep[1]) { SET_STEP_Y(LOW); currentPosition[1] += moveDir[1]; }
    if (doStep[2]) { SET_STEP_Z(LOW); currentPosition[2] += moveDir[2]; }
    if (doStep[3]) { SET_STEP_A(LOW); currentPosition[3] += moveDir[3]; }

    for (int i = 0; i < NUM_AXES; i++) {
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
  
  Serial.println("--- READY ---");
}

void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialIndex > 0) {
        serialBuffer[serialIndex] = '\0';
        processCommand(serialBuffer);
        serialIndex = 0;
      }
    } else if (serialIndex < (int)sizeof(serialBuffer) - 1) {
      serialBuffer[serialIndex++] = c;
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