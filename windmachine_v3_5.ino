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

////////////////////////////////////////////////////////////////////////////////
// 📊 STATUS VARIABELEN
////////////////////////////////////////////////////////////////////////////////
enum MotionState { IDLE, MOVING };
enum HomeState { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK };

volatile MotionState state = IDLE;
volatile HomeState homeState = HOME_IDLE;
volatile int homingAxis = -1; 

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
unsigned long homingWaitStart = 0;
bool homingWaiting = false;

char serialBuffer[128];
int serialIdx = 0;

// Step/Dir Pin Masks (Direct Port Manipulation)
// X (Axis 0): Step 4 -> PD4, Dir 5 -> PD5
// Y (Axis 1): Step 6 -> PD6, Dir 7 -> PD7
// Z (Axis 2): Step 8 -> PB0, Dir 9 -> PB1
// A (Axis 3): Step 2 -> PD2, Dir 3 -> PD3
#define STEP_X_BIT (1 << 4) // PD4
#define DIR_X_BIT  (1 << 5) // PD5
#define STEP_Y_BIT (1 << 6) // PD6
#define DIR_Y_BIT  (1 << 7) // PD7
#define STEP_Z_BIT (1 << 0) // PB0
#define DIR_Z_BIT  (1 << 1) // PB1
#define STEP_A_BIT (1 << 2) // PD2
#define DIR_A_BIT  (1 << 3) // PD3

// Limit Pin Masks (Port C)
// X (Axis 0): A3 -> PC3
// Y (Axis 1): A4 -> PC4
// Z (Axis 2): A5 -> PC5
// A (Axis 3): A2 -> PC2
#define LIMIT_X_BIT (1 << 3)
#define LIMIT_Y_BIT (1 << 4)
#define LIMIT_Z_BIT (1 << 5)
#define LIMIT_A_BIT (1 << 2)

