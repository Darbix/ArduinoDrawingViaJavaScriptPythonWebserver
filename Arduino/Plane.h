#ifndef PLANE_H
#define PLANE_H

#include "StepperMotor.h"
#include <Servo.h>

#define DIR_X -1                    ///< Plane X increases when X motor goes counterclockwise (stepsX--)
#define DIR_Y 1                     ///< PLane Y increases when Y motor goes clockwise (stepsY++)

#define SERVO_DOWN 0                ///< Servo position when the pen is down
#define SERVO_UP 24                 ///< SErvo position when it is up

// 6.4 x 6.4 cm plane dimensions for specific motor circumferences
#define MAX_STEPS_X 1010            ///< Measured steps per whole plane in the X direction
#define MAX_STEPS_Y 680             ///< Measured steps per whole plane in the Y direction

#define ACCELER_STEPS_PERC 0.066    ///< Fraction expressing how many steps to move by will be accelerated (slower delays)

#define MAX(x, y) (((x) > (y)) ? (x) : (y)) ///< Math MAX function
#define MIN(x, y) (((x) < (y)) ? (x) : (y)) ///< Math MIN function

typedef enum{                   ///< Move directions
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
}directionType;

/**
 * 2D plane drawing controller using 2 steppers and 1 servo 
 */
class Plane{
    private:
        long int stepsX;        ///< Current X steps from the home
        long int stepsY;        ///< Current Y steps from the home

        StepperMotor *motorX;   ///< Axis X stepper controller pointer
        StepperMotor *motorY;   ///< Axis Y stepper controller pointer

        Servo *penServo;        ///< Axis Z servo pointer
        int pinServoEnable;     ///< Pin to enable a servo power supply
    
    public:
        /**
         * Construct a new Plane object
         * @param motorX Pointer to the stepper X object
         * @param motorY Pointer to the stepper Y object
         * @param penServo Pointer to the servo object
         * @param pinServoEnable Pin number to enable servo power
         */
        Plane(StepperMotor *motorX, StepperMotor *motorY, Servo *penServo, int pinServoEnable);

        /**
         * Reset the acceleration counter for the both steppers
         */
        void accelerateBoth();

        /**
         * Move to the position [0.0 - 1.0, 0.0 - 1.0]
         * @param fracX Double normalized fraction representing the X coord
         * @param fracY Double normalized fraction representing the Y coord
         */
        void moveTo(double fracX, double fracY);

        /**
         * Make one step in the given direction
         * @param direction 4 possible directions (2 for each exis)
         */
        void step(directionType direction);

        /**
         * Reset the current steps done to zero (sets a new home location)
         */
        void reset();

        /**
         * move the pen servo down
         */
        void penDown();

        /**
         * Move the pen servo up 
         */
        void penUp();
};

#endif