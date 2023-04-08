// KLR-5A [Klarissa], 5-axis 3d-printed Robot Arm Controller
// ©2023 Iniq Myers
// Controller Hardware: Teensy 4.1
// Closed-loop Motor Controllers:
//    Axis1,2,3 - SKR Servo042C V1.1.1  (Step+Dir and UART) [Nema 17 2.0A]
//    Axis4 - Bigtreetech S42C V1.0 (Step+Dir and UART) [Nema 17 1.5A]
// Home positions determined using magnetic hall-effect sensors
// Axis limits defined with micro limit switches (axis 4 has no hall sensor, only microswitch)
// Motors inteface with 3d-printed Split-ring compound planetary epicyclic gearboxes (credit @skyentific)
// Body and linkage design originates from Skyentific 5-axis robot, but modified to resemble Kuka KR600/700
// All parts printed in PLA or Syriatech BluV2[SLA]
// Teach pendant has a 3-axis joystick and emergency stop to kill motor power


// ^^^^^^^^^^^^^^ Libraries ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#include "Arduino.h"      // Library for supporting standard Arduino functions on Teensy Hardware
//#include "Joystick.h"
#include <EEPROM.h>       // Library for storing/recalling data from onboard EEPROM
#include <Bounce2.h>      // Library for debouncing inputs
#include "ArduPID.h"      // Library for PID motor control
#include "teensystep4.h"  // Library for fast, asynchronous stepper motor control on Teensy4
using namespace TS4;      // Namespace for TeensyStep4

// $$$$$$$$$$$ function prototypes
void setupIO(); // Set pins to Input/Output modes and zero joystick axes
void setupMotors(); //Enable outputs, begin TS4 and set speeds

enum axis {X,Y,Z};
axis& operator++(axis& orig) {  //pre increment operator
  orig = static_cast<axis>(orig + 1);
  return orig;
}
axis operator++(axis& orig, int) {  //post increment operator
  axis rVal = orig;
  ++orig;
  return rVal;}

class JoyStick{
  private:
    int teachPendantPinX;
    int teachPendantPinY;
    int teachPendantPinZ;
    int buttonTeachPendantPin;
    //uint16_t Xpos;             // For reading/calculating Joystick X input [0-1023]
                              // ****** shouldn't need, call getPosition() when needed
    uint16_t home[3];            // Home value for zeroing Joystick input [0-1023]
    //uint16_t Zpos;             // For reading/calculating Joystick Z input [0-1023]
    //uint16_t Zhome;            // Home value for zeroing Joystick Z input [0-1023]
    uint8_t  Deadzone; // How far off of home does joystick need to be to activate? [0-255] <--- We will want to have one for each axis, and will want to build a calibration routine
    Bounce buttonBounce; // Define debounce object for joystickButton
  public:
    JoyStick(int pinX, int pinY, int pinZ, int buttonPin); //constructor
    void setPinModes();    
    uint16_t getHome(axis direction);
    uint8_t getDeadzone() {return Deadzone;}
    uint16_t getPosition(axis direction);       
    void getXYZ(uint16_t &x, uint16_t &y, uint16_t &z);
    void setHome();
    uint16_t gethome(axis direction);
    void rotate(axis direction, Stepper& myMotor, uint16_t speed);    //use an enumerated type for direction
};//end of Joystick class

JoyStick::JoyStick(int pinX, int pinY, int pinZ, int buttonPin) {
      teachPendantPinX = pinX;
      teachPendantPinY = pinY;
      teachPendantPinZ = pinZ;
      buttonTeachPendantPin = buttonPin;      
      Deadzone = 50;     
      buttonBounce = Bounce();    
} //end of constructor

void JoyStick::setPinModes() { pinMode(teachPendantPinX,INPUT); //**** is INPUT an enumerated type? <-- Probably, and it's also probably defined wherever pinMode is...
                        pinMode(teachPendantPinY,INPUT); //**** would this ever be set to something besides INPUT <----No, for analog inputs this will always just be INPUT. Technically this is redundant since analog pins are input by default
                        pinMode(teachPendantPinZ,INPUT);  //****which library does pinMode belong to?<--- I guess Arduino.h but I'm having difficulty figuring that out...
                        // pinMode(JOYSTICK_BUTTON,INPUT); //Joystick Button
} //end of setPinModes    