////////////////////////////////////////////////////////////////////////////////
// ⚡ LIMIT SWITCH CHECK
////////////////////////////////////////////////////////////////////////////////
inline bool limitTriggered(int axisIndex) {
  uint8_t mask;
  switch (axisIndex) {
    case 0: mask = LIMIT_X_BIT; break;
    case 1: mask = LIMIT_Y_BIT; break;
    case 2: mask = LIMIT_Z_BIT; break;
    case 3: mask = LIMIT_A_BIT; break;
    default: return false;
  }

  // Software debounce: sample multiple times
  int samples = 0;
  for (int i = 0; i < 5; i++) {
    if (!(PINC & mask)) samples++;
  }
  bool raw = (samples >= 3);
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

      if (i == 0) { // X
        if (dir) PORTD |= DIR_X_BIT; else PORTD &= ~DIR_X_BIT;
      } else if (i == 1) { // Y
        if (dir) PORTD |= DIR_Y_BIT; else PORTD &= ~DIR_Y_BIT;
      } else if (i == 2) { // Z
        if (dir) PORTB |= DIR_Z_BIT; else PORTB &= ~DIR_Z_BIT;
      } else if (i == 3) { // A
        if (dir) PORTD |= DIR_A_BIT; else PORTD &= ~DIR_A_BIT;
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
  Serial.println(F("--- STARTING HOMING SEQUENCE (Z, Y, X, A) ---"));
  startHomingAxis(2); // Z eerst
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
    homeState = HOME_BACKOFF;
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
    homingWaiting = false;
    return;
  }

  if (homingWaiting) {
    if (millis() - homingWaitStart >= 200) {
      homingWaiting = false;
      // After wait, we might need to trigger the next action
      if (homeState == HOME_SLOW_SEEK && homed[homingAxis]) {
         int next = -1;
         if (homingAxis == 2) next = 1;      // Na Z komt Y
         else if (homingAxis == 1) next = 0; // Na Y komt X
         else if (homingAxis == 0) next = 3; // Na X komt A

         startHomingAxis(next);
         return;
      }
    } else {
      return;
    }
  }

  bool hit = limitTriggered(homingAxis);

  switch (homeState) {
    case HOME_FAST_SEEK:
      if (hit) {
        state = IDLE;
        homingWaiting = true;
        homingWaitStart = millis();
        homeState = HOME_BACKOFF;
        
        long backoffAmt = axes[homingAxis].homeDirNegative ? homeBackoffSteps : -homeBackoffSteps;
        startMove(
          (homingAxis == 0 ? backoffAmt : 0),
          (homingAxis == 1 ? backoffAmt : 0),
          (homingAxis == 2 ? backoffAmt : 0),
          (homingAxis == 3 ? backoffAmt : 0),
          homeSlowSpeed
        );
        Serial.println(F(" -> Hit, backing off..."));
      }
      break;

    case HOME_BACKOFF:
      // Controleer of we de sensor al hebben vrijgemaakt
      if (!limitTriggered(homingAxis)) {
        // Sensor is vrij: stop de beweging, wacht kort en start de langzame seek
        state = IDLE;
        homingWaiting = true;
        homingWaitStart = millis();
        homeState = HOME_SLOW_SEEK;
        
        // Bepaal de langzame zoekrichting
        long slowSeek = axes[homingAxis].homeDirNegative ? -100000 : 100000;
        startMove(
          (homingAxis == 0 ? slowSeek : 0),
          (homingAxis == 1 ? slowSeek : 0),
          (homingAxis == 2 ? slowSeek : 0),
          (homingAxis == 3 ? slowSeek : 0),
          homeSlowSpeed
        );
        Serial.println(F(" -> Sensor vrij, seeking slow..."));
      } else {
        // De sensor is nog steeds ingedrukt. 
        // We sturen een geforceerde, kleine beweging de andere kant op om de sensor fysiek te verlaten.
        if (state == IDLE) {
          // Bepaal de veilige terugrij-richting
          long backoffStep = axes[homingAxis].homeDirNegative ? 300 : -300;
          
          startMove(
            (homingAxis == 0 ? backoffStep : 0),
            (homingAxis == 1 ? backoffStep : 0),
            (homingAxis == 2 ? backoffStep : 0),
            (homingAxis == 3 ? backoffStep : 0),
            homeSlowSpeed
          );
          Serial.println(F(" -> Sensor ingedrukt, rij nu actief terug..."));
        }
      }
      break;

    case HOME_SLOW_SEEK:
      if (hit) {
        state = IDLE;
        currentPosition[homingAxis] = 0; 
        homed[homingAxis] = true;

        Serial.print(F("AXIS ")); Serial.print(homingAxis); Serial.println(F(" HOMED OK."));

        homingWaiting = true;
        homingWaitStart = millis();
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
void sendStatusMessage(const __FlashStringHelper* title, const __FlashStringHelper* message) {
  Serial.println(F("\n--------------------------------------------------"));
  Serial.print(F(" >> ")); Serial.println(title);
  if (message != (const __FlashStringHelper*)NULL) {
    Serial.print(F("    ")); Serial.println(message);
  }
  Serial.println(F("--------------------------------------------------"));
}

float getAxisValue(const char* line, char axisLetter, float defaultVal, bool &found) {
  const char* ptr = strchr(line, axisLetter);
  if (ptr != NULL) {
    found = true;
    return strtof(ptr + 1, NULL);
  }
  found = false;
  return defaultVal;
}

void processCommand(char* line) {
  // Trim trailing whitespace
  int len = strlen(line);
  while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\r' || line[len-1] == '\n')) {
    line[--len] = '\0';
  }
  // Skip leading whitespace
  while (*line == ' ') line++;
  if (*line == '\0') return;

  // Convert to uppercase
  for (int i = 0; line[i]; i++) line[i] = toupper(line[i]);

  if (strcmp(line, "G28") == 0 || strcmp(line, "HOME") == 0) {
    startHomingSequence();
    return;
  }

  if (strcmp(line, "G90") == 0) {
    absoluteMode = true; 
    sendStatusMessage(F("MODUS: ABSOLUUT (G90)"), F("Posities berekend vanaf machine-nulpunt."));
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(line, "G91") == 0) {
    absoluteMode = false; 
    sendStatusMessage(F("MODUS: RELATIEF (G91)"), F("Posities berekend vanaf huidige locatie."));
    Serial.println(F("ok"));
    return; 
  }

  if (strcmp(line, "M114") == 0) {
    Serial.println(F("\n--- ACTUELE MACHINE POSITIE ---"));
    char labels[] = {'X', 'Y', 'Z', 'A'};
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
    char axisChars[NUM_AXES] = {'X', 'Y', 'Z', 'A'};
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

        if (i != 3) {
          if (newPosUnit < -0.001 || newPosUnit > axes[i].maxTravelMM + 0.001) {
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
    PORTD &= ~(STEP_X_BIT | STEP_Y_BIT | STEP_A_BIT);
    PORTB &= ~STEP_Z_BIT;

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
    if (doStep[0]) PORTD |= STEP_X_BIT;
    if (doStep[1]) PORTD |= STEP_Y_BIT;
    if (doStep[2]) PORTB |= STEP_Z_BIT;
    if (doStep[3]) PORTD |= STEP_A_BIT;
  } else {
    // Step LOW + Update position
    if (doStep[0]) { PORTD &= ~STEP_X_BIT; currentPosition[0] += moveDir[0]; }
    if (doStep[1]) { PORTD &= ~STEP_Y_BIT; currentPosition[1] += moveDir[1]; }
    if (doStep[2]) { PORTB &= ~STEP_Z_BIT; currentPosition[2] += moveDir[2]; }
    if (doStep[3]) { PORTD &= ~STEP_A_BIT; currentPosition[3] += moveDir[3]; }

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
    } else if (serialIdx < sizeof(serialBuffer) - 1) {
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