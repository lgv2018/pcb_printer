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
// TODO: Ezt is állítsuk be. Nekünk egy jó 40 körüli kéne legyen, szóval egyelőre 30-ra állítom, mert az bőven kevés. 
float Xmin = 0;
float Xmax = 30;
float Ymin = 0;
float Ymax = 30;
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

/**********************
 * void loop() - Main loop
 ***********************/
// Kicsit idegesít, hogy a serialon fogadástól eltekintve csak egy függvényhívás van, s az csinálja az érdemi munkát. 
// De legalább könnyebb debuggolni. 
// A loop()-on nem is írok át semmit, mert az csak a stringfeldolgozás.
void loop() 
{
  delay(200);
  char line[ LINE_BUFFER_LENGTH ];
  char c;
  int lineIndex;
  bool lineIsComment, lineSemiColon;

  lineIndex = 0;
  lineSemiColon = false;
  lineIsComment = false;

  while (1) {

    // Serial reception - Mostly from Grbl, added semicolon support
    // Ezt azt hiszem, verbatim bennhagyom, mert csak a serial küldés. Majd meg kell oldjuk, hogy itt küldjök a G-kódot. 
    while ( Serial.available()>0 ) {
      c = Serial.read();
      if (( c == '\n') || (c == '\r') ) {             // End of line reached
        if ( lineIndex > 0 ) {                        // Line is complete. Then execute!
          line[ lineIndex ] = '\0';                   // Terminate string
          if (verbose) { 
            Serial.print( "Received : "); 
            Serial.println( line ); 
          }
          processIncomingLine( line, lineIndex );
          lineIndex = 0;
        } 
        else { 
          // Empty or comment line. Skip block.
        }
        lineIsComment = false;
        lineSemiColon = false;
        Serial.println("ok");    
      } 
      else {
        if ( (lineIsComment) || (lineSemiColon) ) {   // Throw away all comment characters
          if ( c == ')' )  lineIsComment = false;     // End of comment. Resume line.
        } 
        else {
          if ( c <= ' ' ) {                           // Throw away whitepace and control characters
          } 
          else if ( c == '/' ) {                    // Block delete not supported. Ignore character.
          } 
          else if ( c == '(' ) {                    // Enable comments flag and ignore all characters until ')' or EOL.
            lineIsComment = true;
          } 
          else if ( c == ';' ) {
            lineSemiColon = true;
          } 
          else if ( lineIndex >= LINE_BUFFER_LENGTH-1 ) {
            Serial.println( "ERROR - lineBuffer overflow" );
            lineIsComment = false;
            lineSemiColon = false;
          } 
          else if ( c >= 'a' && c <= 'z' ) {        // Upcase lowercase
            line[ lineIndex++ ] = c-'a'+'A';
          } 
          else {
            line[ lineIndex++ ] = c;
          }
        }
      }
    }
  }
}

void processIncomingLine( char* line, int charNB ) {
  int currentIndex = 0;
  char buffer[ 64 ];                                 // Hope that 64 is enough for 1 parameter
  struct point newPos;

  newPos.x = 0.0;
  newPos.y = 0.0;

  //  Needs to interpret 
  //  G1 for moving
  //  G4 P300 (wait 150ms)
  //  G1 X60 Y30
  //  G1 X30 Y50
  //  M300 S30 (pen down)
  //  M300 S50 (pen up)
  //  Discard anything with a (
  //  Discard any other command!

  while( currentIndex < charNB ) {
    switch ( line[ currentIndex++ ] ) {              // Select command, if any
    case 'U':
      penUp(); 
      break;
    case 'D':
      penDown(); 
      break;
    case 'G':
      buffer[0] = line[ currentIndex++ ];          // /!\ Dirty - Only works with 2 digit commands
      //      buffer[1] = line[ currentIndex++ ];
      //      buffer[2] = '\0';
      buffer[1] = '\0';

      switch ( atoi( buffer ) ){                   // Select G command
      case 0:                                   // G00 & G01 - Movement or fast movement. Same here
      case 1:
        // /!\ Dirty - Suppose that X is before Y
        char* indexX = strchr( line+currentIndex, 'X' );  // Get X/Y position in the string (if any)
        char* indexY = strchr( line+currentIndex, 'Y' );
        if ( indexY <= 0 ) {
          newPos.x = atof( indexX + 1); 
          newPos.y = actuatorPos.y;
        } 
        else if ( indexX <= 0 ) {
          newPos.y = atof( indexY + 1);
          newPos.x = actuatorPos.x;
        } 
        else {
          newPos.y = atof( indexY + 1);
          indexY = '\0';
          newPos.x = atof( indexX + 1);
        }
        drawLine(newPos.x, newPos.y ); // A DrawLine függvényben vannak stepper dolgok, a többi csak feldolgozásnak tűnik
        //        Serial.println("ok");
        actuatorPos.x = newPos.x;
        actuatorPos.y = newPos.y;
        break;
      }
      break;
    case 'M':
      buffer[0] = line[ currentIndex++ ];        // /!\ Dirty - Only works with 3 digit commands
      buffer[1] = line[ currentIndex++ ];
      buffer[2] = line[ currentIndex++ ];
      buffer[3] = '\0';
      switch ( atoi( buffer ) ){
      case 300:
        {
          char* indexS = strchr( line+currentIndex, 'S' );
          float Spos = atof( indexS + 1);
          //          Serial.println("ok");
          if (Spos == 30) { 
            penDown(); 
          }
          if (Spos == 50) { 
            penUp(); 
          }
          break;
        }
      case 114:                                // M114 - Repport position
        Serial.print( "Absolute position : X = " );
        Serial.print( actuatorPos.x );
        Serial.print( "  -  Y = " );
        Serial.println( actuatorPos.y );
        break;
      default:
        Serial.print( "Command not recognized : M");
        Serial.println( buffer );
      }
    }
  }
}