void JoyStick::getXYZ(uint16_t &x, uint16_t &y, uint16_t &z){
          x = getPosition(axis::X);
          y = getPosition(axis::Y);
          z = getPosition(axis::Z);                  
} // end of getAllPositions 

void JoyStick::setHome(){
  axis direction = axis::X;
  for(int i=0;i<3;i++)
    home[i] = getPosition(direction++);
} //end of setHome

uint16_t JoyStick::gethome(axis direction){
  return home[direction];
}

uint16_t JoyStick::getPosition(axis direction){
  switch(direction){
    case X: return analogRead(teachPendantPinX); break;
    case Y: return analogRead(teachPendantPinY); break;
    case Z: return analogRead(teachPendantPinZ); break;
  }
return 0;  //this would be an error
}

void JoyStick::rotate(axis direction, Stepper& myMotor, uint16_t speed){
  int position=0;
  uint16_t currentHome=0;

  switch(direction){
    case X: position = getPosition(axis::X); 
                        currentHome = home[0]; break;
    case Y: position = getPosition(axis::Y); 
                        currentHome = home[1]; break;
    case Z: position = getPosition(axis::Z); 
                        currentHome = home[2]; break;                                       
  }
      if(abs(position-currentHome)>Deadzone){ //Is movement greater than deadzone?
        myMotor.rotateAsync(speed);
        if(position<currentHome){//Stick Pushed Right, Rotate Right
          myMotor.overrideSpeed(-(position-currentHome)/512.0); //Scale motor to axis 
        }else{//Stick Pushed Left, Rotate Left
          myMotor.overrideSpeed(((currentHome-position)/512.0));
        }
      }else{ //Not outside deadzone
          myMotor.overrideSpeed(0.0); //Set speed 0
      }
}



JoyStick joystick(21,22,23,20);

// ################# Constant Declarations ########################
// Motor Driver I/O Pins
/*
#define AXIS1EN  XX // Axis1 - Motor Enable (Low Active)
#define AXIS1DIR XX // Axis1 - Motor Direction
#define AXIS1STP XX // Axis1 - Motor Step
*/
#define AXIS2EN  6  // Axis2 - Motor Enable (Low Active)
#define AXIS2DIR 8  // Axis2 - Motor Direction
#define AXIS2STP 7  // Axis2 - Motor Step

#define AXIS3EN  35 // Axis3 - Motor Enable (Low Active)
#define AXIS3DIR 33 // Axis3 - Motor Direction
#define AXIS3STP 34 // Axis3 - Motor Step

#define AXIS4EN  38 // Axis4 - Motor Enable (Low Active)
#define AXIS4DIR 36 // Axis4 - Motor Direction
#define AXIS4STP 37 // Axis4 - Motor Step
/*
#define AXIS5EN  XX // Axis5 - Motor Enable
#define AXIS5DIR XX // Axis5 - Motor Direction
#define AXIS5STP XX // Axis5 - Motor Step
*/
// Motor Feedback I/O Pins
/*
#define AXIS1HOM XX // Axis1 - Home Position Magnetic Sensor
#define AXIS1ENC XX // Axis1 - Magnetic Absolute Position Encoder (Analog)
#define AXIS1END XX // Axis1 - Endstop Limit Switch (Both Sides)
*/
#define AXIS2HOM 10 // Axis2 - Home Position Magnetic Sensor
#define AXIS2ENC 19 // Axis2 - Magnetic Absolute Position Encoder (Analog)
#define AXIS2END 8  // Axis2 - Endstop Limit Switch (Both Sides)

#define AXIS3HOM 11 // Axis3 - Home Position Magnetic Sensor
#define AXIS3ENC 40 // Axis3 - Magnetic Absolute Position Encoder (Analog)
#define AXIS3END 31 // Axis3 - Endstop Limit Switch (Both Sides)

#define AXIS4HOM 39 // Axis4 - Home/Endstop Limit Switch (Infinite Rotation)
#define AXIS4ENC 41 // Axis4 - Magnetic Absolute Position Encoder (Analog)
/*
#define AXIS5HOM XX // Axis5 - Home Position Magnetic Sensor
#define AXIS5ENC XX // Axis5 - Magnetic Absolute Position Encoder (Analog)
#define AXIS5END XX // Axis5 - Endstop Limit Switch (Both Sides)
*/
// Generic
#define LED 13 //Onboard Feedback Led


