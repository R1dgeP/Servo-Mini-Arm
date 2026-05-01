#include <Servo.h>

// ============================
// PIN ASSIGNMENTS
// ============================
const int basePin     = 3;
const int shoulderPin = 2;
const int elbowPin    = 4;
const int gripperPin  = 5;

const int joy1XPin = A0;  // base left/right
const int joy1YPin = A1;  // shoulder up/down
const int joy2XPin = A2;  // gripper left/right
const int joy2YPin = A3;  // elbow up/down
const int btn1Pin  = 7;   // return to rest
const int btn2Pin  = 8;   // wave emote

// ============================
// SERVO OBJECTS
// ============================
Servo servoBase;
Servo servoShoulder;
Servo servoElbow;
Servo servoGripper;

// ============================
// CURRENT POSITIONS
// ============================
int currentBase     = 90;
int currentShoulder = 90;
int currentElbow    = 90;
int currentGripper  = 90;

// Last positions actually written to servos
// Only write when position changes — eliminates buzzing
int lastWrittenBase     = 90;
int lastWrittenShoulder = 90;
int lastWrittenElbow    = 90;
int lastWrittenGripper  = 90;

// ============================
// SERVO LIMITS
// Prevents servos from pushing past physical limits
// Adjust if any joint binds at extremes
// ============================
const int baseMin     = 0;
const int baseMax     = 180;
const int shoulderMin = 30;
const int shoulderMax = 150;
const int elbowMin    = 30;
const int elbowMax    = 150;
const int gripperMin  = 45;   // closed position
const int gripperMax  = 135;  // open position

// ============================
// REST POSITION
// Where arm returns to on button 1 press
// ============================
const int restBase     = 90;
const int restShoulder = 90;
const int restElbow    = 90;
const int restGripper  = 90;

// ============================
// DEADZONE
// Joystick must move this far from center before arm moves
// Wide deadzone prevents drift from analog noise
// Center value = 512, deadzone = ±150
// ============================
const int joyCenter   = 512;
const int deadzoneSize = 150;
const int deadzoneMin  = joyCenter - deadzoneSize;  // 362
const int deadzoneMax  = joyCenter + deadzoneSize;  // 662

// ============================
// MOVEMENT TIMING
// Controls how fast servos move
// Lower moveInterval = faster updates
// Higher speed value = more degrees per update
// ============================
unsigned long lastMoveTime = 0;
const unsigned long moveInterval = 20;  // milliseconds between updates

const int baseSpeed     = 2;  // degrees per update
const int shoulderSpeed = 1;
const int elbowSpeed    = 1;
const int gripperSpeed  = 2;

// ============================
// BUTTON STATE TRACKING
// Tracks previous button state so we only
// trigger once per press, not continuously
// ============================
bool lastBtn1State = HIGH;
bool lastBtn2State = HIGH;
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
const unsigned long debounceDelay = 250;

// Prevents joystick input during animations
bool animationPlaying = false;

