#include <Servo.h>

//forward method declarations
void motor(int val);
void steer(int val);
void update();
void limitDesiredSpeed(float& desiredSpeed);
void limitDesiredHeading(float& desiredHeading);
void updateHeading();
void updateSpeed();
void encoderCallback();
bool getMessage();

//tracking distance traveled
const int encoderA = 2;
const int encoderB = 7;
static volatile int currentTicks = 0; //volatile data for manipulation in interrupt routines
static int lastTicks = 0;
static const float ticks_per_rotation = 200;
static const float meters_per_rotation = 0.1107 * 3.14; 
static const float gear_ratio = 0.33;

//control limits
static const int maxSpeed = 30; // maximum velocity 
static const int minSpeed = -15;
static const int maxHeading = 1;
static const int minHeading = -1;

//PID Constants
static float   pid_p = 1;
static float   pid_i = 0.0;
static float   pid_d = 0.1;

//Setpoint and error tracking
static float currentSpeed = 0;
static float currentHeading = 0;
static float desiredHeading = 0;
static float desiredSpeed = 0;

//variables used for "dt" parts of PID control
#define HISTORY_SIZE 100
static float         errorSum = 0;
static float         errorHistory[HISTORY_SIZE] = {0};
static unsigned long lastSpeedUpdateMicros = 0;
static int           historyIndex = 0;

//stop conditions for the car
static int lastMessageTime;
static bool timeout = false; 

const int estopPin = 10;
bool estop = false;

//autonomous or human control
const int muxStatePin = A5;
bool muxState = false;

//Motor objects
Servo esc;
const int escPin = 9;
Servo steering;
const int steerPin = 3;

int led_state = HIGH;

void setup()
{
    pinMode(estop, INPUT);

    pinMode(escPin, OUTPUT);
    esc.attach(escPin);
    pinMode(steerPin, OUTPUT);
    steering.attach(steerPin);

    pinMode(encoderA, INPUT);
    attachInterrupt(digitalPinToInterrupt(encoderA), encoderCallback, CHANGE);
    pinMode(encoderB, INPUT);

    pinMode(muxState, INPUT);

    pinMode(13, OUTPUT);
      
    motor(0);
    steer(0);

    Serial.begin(115200);
    lastMessageTime = millis();

}

void loop()
{   
  digitalWrite(13, led_state = !led_state);
  muxState = digitalRead(muxStatePin);
  estop = digitalRead(estopPin);
  if (estop) {
    motor(0);
    steer(0);
  } 
  //if we haven't received a message from the joule in a while, stop driving
  if (lastMessageTime + 500 < millis()) {
    timeout = true;
  }
  update();
  delay(50);
}

void update()
{
  if(getMessage()) {
      limitDesiredSpeed(desiredSpeed);
      limitDesiredHeading(desiredHeading); 
  }
  updateHeading();
  updateSpeed();
}

void limitDesiredSpeed(float& desiredSpeed) {
  desiredSpeed = min(maxSpeed, max(desiredSpeed, minSpeed));
}

void limitDesiredHeading(float& desiredHeading) {
  desiredHeading = min(maxHeading, max(desiredHeading, minHeading));    
}

void updateHeading()
{
  if (!muxState || estop) {
    steer(0);  
    desiredHeading = 0;
    currentHeading = 0;
  }
  else if (currentHeading != desiredHeading) {
    currentHeading = desiredHeading;
    steer(currentHeading); 
  }    
}

void updateSpeed()
{
  if (muxState || estop || timeout) {
    motor(0);  
    errorSum = 0;
    errorHistory[HISTORY_SIZE] = {0};
    desiredSpeed = 0;
    currentSpeed = 0;
  } else {
	  float deltaMeters = ((float)(currentTicks - lastTicks) / ticks_per_rotation) * meters_per_rotation * gear_ratio;
	  float deltaSeconds = (float)(micros() - lastSpeedUpdateMicros) / 1000000;
    currentSpeed = deltaMeters / deltaSeconds;
	  float currentError = desiredSpeed - (deltaMeters / deltaSeconds);
	  
	  // find derivative of error
	  float derivError = (float)(currentError - errorHistory[historyIndex]) 
				    / (micros() - lastSpeedUpdateMicros);

  	// update integral error
  	// TODO test assumption of even-enough spacing of the measurements thru time
  	// TODO test assumption that floating point inaccuracies won't add up too badly
  	historyIndex = (historyIndex+1) % HISTORY_SIZE;
  	errorSum -= errorHistory[historyIndex];
  	errorSum += currentError;
	  float integralError = errorSum / HISTORY_SIZE;
  	  
  	// combine PID terms
  	float targetMotorPwm = pid_p * currentError + pid_i * integralError + pid_d * derivError;
	  
  	// store previous state info
  	lastSpeedUpdateMicros = micros();
  	errorHistory[historyIndex] = currentError;
  	lastTicks = currentTicks;
	  
	  // update acutal PWM value. Slows down change rate to ESC to avoid errors
	  motor(targetMotorPwm);
   }
}

void steer(int val)
{
  steering.write(val + 90);
}

void motor(int val)
{
  esc.write(val + 90);
}

void encoderCallback()
{
  if (digitalRead(encoderA) == digitalRead(encoderB)) {
    currentTicks++;
  } else {
    currentTicks--;
  }
}

bool getMessage()
{
  bool gotMessage = false;
  while(Serial.available())
  {
    if(Serial.read() == '$')
    {
      timeout = false;
      lastMessageTime = millis();
      desiredSpeed = Serial.parseFloat();
      desiredHeading = Serial.parseFloat();
      pid_p = Serial.parseFloat();
      pid_i = Serial.parseFloat();
      pid_d = Serial.parseFloat();
      gotMessage = true;      
      String message = "$";
      message.concat(currentSpeed);
      message.concat(",");
      message.concat(muxState);
      message.concat(",");
      message.concat(estop);
      Serial.println(message);
      Serial.println(currentTicks);
    } else {
      Serial.println(Serial.read() + "  no message\n");
    }
  }
  return gotMessage;
}
