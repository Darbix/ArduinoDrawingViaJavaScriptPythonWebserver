#include <Servo.h>
#include "StepperMotor.h"
#include "Plane.h"

#define PIN_SERVO 34            ///< Pin for the pen servo
#define PIN_SERVO_ENABLE 46     ///< Pin for the power supply activation to save power and not buzz

#define PIN_LED 38              ///< Pin for the LED indicator
#define PIN_RELAY 42            ///< Pin for the relay coil (LOW state turns on the power)

#define BAUD_RATE 115200        ///< Serial communication baud rate also used by the sender
#define RECEIVE_BUFFER_SIZE 20  ///< Max bytes to be read from COM4 port as a single command

const int pinsStepperY[4] = {22,23,24,25};   ///< Direction Y stepper pins
const int pinsStepperX[4] = {28,29,30,31};   ///< Direction X stepper pins

typedef enum{   ///< Point type of a drawn line
    START,
    MID,
    END,
    ERR
}pointType;


// ----- Functions -----
/**
 * Confirm the received data to the sender and allow next to come
 */
void serialConfirm(){
    Serial.print("1\n");
    // Blink the LED
    digitalWrite(PIN_LED, HIGH);
    delay(50);
    digitalWrite(PIN_LED, LOW);
}

/**
 * Turn on the motor power supply relay
 * This technique keeps them tens of degrees colder
 * @return true since the power is on 
 */
bool powerOn(){
    digitalWrite(PIN_RELAY, LOW);
    delay(90);
    return true;
}

/**
 * Turn off the motor power supply relay 
 * @return false since the power is off 
 */
bool powerOff(){
    digitalWrite(PIN_RELAY, HIGH);
    delay(90);
    return false;
}

/**
 * Recognize the received instruction, execute the movement and change the states
 * @param buffer Pointer to a byte data buffer 
 * @param plane Pointer to a controller of axes
 * @param direction Type of a direction in case of 'SET HOME LOCATION' commad
 * @param isPowerOn Bool state in which the power supply is
 */
void handleInstructions(char *buffer, Plane *plane, directionType *direction, bool *isPowerOn){
    // ----- Preparation to process the message -----
    char *endptr = NULL;                // The rest of a converted string if it is NaN
    char instr = buffer[0];             // First message letter specifies an instruction
    buffer[0] = ' ';                    // REplace ethe first letter with a whitespace char
    directionType newDirection = NONE;  // Direction type to which should the last be changed

    // ----- Received point specifications -----
    static double x = -1;   // X coordination of the point
    static double y = -1;   // Y coordination of the point
    pointType pType = ERR;  // Type of the point on the line

    // Instruction recognizer
    switch(instr){
        // MOVE HOME
        case('h'):
            // Power up the motors and move to home location 
            *isPowerOn = powerOn();
            plane->penUp();
            plane->moveTo(0, 0); // home
            // Confirm execution back to the sender and power off
            serialConfirm();
            return;
        // MOVE A SINGLE MOTOR
        case('m'):
            // Select a proper state by the direction
            if(strstr(buffer, "left"))          newDirection = LEFT;
            else if(strstr(buffer, "right"))    newDirection = RIGHT;
            else if(strstr(buffer, "up"))       newDirection = UP;
            else if(strstr(buffer, "down"))     newDirection = DOWN;

            if(newDirection == *direction)
                // Movement stopped by the same arrow button click
                *direction = NONE;
            else
                *direction = newDirection;
            serialConfirm();
            return;
        // SET HOME LOCATION
        case('s'):
            // Reset the already done steps to 0, 0
            plane->reset();
            serialConfirm();
            return;
        // PREPARE TO MOVE TO X
        case('x'):
            // String to double
            x = strtod(buffer, &endptr);
            break;
        // PREPARE TO MOVE TO Y
        case('y'):
            y = strtod(buffer, &endptr);
        // GET THE POINT TYPE
        case('t'):
            if(strstr(buffer, "start"))     pType = START;
            else if(strstr(buffer, "mid"))  pType = MID;
            else if(strstr(buffer, "end"))  pType = END;
            break;
        default:
            break;
    }

    // Possible error in receiving
    if(endptr != NULL && endptr[0] != '\0'){
        x = y = -1;
        return;
    }

    // After the coords and the type is received, start moving
    if(pType != ERR && x >= 0 && y >= 0){
        *isPowerOn = powerOn();
        plane->moveTo(x, y);

        if(pType == START)
            plane->penDown();
        else if(pType == END)
            plane->penUp();
        // MID => keep the pen down

        // Reset the coordinates for the next message
        x = y = -1;

        serialConfirm();
    }
}


// ----- Global variables -----
Servo penServo;
StepperMotor motorX(pinsStepperX);
StepperMotor motorY(pinsStepperY);
// Main control object to move the axes
Plane plane(&motorX, &motorY, &penServo, PIN_SERVO_ENABLE);

void setup(){
    Serial.begin(BAUD_RATE);

    for(int i = 0; i < 4; i++){
        pinMode(pinsStepperX[i], OUTPUT);
        pinMode(pinsStepperY[i], OUTPUT);
    }
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_SERVO_ENABLE, OUTPUT);

    penServo.attach(PIN_SERVO);

    // Inform the communication unit that it can start sending data
    serialConfirm();
}

void loop(){
    static char buffer[RECEIVE_BUFFER_SIZE];    // Message buffer
    static unsigned index = 0;                  // Current buffer byte (char) index

    static directionType direction = NONE;      // Direction of a single stepper movement
    static bool isPowerOn = false;              // Power supply on/off state

    if(direction == NONE){
        // If the direction is not set, turn off the power and reset the accecleration
        if(isPowerOn){
            isPowerOn = powerOff();
            plane.accelerateBoth();
        }
    }
    else{
        // Prevention to not turn on the power every cycle (step) for long moves 
        if(!isPowerOn)
            isPowerOn = powerOn();

        // Direction is set, so a single stepper mover to set a new home location
        plane.step(direction);
    }

    // if some message is received, identify it
    if(Serial.available()){
        // Read one char (byte)
        char c = Serial.read();

        // Every correct message ends with a newline char
        if(c == '\n' && index != 0){
            // Create a 'string' from a char array
            buffer[index] = '\0';

            // Reset the current index for the next instruction (message)
            index = 0;

            // Parse the current message parts and do the movement
            handleInstructions(buffer, &plane, &direction, &isPowerOn);
        }
        // buffering coming bytes (letters)
        else if(index < RECEIVE_BUFFER_SIZE - 1)
            buffer[index++] = c;
        else
            index = 0;
    }
}