// ============================
// READ JOYSTICK WITH AVERAGING
// Takes 4 samples and averages them
// Reduces electrical noise on analog pins
// ============================
int readJoystick(int pin) {
  long sum = 0;
  for (int i = 0; i < 4; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return sum / 4;
}

// ============================
// JOYSTICK TO DIRECTION
// Converts raw joystick value to movement intent
// Returns: +1 (move one way), -1 (move other way), 0 (no move)
// Inverted because joysticks are held upside down
// ============================
int joystickDir(int joyValue) {
  if (joyValue < deadzoneMin) return +1;
  if (joyValue > deadzoneMax) return -1;
  return 0;
}

// ============================
// SAFE SERVO WRITE
// Only sends signal to servo if position actually changed
// Eliminates constant PWM writes that cause buzzing
// ============================
void safeWrite(Servo &servo, int current, int &lastWritten) {
  if (current != lastWritten) {
    servo.write(current);
    lastWritten = current;
  }
}

// ============================
// SMOOTH SINGLE SERVO MOVE
// Used inside animations
// Moves one degree at a time with a delay between steps
// ============================
void smoothMove(Servo &servo, int &current, int &lastWritten, int target, int stepDelay) {
  while (current != target) {
    if (current < target) current++;
    else current--;
    safeWrite(servo, current, lastWritten);
    delay(stepDelay);
  }
}

// ============================
// SETUP
// Runs once on power up
// ============================
void setup() {
  Serial.begin(9600);
  
  // INPUT_PULLUP means:
  // unpressed button = HIGH (internal resistor pulls pin up)
  // pressed button = LOW (button connects pin to ground)
  pinMode(btn1Pin, INPUT_PULLUP);
  pinMode(btn2Pin, INPUT_PULLUP);
  
  // Attach and home servos one at a time
  // Prevents all 4 drawing peak current simultaneously (brownout)
  Serial.println("Homing servos...");
  
  servoBase.attach(basePin);
  servoBase.write(restBase);
  Serial.println("Base homed");
  delay(800);
  
  servoShoulder.attach(shoulderPin);
  servoShoulder.write(restShoulder);
  Serial.println("Shoulder homed");
  delay(800);
  
  servoElbow.attach(elbowPin);
  servoElbow.write(restElbow);
  Serial.println("Elbow homed");
  delay(800);
  
  servoGripper.attach(gripperPin);
  servoGripper.write(restGripper);
  Serial.println("Gripper homed");
  delay(1000);
  
  Serial.println();
  Serial.println("=== Robot Arm Ready ===");
  Serial.println("J1 LR = Base rotation");
  Serial.println("J1 UD = Shoulder");
  Serial.println("J2 LR = Gripper");
  Serial.println("J2 UD = Elbow");
  Serial.println("Btn1  = Return to rest");
  Serial.println("Btn2  = Wave emote");
  Serial.println("=======================");
}

// ============================
// MAIN LOOP
// Runs continuously after setup
// ============================
void loop() {
  // Skip joystick control if animation is running
  if (animationPlaying) return;
  
  // Read all four joystick axes with noise averaging
  int joy1X = readJoystick(joy1XPin);  // base
  int joy1Y = readJoystick(joy1YPin);  // shoulder
  int joy2X = readJoystick(joy2XPin);  // gripper
  int joy2Y = readJoystick(joy2YPin);  // elbow
  
  // Convert readings to directional intent
  // +1 = move toward max limit
  // -1 = move toward min limit
  //  0 = joystick in deadzone, no movement
  int baseDir     = joystickDir(joy1X);
  int shoulderDir = joystickDir(joy1Y);
  int gripperDir  = joystickDir(joy2X);
  int elbowDir    = joystickDir(joy2Y);
  
  // Only update servo positions at fixed time intervals
  // This makes movement speed consistent regardless of loop speed
  unsigned long now = millis();
  if (now - lastMoveTime >= moveInterval) {
    lastMoveTime = now;
    
    // Move each joint in its commanded direction
    // constrain() keeps values within safe limits
    if (baseDir != 0)
      currentBase = constrain(currentBase + (baseDir * baseSpeed), baseMin, baseMax);
    
    if (shoulderDir != 0)
      currentShoulder = constrain(currentShoulder + (shoulderDir * shoulderSpeed), shoulderMin, shoulderMax);
    
    if (elbowDir != 0)
      currentElbow = constrain(currentElbow + (elbowDir * elbowSpeed), elbowMin, elbowMax);
    
    if (gripperDir != 0)
      currentGripper = constrain(currentGripper + (gripperDir * gripperSpeed), gripperMin, gripperMax);
    
    // Write new positions to servos
    // safeWrite only sends signal if position actually changed
    safeWrite(servoBase,     currentBase,     lastWrittenBase);
    safeWrite(servoShoulder, currentShoulder, lastWrittenShoulder);
    safeWrite(servoElbow,    currentElbow,    lastWrittenElbow);
    safeWrite(servoGripper,  currentGripper,  lastWrittenGripper);
  }
  
  // ============================
  // BUTTON HANDLING
  // Check for button presses
  // Using edge detection: only trigger on transition
  // from not-pressed (HIGH) to pressed (LOW)
  // ============================
  bool btn1 = digitalRead(btn1Pin);
  bool btn2 = digitalRead(btn2Pin);
  
  // Button 1: return to rest
  if (btn1 == LOW && lastBtn1State == HIGH) {
    if (millis() - lastBtn1Press > debounceDelay) {
      lastBtn1Press = millis();
      Serial.println("Returning to rest...");
      returnToRest();
    }
  }
  lastBtn1State = btn1;
  
  // Button 2: play wave emote
  if (btn2 == LOW && lastBtn2State == HIGH) {
    if (millis() - lastBtn2Press > debounceDelay) {
      lastBtn2Press = millis();
      Serial.println("Playing wave emote...");
      playEmote();
    }
  }
  lastBtn2State = btn2;
}

// ============================
// RETURN TO REST
// Moves all joints simultaneously back to rest position
// Runs until all joints reach their target
// ============================
void returnToRest() {
  animationPlaying = true;
  
  bool moving = true;
  while (moving) {
    moving = false;
    
    if (currentBase != restBase) {
      currentBase += (currentBase < restBase) ? 1 : -1;
      safeWrite(servoBase, currentBase, lastWrittenBase);
      moving = true;
    }
    if (currentShoulder != restShoulder) {
      currentShoulder += (currentShoulder < restShoulder) ? 1 : -1;
      safeWrite(servoShoulder, currentShoulder, lastWrittenShoulder);
      moving = true;
    }
    if (currentElbow != restElbow) {
      currentElbow += (currentElbow < restElbow) ? 1 : -1;
      safeWrite(servoElbow, currentElbow, lastWrittenElbow);
      moving = true;
    }
    if (currentGripper != restGripper) {
      currentGripper += (currentGripper < restGripper) ? 1 : -1;
      safeWrite(servoGripper, currentGripper, lastWrittenGripper);
      moving = true;
    }
    
    delay(18);
  }
  
  Serial.println("At rest.");
  animationPlaying = false;
}

// ============================
// WAVE EMOTE
// Sequence:
// 1. Raise shoulder and bend elbow up
// 2. Open gripper
// 3. Wave base left/right 3 times with gripper snapping open/closed
// 4. Center base
// 5. Elbow pump twice
// 6. Close gripper
// 7. Return to rest
// ============================
void playEmote() {
  animationPlaying = true;
  
  // Phase 1: raise arm up
  Serial.println("Emote phase 1: raising arm");
  smoothMove(servoShoulder, currentShoulder, lastWrittenShoulder, 130, 18);
  smoothMove(servoElbow,    currentElbow,    lastWrittenElbow,    60,  18);
  delay(300);
  
  // Phase 2: open gripper
  Serial.println("Emote phase 2: opening gripper");
  smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMax, 15);
  delay(200);
  
  // Phase 3: wave left/right 3 times
  // Gripper snaps closed on left swing, open on right swing
  Serial.println("Emote phase 3: waving");
  for (int i = 0; i < 3; i++) {
    smoothMove(servoBase,    currentBase,    lastWrittenBase,    60,         10);
    smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMin, 10);
    delay(80);
    smoothMove(servoBase,    currentBase,    lastWrittenBase,    120,        10);
    smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMax, 10);
    delay(80);
  }
  
  // Phase 4: center base
  smoothMove(servoBase, currentBase, lastWrittenBase, 90, 15);
  delay(200);
  
  // Phase 5: elbow pump twice
  Serial.println("Emote phase 5: elbow pump");
  for (int i = 0; i < 2; i++) {
    smoothMove(servoElbow, currentElbow, lastWrittenElbow, 120, 10);
    delay(80);
    smoothMove(servoElbow, currentElbow, lastWrittenElbow, 60,  10);
    delay(80);
  }
  
  // Phase 6: close gripper
  smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMin, 15);
  delay(400);
  
  // Phase 7: return to rest
  Serial.println("Emote phase 7: returning to rest");
  animationPlaying = false;
  returnToRest();
  
  Serial.println("Emote complete.");
}