// ************************** Variable Declarations *******************************
double input,output,setpoint;

uint16_t maximumSpeed = 9000;         // Maximum motor speed      (STP+DIR)
uint16_t acceleration = 25000;        // Motor Acceleration       (STP+DIR)
uint16_t speed = 5000;                // Motor Speed              (STP+DIR)

uint16_t  homingRotationSpeed = 2000; // Motor speed while homing (STP+DIR)
int32_t   homingRange;                // Total range between endstops
int32_t   homingBottom;               // Bottom endstop position
int32_t   homingTop;                  // Top endstop position
bool      homing = false;             // Is the robot in homing state (allows for running the endstops without disabling motors)   
uint16_t  rawPos[5];                  // Axis3 Raw analog encoder position
bool      a3end = false;              // Axis3 endstop state
uint8_t   encoderPins[5];             // Array of analog pins for updating encoder values

volatile bool a3home=false;           // The current home switch status of Axis 3
volatile bool a4home=false;           // The current home switch status of Axis 4

volatile bool estop = true;
bool mstop = true;

double input3,output3,setpoint3=512;
double input4,output4,setpoint4=512;
double Kp3 =0.022, Ki3 = 0, Kd3 = .0001; // Define PID tuning values
double Kp4 =.01, Ki4 = 0.0, Kd4 = 0; // Define PID tuning values
ArduPID a3PID; // Define PID Object
ArduPID a4PID;
int targetPosition; // Define target position for PID controller

// %%%%%%%%%%%%%%%%%%%%%% Stepper Motor Declarations %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//Stepper axis1(AXIS1STP, AXIS1DIR); // Define stepper motor with Step and Direction pins
Stepper axis2(AXIS2STP, AXIS2DIR); // Define stepper motor with Step and Direction pins
Stepper axis3(AXIS3STP, AXIS3DIR);  // Define stepper motor with Step and Direction pins
Stepper axis4(AXIS4STP, AXIS4DIR);  // Define stepper motor with Step and Direction pins
//Stepper axis5(AXIS5STP, AXIS5DIR);  // Define stepper motor with Step and Direction pins

// ()()()() Other Declarations ()()()()
Bounce a3h = Bounce(); // Define debounce object for axis3Home
Bounce a3s = Bounce(); // Define debounce object for axis3End 

void a3end_ISR(){ //Axis 3 HomeSwitch On Interrupt
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if(!a3end){
    if(interrupt_time - last_interrupt_time >5000){
        a3end = true;
    }
    last_interrupt_time = interrupt_time;
  }
}

void axis4Home(){ // Depreciated Homing Process (needs to be reworked to be axis-agnostic)
  //Rotate Clockwise until Homing Switch is Engaged
  while(!digitalRead(AXIS4HOM)){
    axis4.rotateAsync(homingRotationSpeed);
  }
  //Back off of switch at 1/4 speed
  while(digitalRead(AXIS4HOM)){
    axis4.rotateAsync(-(homingRotationSpeed/4));
  }
  delay(500);
  //Re-engage switch at 1/4 speed
  while(!digitalRead(AXIS4HOM)){
    axis4.rotateAsync(homingRotationSpeed/4);
  }
  //Set new home position to homing switch
  axis4.setPosition(0.0);
  //delay(1000);

  //Move off switch in the negative direction
  while(digitalRead(AXIS4HOM)){
    axis4.rotateAsync(-homingRotationSpeed);
  }
  delay(100);
  while(!digitalRead(AXIS4HOM)){
    axis4.rotateAsync(-homingRotationSpeed);
  }

  //Back off of switch at 1/4 speed
  while(digitalRead(AXIS4HOM)){
    axis4.rotateAsync((homingRotationSpeed/4));
  }
  delay(100);
  //Re-engage switch at 1/4 speed
  while(!digitalRead(AXIS4HOM)){
    axis4.rotateAsync(-(homingRotationSpeed/4));
  }
  homingRange = axis4.getPosition();
  
  while(axis4.getPosition()<=(homingRange/2.0)){
    axis4.rotateAsync(homingRotationSpeed);
    delayMicroseconds(15);
  }
  axis4.setPosition(0);
  homingTop = homingRange/2;
  homingBottom = -(homingRange/2);
}

