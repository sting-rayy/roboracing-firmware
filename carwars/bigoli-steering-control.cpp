#include "mbed.h"
#include "rtos.h"
#include "algorithm"
#include "stdlib.h"

PwmOut driverPin(p23);
Serial serial(USBTX, USBRX);
DigitalOut led(LED1);
AnalogIn pot(p15);
Timer timer;

const int mainLoopMillis = 25;
const int timeoutMillis = 500;

const int pwmPeriodMs = 16;
const int pwmPulseMinUs = 1250;
const int pwmPulseMaxUs = 1750;
const int pwmPulseRangeUs = pwmPulseMaxUs - pwmPulseMinUs;

const float potMin = 0.18;
const float potMax = 0.91;

float steerPID_P = 0.7;
float steerPID_I = 0; //0.05;
float steerPID_D = 2;
const int errorHistorySize = 20;
float errorHistory[errorHistorySize] = {0};
int errorHistoryIndex = 0;
float lastError = 0;
unsigned int lastPIDTime;
bool pidUpdated;

float desiredHeading = 0;


float clamp(float x, float minX, float maxX) {
    return min(max(x, minX), maxX);
}

float linearRemap(float x, float x1, float x2, float y1, float y2) {
    float proportion = (x - x1) / (x2 - x1);
    return y1 + proportion * (y2 - y1);
}

unsigned int timeMillis() {
    return (unsigned int)(timer.read() * 1000.0);
}

void steerPower(float x) {
    x = clamp(x, -0.83, 0.83); // constrain speed to safe levels (multiply by 12 for volts)
    x = linearRemap(x, -1, 1, 0, 1); // remap [-1,1] to [0,1] for pwm logic
    int width = (int)(pwmPulseMinUs + (pwmPulseRangeUs * x));
    driverPin.pulsewidth_us(width);
}

float getHeading() {
    float h = linearRemap(pot.read(), potMin, potMax, -1, 1);
    return clamp(h, -1, 1);
}

bool getMessage() {
    bool gotMessage = false;
    while (serial.readable()) {
        char first = serial.getc();
        if (first == '$') {
            serial.scanf("%f", &desiredHeading);
            gotMessage = true;
        }
        if (first == '#') {
            serial.scanf("%f,%f,%f", 
                &steerPID_P, &steerPID_I, &steerPID_D);
            pidUpdated = true;
        }
    }
    return gotMessage;
}

float getPIDCorrection() {
    float error = desiredHeading - getHeading();

    errorHistoryIndex = (errorHistoryIndex + 1) % errorHistorySize;
    errorHistory[errorHistoryIndex] = error;
    float errorTotal = 0;
    for(int i = 0; i < errorHistorySize; i++) {
        errorTotal += errorHistory[i];
    }

    float dError = error - lastError;
    lastError = error;

    long thisTime = timeMillis();
    float dt = thisTime - lastPIDTime;
    lastPIDTime = thisTime;

    float correction = steerPID_P * error 
                     + steerPID_I * errorTotal 
                     + steerPID_D * dError/dt;
    return clamp(correction, -1.0, 1.0);
}

int main() {
    timer.start();
    unsigned int lastUpdate = lastPIDTime = timeMillis();

    Thread::wait(timeoutMillis + 50);

    while(true) {
        unsigned int thisTime = timeMillis();
        
        pidUpdated = false;
        bool received = getMessage();
        if(received) {
            led = !led;
            lastUpdate = thisTime;
        }

        if(thisTime < lastUpdate + timeoutMillis && thisTime > timeoutMillis) {
            float output = getPIDCorrection();
            steerPower(output);
            if(received) {
                serial.printf("$pot = %.2f, ", pot.read());
                serial.printf("heading = %.2f, ", getHeading());
                serial.printf("target = %.2f, ", desiredHeading);
                serial.printf("drive = %.2f", output);
                if(pidUpdated) serial.printf(", PID values updated");
                serial.printf("\r\n");
            }
        } else {
            // timeout
            steerPower(0);
            if(received) {
                serial.printf("$[timeout] pot=%.2f", pot.read());
                if(pidUpdated) serial.printf(", PID values updated");
                serial.printf("\r\n");
            }
        }

        int msRemain = thisTime + mainLoopMillis - (int)timeMillis();
        if(msRemain > 0) {
            Thread::wait(msRemain);
        }
        // serial.printf("wait = %d\r\n", msRemain);
    }
}