/*********************************
 * Draw a line from (x0;y0) to (x1;y1). 
 * Bresenham algo from https://www.marginallyclever.com/blog/2013/08/how-to-build-an-2-axis-arduino-cnc-gcode-interpreter/
 * int (x1;y1) : Starting coordinates
 * int (x2;y2) : Ending coordinates
 **********************************/
void drawLine(float x1, float y1) {

  if (verbose)
  {
    Serial.print("fx1, fy1: ");
    Serial.print(x1);
    Serial.print(",");
    Serial.print(y1);
    Serial.println("");
  }  

  //  Bring instructions within limits
  if (x1 >= Xmax) { 
    x1 = Xmax; 
  }
  if (x1 <= Xmin) { 
    x1 = Xmin; 
  }
  if (y1 >= Ymax) { 
    y1 = Ymax; 
  }
  if (y1 <= Ymin) { 
    y1 = Ymin; 
  }

  if (verbose)
  {
    Serial.print("Xpos, Ypos: ");
    Serial.print(Xpos);
    Serial.print(",");
    Serial.print(Ypos);
    Serial.println("");
  }

  if (verbose)
  {
    Serial.print("x1, y1: ");
    Serial.print(x1);
    Serial.print(",");
    Serial.print(y1);
    Serial.println("");
  }

  //  Convert coordinates to steps
  x1 = (int)(x1*StepsPerMillimeterX);
  y1 = (int)(y1*StepsPerMillimeterY);
  float x0 = Xpos;
  float y0 = Ypos;

  //  Let's find out the change for the coordinates
  long dx = abs(x1-x0);
  long dy = abs(y1-y0);
  int sx = x0<x1 ? StepInc : -StepInc; // Actual number of steps done in the X direction
  int sy = y0<y1 ? StepInc : -StepInc; // Actual number of steps done in the Y direction

  long i;
  long over = 0;

  if (dx > dy) {
    for (i=0; i<dx; ++i) {
      StepperX.move(sx); // A mi könyvtárunkban így adjuk meg, hogy ennyi lépést menjen
      over+=dy;
      if (over>=dx) {
        over-=dx;
        StepperY.move(sy);
      }
      delay(StepDelay);
    }
  }
  else {
    for (i=0; i<dy; ++i) {
      StepperY.move(sy);
      over+=dx;
      if (over>=dy) {
        over-=dy;
        StepperX.move(sx);
      }
      delay(StepDelay);
    }    
  }

  if (verbose)
  {
    Serial.print("dx, dy:");
    Serial.print(dx);
    Serial.print(",");
    Serial.print(dy);
    Serial.println("");
  }

  if (verbose)
  {
    Serial.print("Going to (");
    Serial.print(x0);
    Serial.print(",");
    Serial.print(y0);
    Serial.println(")");
  }

  //  Delay before any next lines are submitted
  delay(LineDelay);
  //  Update the positions
  Xpos = x1;
  Ypos = y1;
}

//  Raises pen
// Ezt elvileg nem kell átírni, mert a servo könyvtár ugyanaz
void penUp() { 
  penServo.write(penZUp); 
  delay(LineDelay); 
  Zpos=Zmax; 
  if (verbose) { 
    Serial.println("Pen up!"); 
  } 
}
//  Lowers pen
// Ugyanezért ezt sem
void penDown() { 
  penServo.write(penZDown); 
  delay(LineDelay); 
  Zpos=Zmin; 
  if (verbose) { 
    Serial.println("Pen down."); 
  } 
}
