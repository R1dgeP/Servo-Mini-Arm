#include <Servo.h>

// ============================
// PIN ASSIGNMENTS
// ============================
const int basePin     = 3;
const int shoulderPin = 2;
const int elbowPin    = 4;
const int gripperPin  = 5;

const int joy1XPin = A0;
const int joy1YPin = A1;
const int joy2XPin = A2;
const int joy2YPin = A3;
const int btn1Pin  = 7;
const int btn2Pin  = 8;

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

int lastWrittenBase     = -1;
int lastWrittenShoulder = -1;
int lastWrittenElbow    = -1;
int lastWrittenGripper  = -1;

// ============================
// SERVO LIMITS
// ============================
const int baseMin     = 0;
const int baseMax     = 180;
const int shoulderMin = 30;
const int shoulderMax = 150;
const int elbowMin    = 30;
const int elbowMax    = 150;
const int gripperMin  = 45;
const int gripperMax  = 135;

// ============================
// REST POSITION
// Base rest is now adjustable in case servo's true center isn't 90
// Tune this if the arm doesn't sit straight at "rest"
// ============================
const int restBase     = 90;
const int restShoulder = 90;
const int restElbow    = 90;
const int restGripper  = 90;

// ============================
// CALIBRATED JOYSTICK CENTERS
// These get measured at startup instead of assumed at 512
// Compensates for joysticks that don't rest exactly at 512
// ============================
int joy1XCenter = 512;
int joy1YCenter = 512;
int joy2XCenter = 512;
int joy2YCenter = 512;

// Wide deadzone — applied as offset from calibrated center
const int deadzoneSize = 180;

// ============================
// MOVEMENT TIMING
// ============================
unsigned long lastMoveTime = 0;
const unsigned long moveInterval = 25;

const int baseSpeed     = 1;  // base intentionally slow for stability
const int shoulderSpeed = 1;
const int elbowSpeed    = 1;
const int gripperSpeed  = 2;

// ============================
// BUTTON STATE
// ============================
bool lastBtn1State = HIGH;
bool lastBtn2State = HIGH;
unsigned long lastBtn1Press = 0;
unsigned long lastBtn2Press = 0;
const unsigned long debounceDelay = 250;

bool animationPlaying = false;

// ============================
// CALIBRATE JOYSTICKS
// Reads each joystick 30 times at startup (assumes hands off)
// Averages the readings to find true resting position
// This eliminates "false center" jitter
// ============================
void calibrateJoysticks() {
  Serial.println("Calibrating joysticks - DO NOT TOUCH");
  delay(500);
  
  long sum1X = 0, sum1Y = 0, sum2X = 0, sum2Y = 0;
  
  for (int i = 0; i < 30; i++) {
    sum1X += analogRead(joy1XPin);
    sum1Y += analogRead(joy1YPin);
    sum2X += analogRead(joy2XPin);
    sum2Y += analogRead(joy2YPin);
    delay(10);
  }
  
  joy1XCenter = sum1X / 30;
  joy1YCenter = sum1Y / 30;
  joy2XCenter = sum2X / 30;
  joy2YCenter = sum2Y / 30;
  
  Serial.print("J1X center: "); Serial.println(joy1XCenter);
  Serial.print("J1Y center: "); Serial.println(joy1YCenter);
  Serial.print("J2X center: "); Serial.println(joy2XCenter);
  Serial.print("J2Y center: "); Serial.println(joy2YCenter);
}

// ============================
// READ JOYSTICK WITH AVERAGING
// 8 samples for extra noise rejection (was 4)
// ============================
int readJoystick(int pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return sum / 8;
}

// ============================
// JOYSTICK TO DIRECTION
// Now uses calibrated center instead of assumed 512
// Inverted (joystick held upside down)
// ============================
int joystickDir(int joyValue, int center) {
  if (joyValue < center - deadzoneSize) return +1;
  if (joyValue > center + deadzoneSize) return -1;
  return 0;
}

// ============================
// SAFE SERVO WRITE
// ============================
void safeWrite(Servo &servo, int current, int &lastWritten) {
  if (current != lastWritten) {
    servo.write(current);
    lastWritten = current;
  }
}

// ============================
// SMOOTH MOVE (used in animations)
// ============================
void smoothMove(Servo &servo, int &current, int &lastWritten, int target, int stepDelay) {
  while (current != target) {
    if (current < target) current++;
    else current--;
    safeWrite(servo, current, lastWritten);
    delay(stepDelay);
  }
}

