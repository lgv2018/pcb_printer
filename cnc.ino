/* 
 Elvileg a plotter kódja, A4988 motorvezérlőre írva. 
 */

#include <Arduino.h>
#include <Servo.h>
#include "BasicStepperDriver.h"
#include "MultiDriver.h"
#include "SyncDriver.h"

#define LINE_BUFFER_LENGTH 512
// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200
// Since microstepping is set externally, make sure this matches the selected mode
// If it doesn't, the motor will move at a different RPM than chosen
// 1=full step, 2=half step etc.
#define MICROSTEPS 32

// X motor
#define DIR_X 8
#define STEP_X 9

// Y motor
#define DIR_Y 6
#define STEP_Y 7

// RPM értékek
// TODO: Kitalálni, ezek mekkorák legyenek
#define RPM_X 5
#define RPM_Y 5

// 2-wire basic config, microstepping is hardwired on the driver
// Other drivers can be mixed and matched but must be configured individually
BasicStepperDriver stepperX(MOTOR_STEPS, DIR_X, STEP_X);
BasicStepperDriver stepperY(MOTOR_STEPS, DIR_Y, STEP_Y);

// Servo position for Up and Down 
const int penZUp = 80;
const int penZDown = 40;

// Servo on PWM pin 6
// TODO: Ezt is megnézni, hogy jó-e
const int penServoPin = 6; 

// Ez a sor az eredetiből van, és valszeg fog kelleni, mert ez a szervónak kell, nem a steppernek
// Egyelőre bennhagyom
Servo penServo;  

/* Structures, global variables    */
struct point { 
  float x; 
  float y; 
  float z; 
};

// Current position of plothead
// Ez is az ere
struct point actuatorPos;

//  Drawing settings, should be OK
float StepInc = 1;
int StepDelay = 0;
int LineDelay = 50;
int penDelay = 50;

// Motor steps to go 1 millimeter.
// Use test sketch to go 100 steps. Measure the length of line. 
// Calculate steps per mm. Enter here.
// TODO: Ezt mérjük le valamikor. Valszeg nekünk is ennyi lesz. 
float StepsPerMillimeterX = 6.0;
float StepsPerMillimeterY = 6.0;

// Drawing robot limits, in mm
// OK to start with. Could go up to 50 mm if calibrated well. 
// TODO: Ezt is állítsuk be. Nekünk egy jó 150 körüli kéne legyen, szóval egyelőre 100-ra állítom, mert az bőven kevés. 
float Xmin = 0;
float Xmax = 100;
float Ymin = 0;
float Ymax = 100;
float Zmin = 0;
float Zmax = 1;

float Xpos = Xmin; // X tengelyen alaphelyzet
float Ypos = Ymin; // Y tengelyen alaphelyzet
float Zpos = Zmax; // Z tengelyen alaphelyzet - a toll fel van emelve

// Set to true to get debug output.
boolean verbose = false;

//  Needs to interpret 
//  G1 for moving
//  G4 P300 (wait 150ms)
//  M300 S30 (pen down)
//  M300 S50 (pen up)
//  Discard anything with a (
//  Discard any other command!

/**********************
 * void setup() - Initialisations
 ***********************/
void setup() {
  //  Setup
  Serial.begin( 9600 );
  
  penServo.attach(penServoPin); // Mondtam, hogy fog kelleni az a Servo ott a 40. sorban
  penServo.write(penZUp);
  delay(200);

  // Decrease if necessary
  // TODO: Itt nem vagyok biztos, hogy csak ennyi kell, de egyelőre legyen.  
  stepperX.begin(RPM_X, MICROSTEPS);
  stepperY.begin(RPM_Y, MICROSTEPS);  

  //  Set & move to initial default position
  // TBD

  //  Notifications!!!
  Serial.println("Mini CNC Plotter alive and kicking!");
  Serial.print("X range is from "); 
  Serial.print(Xmin); 
  Serial.print(" to "); 
  Serial.print(Xmax); 
  Serial.println(" mm."); 
  Serial.print("Y range is from "); 
  Serial.print(Ymin); 
  Serial.print(" to "); 
  Serial.print(Ymax); 
  Serial.println(" mm."); 
}
