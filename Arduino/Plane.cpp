#include "Plane.h"
#include <Arduino.h>

Plane::Plane(StepperMotor *motorX, StepperMotor *motorY, Servo *penServo, int pinServoEnable){
    this->motorX = motorX;
    this->motorY = motorY;
    this->penServo = penServo;
    this->pinServoEnable = pinServoEnable;
    penUp();

    stepsX = 0;
    stepsY = 0;
};

void Plane::accelerateBoth(){
    motorX->resetAcceleration();
    motorY->resetAcceleration();
}

void Plane::moveTo(double fracX, double fracY){
    // Calculate the steps to the target from the home location (0 to MAX_STEPS_X/Y) 
    long int destX = MAX_STEPS_X * fracX * DIR_X;
    long int destY = MAX_STEPS_Y * fracY * DIR_Y;
    // Calculate the difference of steps from the current location
    long int moveByX = destX - stepsX;
    long int moveByY = destY - stepsY;

    long int absMoveByX = abs(moveByX);
    long int absMoveByY = abs(moveByY);

    // How many steps of the axis with more MAX_STEPS are equal one step on the axis with less MAX_STEPS
    // E.g. if diff == 2.0, one of the axes moves 2 steps, while the second one just one at the same time
    // If the move is 'diagonal' (both axes move), it would draw wrong shapes, if it wasn't equalized 
    double diff = 1;
    if(moveByX != 0 && moveByY != 0)
        diff = 1.0 * MAX(absMoveByX, absMoveByY) / MIN(absMoveByX, absMoveByY);

    // If an acceleration is used, adapt it to a number of move-by-steps
    if(motorX->accelerCntr)
        motorX->accelerCntr = (unsigned)(absMoveByX * ACCELER_STEPS_PERC * motorX->stepPhases);
    if(motorY->accelerCntr)
        motorY->accelerCntr = (unsigned)(absMoveByY * ACCELER_STEPS_PERC * motorY->stepPhases);

    long unsigned phases = 0;
    double diffCounter = diff;

    while(stepsX != destX || stepsY != destY){
        // While at least one axis is not at the target position, alternately move axes
        // One stepper phase can wait for the minimal delay, while the second stepper can asynchronously move
        // The axis with less MAX_STEPS must have 'diff' times longer phase delay to move correctly
        
        if(stepsX != destX){
            // If X steps to do are less than Y and at least diffCounter Y steps were done  
            if(absMoveByX < absMoveByY && (double)phases >= diffCounter){
                stepsX += motorX->stepperMove(moveByX > 0, diff);
                diffCounter += diff;
            }
            // If X steps to do are more than Y or Y is done yet
            else if(absMoveByX >= absMoveByY || stepsY == destY)
                stepsX += motorX->stepperMove(moveByX > 0, 1);
        }
        if(stepsY != destY){
            // If Y steps to do are less than X and at least diffCounter X steps were done  
            if(absMoveByX >= absMoveByY && (double)phases >= diffCounter){
                stepsY += motorY->stepperMove(moveByY > 0, diff);
                diffCounter += diff;
            }   
            // If Y steps to do are more than X or X is done yet
            else if(absMoveByX < absMoveByY || stepsX == destX)
                stepsY += motorY->stepperMove(moveByY > 0, 1);
        }
        phases++;
    }
}

void Plane::step(directionType direction){
    int step = 0;
    bool clockwise;

    // Function used only to set a new home location (so only one stepper moves)
    if(direction == RIGHT || direction == LEFT){
        clockwise = direction == RIGHT? DIR_X > 0: DIR_X < 0;
        // While the step is not done yet (all it's phases), repeat caling the phase movement
        while((step = motorX->stepperMove(clockwise, 1)) == 0);
        stepsX += step;
    }
    else if(direction == UP || direction == DOWN){
        clockwise = direction == UP? DIR_Y > 0: DIR_Y < 0;
        while((step = motorY->stepperMove(clockwise, 1)) == 0);
        stepsY += step;
    }
}

void Plane::reset(){
    stepsX = 0;
    stepsY = 0;
}

void Plane::penDown(){
    digitalWrite(pinServoEnable, HIGH);
    penServo->write(SERVO_DOWN);
    delay(100);
    // Power turns off, so it doesn't waste energy and load steppers while drawing
    digitalWrite(pinServoEnable, LOW);
}

void Plane::penUp(){
    digitalWrite(pinServoEnable, HIGH);
    penServo->write(SERVO_UP);
    // Hold the pen up with power, or it may drop down
}