void setupIO(){ // Setup pin modes for I/O
  joystick.setPinModes();  

  encoderPins[0]= AXIS3ENC; //Set to axis3 while testing
  encoderPins[1]= AXIS3ENC; //Set to axis3 while testing
  encoderPins[2]= AXIS3ENC;
  encoderPins[3]= AXIS4ENC;
 // encoderPins[4]= AXIS5ENC;
 // pinMode(AXIS1EN,OUTPUT);       // Axis 1 Enable (Step + Direction set by TS4)
  pinMode(AXIS2EN,OUTPUT);        // Axis 2 Enable (Step + Direction set by TS4)
  pinMode(AXIS3EN,OUTPUT);        // Axis 3 Enable (Step + Direction set by TS4)
  pinMode(AXIS4EN,OUTPUT);        // Axis 4 Enable (Step + Direction set by TS4)
 // pinMode(AXIS5EN,OUTPUT);       // Axis 5 Enable

  a3h.attach ( AXIS3HOM , INPUT ); // Attach debounce object to Axis3Hom
  a3h.interval(10); // Define debounce interval (ms)
  a3s.attach(AXIS3END,INPUT_PULLUP); //Attach debounce object to Axis3End
  a3s.interval(30); // Define debounce interval(ms)

  // Record Zero Position for all joystick axes to calculate offsets 
  joystick.setHome();  
  }

void setupMotors(){ // Enable motor outputs, begin TS4 service and set motor speeds
  // Enable Motor Outputs
  //digitalWrite(AXIS1EN,LOW); //Low Active, enable axis1 motor motion
  digitalWrite(AXIS2EN,LOW); //Low Active, enable axis2 motor motion
  digitalWrite(AXIS3EN,LOW); //Low Active, enable axis3 motor motion
  digitalWrite(AXIS4EN,LOW); //Low Active, enable axis4 motor motion
  //digitalWrite(AXIS5EN,LOW); // enable axis5 motor motion

  TS4::begin(); //Begin TeensyStep4 Service

  // Set Motor Speeds and acceleration
 // axis1.setMaxSpeed(maximumSpeed);
 // axis1.setAcceleration(acceleration);
  axis2.setMaxSpeed(maximumSpeed);
  axis2.setAcceleration(acceleration);
  axis3.setMaxSpeed(maximumSpeed);
  axis3.setAcceleration(acceleration);
  axis4.setMaxSpeed(maximumSpeed);
  axis4.setAcceleration(acceleration);

}


void updatePositions(){
  for(int i=0;i < 5;i++){ // For each axis
      rawPos[i]=analogRead(encoderPins[i]); //Read encoder pin[0-1023] and write to rawPos[axis-1]
  }
}

uint16_t updatePosition(uint8_t axis){
  rawPos[axis-1]=analogRead(encoderPins[axis-1]);
  return rawPos[axis-1];
}

