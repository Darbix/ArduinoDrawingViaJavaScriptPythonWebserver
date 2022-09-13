#include "StepperMotor.h"
#include <Arduino.h>

StepperMotor::StepperMotor(const int pins[4]){
    this->pins = pins;
    resetAcceleration();

    lastPhase = 0;
    lastMicros = 0;
    currentMicros = 0;
}

void StepperMotor::stepPhase(const unsigned val[4]){
    // Power digital 1/0 all 4 stepper pins according to a given sequence 
    for(int v = 0; v < 4; v++)
        digitalWrite(pins[v], val[v]);
}

void StepperMotor::resetAcceleration(){
    accelerCntr = ACCELER_PHASES;
}

int StepperMotor::stepperMove(bool clockwise, double delayMultiplier){
    // Whole step is done after 'stepPhases' iterations
    currentMicros = micros();

    // Pick a minimal delay to wait depending on accelerCntr
    unsigned long realDelay = accelerCntr == 0? DELAY_MICROS: INITIAL_ACCELER_DELAY;

    // If a delay from the last step part is >= requested minimal delay, do one phase
    if(currentMicros - lastMicros >= (unsigned long)(ceil(realDelay * delayMultiplier))){
        if(accelerCntr > 0)
            accelerCntr--;
        
        // If the movement is counter-clockwise, reverse the phaseMatrix order
        stepPhase(phaseMatrix[clockwise? lastPhase: stepPhases - 1 - lastPhase]);

        lastPhase++;
        lastMicros = micros();

        if(lastPhase >= stepPhases){
            lastPhase = 0;

            if(clockwise)
                return 1;   // Done one step clockwise
            else 
                return -1;  // Done one step counterclockwise
        }
        return 0;
    }
    else
        // Delay is not enough long, step is not done yet
        return 0;
}