void setup() {
  Serial.begin(9600);
  
  pinMode(btn1Pin, INPUT_PULLUP);
  pinMode(btn2Pin, INPUT_PULLUP);
  
  Serial.println("Homing servos...");
  
  servoBase.attach(basePin);
  servoBase.write(restBase);
  delay(800);
  
  servoShoulder.attach(shoulderPin);
  servoShoulder.write(restShoulder);
  delay(800);
  
  servoElbow.attach(elbowPin);
  servoElbow.write(restElbow);
  delay(800);
  
  servoGripper.attach(gripperPin);
  servoGripper.write(restGripper);
  delay(1000);
  
  // Calibrate joysticks AFTER servos are powered and stable
  // Hands must be off the joysticks during this!
  calibrateJoysticks();
  
  Serial.println();
  Serial.println("=== Robot Arm Ready ===");
}

void loop() {
  if (animationPlaying) return;
  
  // Read all joysticks with averaging
  int joy1X = readJoystick(joy1XPin);
  int joy1Y = readJoystick(joy1YPin);
  int joy2X = readJoystick(joy2XPin);
  int joy2Y = readJoystick(joy2YPin);
  
  // Get directional intent using calibrated centers
  int baseDir     = joystickDir(joy1X, joy1XCenter);
  int shoulderDir = joystickDir(joy1Y, joy1YCenter);
  int gripperDir  = joystickDir(joy2X, joy2XCenter);
  int elbowDir    = joystickDir(joy2Y, joy2YCenter);
  
  // Time-gated movement updates
  unsigned long now = millis();
  if (now - lastMoveTime >= moveInterval) {
    lastMoveTime = now;
    
    if (baseDir != 0)
      currentBase = constrain(currentBase + (baseDir * baseSpeed), baseMin, baseMax);
    
    if (shoulderDir != 0)
      currentShoulder = constrain(currentShoulder + (shoulderDir * shoulderSpeed), shoulderMin, shoulderMax);
    
    if (elbowDir != 0)
      currentElbow = constrain(currentElbow + (elbowDir * elbowSpeed), elbowMin, elbowMax);
    
    if (gripperDir != 0)
      currentGripper = constrain(currentGripper + (gripperDir * gripperSpeed), gripperMin, gripperMax);
    
    safeWrite(servoBase,     currentBase,     lastWrittenBase);
    safeWrite(servoShoulder, currentShoulder, lastWrittenShoulder);
    safeWrite(servoElbow,    currentElbow,    lastWrittenElbow);
    safeWrite(servoGripper,  currentGripper,  lastWrittenGripper);
  }
  
  // Buttons
  bool btn1 = digitalRead(btn1Pin);
  bool btn2 = digitalRead(btn2Pin);
  
  if (btn1 == LOW && lastBtn1State == HIGH) {
    if (millis() - lastBtn1Press > debounceDelay) {
      lastBtn1Press = millis();
      Serial.println("Returning to rest...");
      returnToRest();
    }
  }
  lastBtn1State = btn1;
  
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
    
    delay(20);
  }
  
  animationPlaying = false;
}

// ============================
// WAVE EMOTE
// ============================
void playEmote() {
  animationPlaying = true;
  
  smoothMove(servoShoulder, currentShoulder, lastWrittenShoulder, 130, 18);
  smoothMove(servoElbow,    currentElbow,    lastWrittenElbow,    60,  18);
  delay(300);
  
  smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMax, 15);
  delay(200);
  
  for (int i = 0; i < 3; i++) {
    smoothMove(servoBase,    currentBase,    lastWrittenBase,    60, 10);
    smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMin, 10);
    delay(80);
    smoothMove(servoBase,    currentBase,    lastWrittenBase,    120, 10);
    smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMax, 10);
    delay(80);
  }
  
  smoothMove(servoBase, currentBase, lastWrittenBase, 90, 15);
  delay(200);
  
  for (int i = 0; i < 2; i++) {
    smoothMove(servoElbow, currentElbow, lastWrittenElbow, 120, 10);
    delay(80);
    smoothMove(servoElbow, currentElbow, lastWrittenElbow, 60,  10);
    delay(80);
  }
  
  smoothMove(servoGripper, currentGripper, lastWrittenGripper, gripperMin, 15);
  delay(400);
  
  animationPlaying = false;
  returnToRest();
}
