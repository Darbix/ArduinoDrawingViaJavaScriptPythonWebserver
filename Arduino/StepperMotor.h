#ifndef STEPPERMOTOR_H
#define STEPPERMOTOR_H

/**
 * 60 / DELAY_MICROS * 10^-6 [s] * 512 steps-per-revolution * 8 phases:
 * RPM Examples
 * for DELAY_MICROS 830 = 17.6 rpm max [5V power bank]
 * for DELAY_MICROS 620 = 23.6 rpm max [9V power supply]
 * for DELAY_MICROS 365 = 40.1 rpm max [12V power supply]
 * 
 * Userd 28BYJ-48 w/ ULN2003 controller with a weak power source moves really slowly
 * REommended using more powerful stepper motor and a stable power supply to achieve a higher speed
 */
#define DELAY_MICROS 380            ///< 'Asynchronous' delay between step phases
/**
 * Following settings are used to boost the stepper motors to reach higher speeds
 * At the start the motor needs more power or longer phase delays. Otherwise it just vibrates
 * Motor than normally heats up to +-90°C, which can be solved with a relay controlling the supply
 */
#define INITIAL_ACCELER_DELAY 500   ///< Delay used when starting the motor
#define ACCELER_PHASES 300          ///< Motor starting number of phases that uses slower delays

/**
 * Stepper motor controller
 */
class StepperMotor{
    private:
        const int *pins;                        ///< Pointer to an array of 4 stepper motor pins
        unsigned lastPhase;                     //< Last step phase done in the previous cycle

        const unsigned phaseMatrix[8][4] = {    ///< Matrix of a fullstep phase order
            {1,0,0,0},
            {1,1,0,0},
            {0,1,0,0},
            {0,1,1,0},
            {0,0,1,0},
            {0,0,1,1},
            {0,0,0,1},
            {1,0,0,1},
        }   ;

        long unsigned lastMicros;       ///< Microseconds from the start to the last step phase
        long unsigned currentMicros;    ///< Microseconds from the start to the current phase
        
        /**
         * Write digital output to all stepper pins a single phase consists from
         * @param val Int array of 4 phase digital values
         */
        void stepPhase(const unsigned val[4]);

    public:
        const unsigned stepPhases = 8;  ///< Number of the step phases (cycles)
        unsigned accelerCntr;           ///< Initial phases counter for acceleration
        /**
         * @brief Construct a new Stepper Motor object
         * @param pins Array of 4 int pin numbers
         */
        StepperMotor(const int pins[4]);

        /**
         * Restart the acceleration counter
         */
        void resetAcceleration();

        /**
         * Moves the stepper 1 step by repeated calling of this function. Delay between the phases
         * is therefore non-blocking and the step is done when the return value is not equal 0
         * @param clockwise Bool if the movement is clockwise
         * @param delayMultiplier Constatnt float which multiplies the default phase delay
         * @return int 0 if the step is not done yet, otherwise 1 (clockwise) or -1 (counterc.w.) 
         */
        int stepperMove(bool clockwise, double delayMultiplier);
};


#endif