void a3homingTest(){
  uint16_t top,bot,hom1,hom2,hom3;
  Serial.println("homing test begun");
  
  Serial.println("Check Nearest Endstop");
  a3h.update(); //Process debounce on axis3Home
  a3s.update(); //Process debounce on axis3End
 // updatePositions();//Update position encoders
  if(updatePosition(3)>512){ //If it's on the top side of home
    Serial.println("Top Switch First");
    Serial.println(rawPos[2]);
    axis3.rotateAsync(-homingRotationSpeed); //Rotate upwards at the homing speed
    while(!a3s.fell()){ //While the endstop hasn't triggered
      a3s.update(); //Update bounce input
    }
    axis3.stop(); //Stop when endstop reached
    delayMicroseconds(100); //let inputs and motors settle 100us
    updatePositions(); //Update encoders
    Serial.print("Top Switch Position: ");
    top = rawPos[2]; //Store top position
    Serial.println(top);

    Serial.println("Home Topside Next");
    a3h.update(); //Process home switch debounce
    axis3.rotateAsync(homingRotationSpeed); //Rotate downwards at homing speed
    while(!a3h.read()){ //While the homing switch hasn't triggered
      a3h.update(); //Update bounce input
    }
    hom2 = updatePosition(3); // Record encoder position for home range top
    //updatePositions(); //When triggered, read encoder value
    Serial.print("Homing Top: ");
    Serial.println(hom2);

    axis3.rotateAsync(homingRotationSpeed); //re-assert rotation in the downwards direction
    while(a3h.read()){//While homing switch is still triggered
      a3h.update(); //Update the bounce input
    }
    hom3 = updatePosition(3); //When home switch untriggers, record encoder position
    Serial.print("Homing Bottom(a):");
    Serial.println(hom3);
    delayMicroseconds(100);
    a3s.update();
    Serial.println("Bottom Switch Next");
    axis3.rotateAsync(homingRotationSpeed); //re-assert downward rotation at homing speed
    while(!a3s.fell()){ //while the endstop hasn't triggered
      a3s.update(); //Update bounce processor
    }
    axis3.stop(); //When triggered, stop motors
    bot = updatePosition(3);
    Serial.print("Bottom Switch: ");
    Serial.println(bot);

    Serial.println("Home Verification/Return Last");
    axis3.rotateAsync(-homingRotationSpeed); //Begin rotation in upwards direction at homing speed
    while(!a3h.read()){ //While the homing switch is not active
      a3h.update(); //Update bounce processor
    }
    axis3.stop(); //When triggered, stop and record encoder value
    hom1 =(updatePosition(3)+hom3)/2;
    Serial.print("Home Bottom(b)");
    Serial.println(rawPos[2]);
    hom1 = (rawPos[2]+hom3)/2; //Set home range bottom
    hom3 = (hom1+hom2)/2; //Set home range middle
    

    Serial.println("Homing Test Complete");
    updatePositions();
    axis3.rotateAsync(-homingRotationSpeed/10); //Continue upward rotation at homingSpeed/10
    while(updatePosition(3)<hom3){ //Wait until encoder position is at least "home middle"
    //Wait
    }
    axis3.stop(); //Stop motor
    axis3.setPosition(0); //Set TeensyStep motor position to 0 (Also sets speed to 0)

    // ----------------End of Topside Test---------------

  }else{
    Serial.println("Bottom Switch First");
    axis3.rotateAsync(homingRotationSpeed); // Rotate in downward direction at homing speed
    a3s.update(); //Process debounce on a3Endstop
    while(!a3s.fell()||a3s.read()!=0){ //while endstop not triggered or held down
      a3s.update(); // update debounce processor
    }
    axis3.stop(); // When triggered or held down, stop motor motion
    updatePositions();
    Serial.print("Bottom Switch Position: ");
    Serial.println(rawPos[2]);
  }

  // -------------- End of Bottomside Test ------------------

  //Verify home/encoder matched
  while(!a3h.read()){ //While homing sensor is not flagged...
    updatePositions();
    if(a3s.read()){ //Check if endstop flagged
      axis3.stop(); // STOP! (We should be rotating towards home!)
      if(rawPos[2]>512){ // Check Position and begin rotating away from stops
        axis3.rotateAsync(homingRotationSpeed);
      }else{
        axis3.rotateAsync(-homingRotationSpeed);
      }
    }else{ // Endstop not flagged
     if(rawPos[2]>512){ //Rotate towards home
        axis3.rotateAsync(homingRotationSpeed);
      }else{
        axis3.rotateAsync(-homingRotationSpeed);
      }
    }
    a3s.update();
    a3h.update();
  }
  updatePositions();
  int a3nicePos = map (rawPos[2],0,1023,-180,180); //map analog input to axis degrees
  if(a3nicePos<-4 || a3nicePos>8){ //did the sensor flag outside the accepted range?
    Serial.println("Encoder/Homing Switch Mismatch! Check Alignment.");
    Serial.println(rawPos[2]);
    axis4.overrideSpeed(0.0);
  }else{ //Homing switch and encoder agree, test complete
    Serial.println("Basic Homing Test Complete");
    Serial.println(a3nicePos); //Usually homes at -2
    axis4.overrideSpeed(0.0);
  }

}


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<   SETUP (Run Once at Startup)  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
void setup()
{
  estop = false; //Allow robot motion
  mstop = true;  //Force ManualControl
  Serial.begin(115200); //Begin USB serial for debugging
  Serial.print("Homing Joystick, Leave Center...");
  setupIO(); // Set pins to Input/Output modes and zero joystick axes
  Serial.print("...");
  setupMotors(); //Enable outputs, begin TS4 and set speeds
  Serial.print("done.");
  //a3homingTest(); //Home axis3, verify homing switch, endstops and magnetic position encoder
  targetPosition = 700; //Set target position for PID axis control
  setpoint3 = targetPosition;

    a3PID.begin(&input3,                // input
              &output3,               // current output
              &setpoint3,             // setpoint
              Kp3,Ki3,Kd3          );   // P,I,D
  a3PID.setSampleTime(25);
  a3PID.start();
  a3PID.setOutputLimits(-1,1);

  /*a4PID.begin(&input4,                // input
              &output4,               // current output
              &setpoint4,             // setpoint
              Kp4,Ki4,Kd4          );   // P,I,D
  a4PID.setSampleTime(25);
  a4PID.start();
  a4PID.setOutputLimits(-1,1);*/
}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> LOOP (Run repeatedly after Setup) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void loop()
{
    //&&&&&&&&&&&&&&& PID Control Testing &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
  input3 = updatePosition(3);
  int a3nicePos = map (input3,0,1023,-180,180); //Map raw encoder to degrees
  a3s.update(); //update endstop
  if(a3s.fell()){ //if endstop triggered
    axis3.stop();
    mstop = true;
    Serial.println("Endstop Limit Switch Activated! Stopping Motors");
    if(a3nicePos>-106 || a3nicePos<103){ //Is the encoder outside an expected endstop zone?
       Serial.println("Encoder/Endstop Position Mismatch! Check Alignment");
    }
  }

  if(!estop&&!mstop){
    a3PID.compute();
  //  a4PID.compute();

    if(abs(output3)>0){
      axis3.rotateAsync(maximumSpeed);
      axis3.overrideSpeed(-output3);
    }else{
      if(input3==setpoint3){
        axis3.stop();
        Serial.print(" Target Reached: ");
        Serial.println(input3);
        delay(1000);
        if(input3==setpoint3){
          int newTarget = random(220,511);
          while(abs(newTarget-setpoint3)<10){
            newTarget = random(220,511);
          }
          setpoint3 = newTarget;
          Serial.print(" New Target: ");
          Serial.print(setpoint3);
        }
      }
      
    }

    
  }else{
    //*************** Manual Control Mode ********************
    updatePositions();
    int a3nicePos = map (rawPos[2],0,1023,-180,180); //Map raw encoder to degrees
    // *#*#*#*#*#*#*#*#*#*#*#*#*# Safety Checks *#*#*#*#*#*#*#*#*#*#*#*#
    a3s.update(); //update endstop bounce
    if(a3s.fell()){ //if endstop triggered
      axis3.stop();
      //estop = false;
      mstop = true;
      Serial.println("Endstop Limit Switch Activated! Stopping Motors");
      if(a3nicePos>-106 || a3nicePos<103){ //Is the encoder outside an expected endstop zone?
        Serial.println("Encoder/Endstop Position Mismatch! Check Alignment");
      }
    }
    a3h.update(); //update home switch bounce
    if(a3h.rose()){ //if home switch activated
        a3home = true;
        a3end = false;
        //Serial.println("Home Position Detected");
        mstop = false;
      // estop = true;
      if(a3nicePos<-5 || a3nicePos>8){ //Is the encoder outside the acceptable homing zone?
        //estop = true;
        mstop=true;
        Serial.println("Homing Sensor / Encoder Mismatch");
        Serial.println(a3nicePos);
      }
    }
    //#*#*#*#*#*#* Safety Checks Complete, Allow Motion Calculation *#*#*#*#**#*#

  //Manual Joystick Inputs to Motor Outputs
    if(!estop){ //Skip all if estop
        joystick.rotate(axis::Y, axis3, speed);
        joystick.rotate(axis::X, axis2, speed);
        joystick.rotate(axis::Z, axis4, speed);
    }else{ //Estop = true!!
    //  axis1.overrideSpeed(0.0);
      axis2.overrideSpeed(0.0);
      axis3.overrideSpeed(0.0); //Set motor speed 0
      axis4.overrideSpeed(0.0);
      //axis5.overrideSpeed(0.0);
    }
  }

  
  
  
  
  
 
}
