// Including libraries
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include "Adafruit_VL53L0X.h"
#include <VL53L0X.h>
#include "INA226.h"
#include <Adafruit_ICM20948.h>

// Constants
#define SOUND_SPEED 0.034   //speed of sound at 20C cm/uS

// Defining error codes

// Defining pins
int buzzerPin = 32;
int dcVoltageSensor = 13;

int serialCL = 22;
int serialDA = 21;

int PCA9548AA0 = 33;
int PCA9548AA1 = 25;
int PCA9548AA2 = 26;

int servoRx = 23;
int servoTX = 19;

int trigSideLeft = 17;
int echoSideLeft = 16;
int trigSideRight = 4;
int echoSideRight = 2;
int trigFrontLeft = 27;
int echoFrontLeft = 36;
int trigFrontRight = 14;
int echoFrontRight = 39;
int trigFrontDown = 12;
int echoFrontDown = 34;

//Variables
int servoESPUARTBaud = 9600;
String UARTValidationMesg = "SENS-SERV_UART_W";  // string to validate working UART to servo esp, valid if "SENS_UART_V"
String UARTValidAnsw = "SENS-SERV_UART_V";  // answer if UART is working as expected
int errorCount = 0;
int warningCount = 0;
    //variables for serial data string
int tof_front_dmm;  //_dmm for distance in millimeters
int tof_back_dmm;
float hc_left_dmm;
float hc_right_dmm;
float hc_frontLeft_dmm;
float hc_frontRight_dmm;
float hc_frontDown_dmm;
float icm_pitch;
float icm_roll;
float icm_temp;
float icm_magX;
float icm_magY;
float icm_magZ;
float ina_vbus;

String tof_front_dmmS; //_dmmS for distance in millimeters formatted as String
String tof_back_dmmS;
String hc_left_dmmS;
String hc_right_dmmS;
String hc_frontLeft_dmmS;
String hc_frontRight_dmmS;
String hc_frontDown_dmmS;
String icm_pitchS;    //formatted as string
String icm_rollS;
String icm_tempS;
String icm_magXS;
String icm_magYS;
String icm_magZS;
String ina_vbusS;
// bools for initialization look-up
bool tof_front_bool = false;
bool tof_back_bool = false;
bool ina226_bool = false;
bool icm_bool = false;
// Kalman filter variables
float pitch, roll;                         // Filtered angles
float pitch_rate, roll_rate;               // Angular velocity from gyroscope
float dt = 0.01;                           // Time step (s)
float gyro_angle_pitch, gyro_angle_roll;   // Gyroscope-integrated angles
float accel_angle_pitch, accel_angle_roll; // Accelerometer angles
float bias_pitch = 0, bias_roll = 0;       // Estimated biases
float P[2][2] = {{1, 0}, {0, 1}};          // Error covariance matrix
float Q_angle = 0.001, Q_bias = 0.003;     // Process noise
float R_measure = 0.03;                    // Measurement noise


//PCA9548A busses
int tofFront_Bus = 2;
int tofBack_Bus = 7;
int ina226_Bus = 3;

// I2C addresses
uint8_t PCA9548A_Address = 0x70;
uint8_t INA226_Address = 0x40;
uint8_t tofFront_Address = 0x30;
uint8_t tofBack_Address = 0x30;

// Declaring variables
int jetsonBootUpTime = 10000; // Waiting 10 seconds for the Jetson to boot up

// Defining instances
HardwareSerial ServoSerial(2);  //UART to servo ESP32
Adafruit_ICM20948 ICM;  //ICM20948 object
INA226 INA(INA226_Address);
VL53L0X tofFront;
VL53L0X tofBack;

// Defining structs
struct initTofBoolStruct {
  bool tof_front_valid;
  bool tof_back_valid;
};

struct icmAngleStruct {
  float icm_pitch_struct;
  float icm_roll_struct;
};

// Declaring methods
void PCA9548ASelect(int);
float getUltrasonicDistance(int);
icmAngleStruct getICMAnglesDeg();
float inaGetVBusVolt();
bool servoUARTValidation();
initTofBoolStruct initTof();
bool initINA226();
bool initICM();

void setup() {
  // set up debug / check serial
  Serial.begin(115200);
  Wire.begin();

  // wait for the Jetson to boot up
  delay(jetsonBootUpTime);

  // set pinMode
  pinMode(buzzerPin, OUTPUT);
  pinMode(dcVoltageSensor, INPUT);

  pinMode(PCA9548AA0, OUTPUT);
  pinMode(PCA9548AA1, OUTPUT);
  pinMode(PCA9548AA2, OUTPUT);

  pinMode(trigSideLeft, OUTPUT);
  pinMode(echoSideLeft, INPUT);
  pinMode(trigSideRight, OUTPUT);
  pinMode(echoSideRight, INPUT);
  pinMode(trigFrontLeft, OUTPUT);
  pinMode(echoFrontLeft, INPUT);
  pinMode(trigFrontRight, OUTPUT);
  pinMode(echoFrontRight, INPUT);
  pinMode(trigFrontDown, OUTPUT);
  pinMode(echoFrontDown, INPUT);

  //Start UART to servo ESP32
  try {
    ServoSerial.begin(servoESPUARTBaud, SERIAL_8N1, servoRx, servoTX);
    Serial.println("SENS_INIT_I_001");
  }catch (String error) {
    Serial.println("SENS_INIT_E_001");
    //ServoSerial.println("SENS_INIT_E_001");
    errorCount++;
    while(1);
  }

  bool servoUARTworking = servoUARTValidation(); // is set to true, when UART to servo esp32 has been tested and validated

  if(servoUARTworking == false){  // continue, if UART comminication is working correctly
    while(1);
  } else {
    Serial.println("SENS_COMU_I_001");
    ServoSerial.println("SENS_COMU_I_001");
  }



//beginning of sensor initialization
  initTofBoolStruct tofBools = initTof();

  if(tofBools.tof_front_valid == false){
    Serial.println("SENS_INIT_W_004");
    ServoSerial.println("SENS_INIT_W_004");
    warningCount++;
  } else {
    Serial.println("SENS_INIT_I_003");
    ServoSerial.println("SENS_INIT_I_003");
  }
  
  if(tofBools.tof_back_valid == false){
    Serial.println("SENS_INIT_W_005");
    ServoSerial.println("SENS_INIT_W_005");
    warningCount++;
  } else {
    Serial.println("SENS_INIT_I_004");
    ServoSerial.println("SENS_INIT_I_004");
  }

  if(initINA226()){
    Serial.println("SENS_INIT_I_005");
    ServoSerial.println("SENS_INIT_I_005");
  } else {
    Serial.println("SENS_INIT_W_006");
    ServoSerial.println("SENS_INIT_W_006");
    warningCount++;
  }

  if(initICM()){
    Serial.println("SENS_INIT_I_006");
    ServoSerial.println("SENS_INIT_I_006");
  } else {
    Serial.println("SENS_INIT_W_007");
    ServoSerial.println("SENS_INIT_W_007");
    warningCount++;
  }

  Serial.print("SENS_INIT_I_007," + errorCount + ',' + warningCount); //sending info code (indicates that initialization has finished)
}

void loop() {
  // loop that sends sensor data over UART Serial to servo ESP

  //reading front tof sensor and format to [xxxx](mm)
  if(tof_front_bool){   //read data only when initialized successfully
    PCA9548ASelect(tofFront_Address);
    char buffer1[5];
    tof_front_dmm = tofFront.readRangeSingleMillimeters();
    sprintf(buffer1, "%04d", tof_front_dmm);
    tof_front_dmmS = String(buffer1);
  } else{
    tof_front_dmmS = "9999";
  }
  //reading back tof sensor and format to [xxxx](mm)
  if (tof_back_bool) { // read data only when initialized successfully
    PCA9548ASelect(tofBack_Address);
    char buffer2[5];
    tof_back_dmm = tofBack.readRangeSingleMillimeters();
    sprintf(buffer2, "%04d", tof_back_dmm);
    tof_back_dmmS = String(buffer2);
  } else {
    tof_back_dmmS = "9999";
  }

  //reading hc_left ultrasonic sensor and format to [xxxx.xx]
  char buffer3[10];
  hc_left_dmm = getUltrasonicDistance(0);
  dtostrf(hc_left_dmm, 7, 2, buffer3);   
  hc_left_dmmS = String(buffer3);

  //reading hc_right ultrasonic sensor and format to [xxxx.xx]
  char buffer4[10];
  hc_right_dmm = getUltrasonicDistance(1);
  dtostrf(hc_right_dmm, 7, 2, buffer4);
  hc_right_dmmS = String(buffer4);

  //reading hc_frontLeft ultrasonic sensor and format to [xxxx.xx]
  char buffer5[10];
  hc_frontLeft_dmm = getUltrasonicDistance(2);
  dtostrf(hc_frontLeft_dmm, 7, 2, buffer5);
  hc_frontLeft_dmmS = String(buffer5);

  //reading hc_frontRight ultrasonic sensor and format to [xxxx.xx]
  char buffer6[10];
  hc_frontRight_dmm = getUltrasonicDistance(3);
  dtostrf(hc_frontRight_dmm, 7, 2, buffer6);
  hc_frontRight_dmmS = String(buffer6);

  //reading hc_frontDown ultrasonic sensor and format to [xxxx.xx]
  char buffer7[10];
  hc_frontDown_dmm = getUltrasonicDistance(4);
  dtostrf(hc_frontDown_dmm, 7, 2, buffer7);
  hc_frontDown_dmmS = String(buffer7);

  //reading icm angles
  icmAngleStruct icmAngleValues = getICMAnglesDeg();  //get ICM Angles in degrees as struct
  char buffer8[10];
  icm_pitch = icmAngleValues.icm_pitch_struct;
  dtostrf(icm_pitch, 6, 2, buffer8);   //format to [xxx.xx](°)
  icm_pitchS = String(buffer8);
  char buffer9[10];
  icm_roll = icmAngleValues.icm_roll_struct;
  dtostrf(icm_roll, 6, 2, buffer9);    //format to [xxx.xx](°)
  icm_rollS = String(buffer9);

  // code for reading magX, magY, magZ of ICM
  //
  //
  //
  //
  //
  //

  //code for reading temp of ICM
  //
  //
  //
  //

  //reading INA226 voltage vbus
  char buffer10[5];
  ina_vbus = inaGetVBusVolt();
  dtostrf(ina_vbus, 4, 2, buffer10);  //format to [xx.xx](V)
  ina_vbusS = String(buffer10);

  delay(100); //delay for 10Hz update rate
}

// Defining functions



void PCA9548ASelect(int bus){
  Wire.beginTransmission(PCA9548A_Address);
  Wire.write(1 << bus);
  Wire.endTransmission();
}


bool servoUARTValidation(){
  bool validAnswer = false;

  ServoSerial.println(UARTValidationMesg);
  delay(500);

  if(ServoSerial.available()){
    String receivedMesg = ServoSerial.readStringUntil('\n');
    
    if(receivedMesg == UARTValidAnsw){
      Serial.println("SENS_COMU_I_002!");
      validAnswer = true;
    } else {
      Serial.println("SENS_UART_E_003");
      errorCount++;
    }
  } else {
    Serial.println("SENS_UART_E_002");
    errorCount++;
  }

  return validAnswer;
}

initTofBoolStruct initTof(){
  bool tofFront_Init = false;
  bool tofBack_Init = false;

  PCA9548ASelect(tofFront_Bus);   //select front L0X
  if(!tofFront.init()) {
    tofFront_Init = false;
    tof_front_bool = false;
  } else {
    tofFront_Init = true;
    tof_front_bool = true;
  }

  PCA9548ASelect(tofBack_Bus); // select back L0X
  if (!tofBack.init()) {
    tofBack_Init = false;
    tof_back_bool = false;
  } else {
    tofBack_Init = true;
    tof_back_bool = true;
  }

  initTofBoolStruct tofValidBools = {tofFront_Init, tofBack_Init};

  return tofValidBools;
}

bool initINA226(){
  bool ina226_Init = false;

  PCA9548ASelect(ina226_Bus);

  if(!INA.begin()){
    ina226_Init = false;
    ina226_bool = false;
  } else {
    ina226_Init = true;
    ina226_bool = true;
    INA.setMaxCurrentShunt(1, 0.1);
  }

  return ina226_Init;
}

bool initICM(){
  bool ICM_Init = false;

  if(!ICM.begin_I2C()){
    ICM_Init = false;
    icm_bool = false;
  } else {
    ICM_Init = true;
    icm_bool = true;
  }

  return ICM_Init;
}


float getUltrasonicDistance(int sensor){
  float distanceCm = 0;
  float distanceMm = 0;
  long duration = 0;

  switch (sensor){
    case 0:
      digitalWrite(trigSideLeft, LOW);  //set low, to make shure that is low
      delayMicroseconds(2);

      digitalWrite(trigSideLeft, HIGH);   //set high for 10uS to send a pulse
      delayMicroseconds(10);
      digitalWrite(trigSideLeft, LOW);

      duration = pulseIn(echoSideLeft, HIGH);

      distanceCm = duration * SOUND_SPEED / 2;  //calculate distance in cm
      distanceMm = distanceCm * 10;   //convert distance from cm to mm      
      break;
    case 1:
      digitalWrite(trigSideRight, LOW);
      delayMicroseconds(2);

      digitalWrite(trigSideRight, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigSideRight, LOW);

      duration = pulseIn(echoSideRight, HIGH);

      distanceCm = duration * SOUND_SPEED / 2;
      distanceMm = distanceCm * 10;
      break;
    case 2:
      digitalWrite(trigFrontLeft, LOW);
      delayMicroseconds(2);

      digitalWrite(trigFrontLeft, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigFrontLeft, LOW);

      duration = pulseIn(echoFrontLeft, HIGH);

      distanceCm = duration * SOUND_SPEED / 2;
      distanceMm = distanceCm * 10;
      break;
    case 3:
      digitalWrite(trigFrontRight, LOW);
      delayMicroseconds(2);

      digitalWrite(trigFrontRight, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigFrontRight, LOW);

      duration = pulseIn(echoFrontRight, HIGH);

      distanceCm = duration * SOUND_SPEED / 2;
      distanceMm = distanceCm * 10;
      break;
    case 4:
      digitalWrite(trigFrontDown, LOW);
      delayMicroseconds(2);

      digitalWrite(trigFrontDown, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigFrontDown, HIGH);

      duration = pulseIn(echoFrontDown, HIGH);

      distanceCm = duration * SOUND_SPEED / 2;
      distanceMm = distanceCm * 10;
      break;
  }

  return distanceMm;
}

icmAngleStruct getICMAnglesDeg(){
  sensors_event_t accel, gyro, mag, temp;
  ICM.getEvent(&accel, &gyro, &mag, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  float gx = gyro.gyro.x * 180.0 / PI; // Gyro X (deg/s)
  float gy = gyro.gyro.y * 180.0 / PI; // Gyro Y (deg/s)

  // Calculate accelerometer angles
  accel_angle_pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  accel_angle_roll = atan2(ay, az) * 180.0 / PI;

  // Kalman filter prediction step
  pitch_rate = gx - bias_pitch;
  roll_rate = gy - bias_roll;

  gyro_angle_pitch += pitch_rate * dt;
  gyro_angle_roll += roll_rate * dt;

  P[0][0] += dt * (dt * P[1][1] - P[0][1] - P[1][0] + Q_angle);
  P[0][1] -= dt * P[1][1];
  P[1][0] -= dt * P[1][1];
  P[1][1] += Q_bias * dt;

  // Kalman filter update step
  float y_pitch = accel_angle_pitch - gyro_angle_pitch;
  float y_roll = accel_angle_roll - gyro_angle_roll;

  float S = P[0][0] + R_measure; // Innovation covariance
  float K[2];                    // Kalman gain
  K[0] = P[0][0] / S;
  K[1] = P[1][0] / S;

  gyro_angle_pitch += K[0] * y_pitch;
  gyro_angle_roll += K[0] * y_roll;

  bias_pitch += K[1] * y_pitch;
  bias_roll += K[1] * y_roll;

  P[0][0] -= K[0] * P[0][0];
  P[0][1] -= K[0] * P[0][1];
  P[1][0] -= K[1] * P[0][0];
  P[1][1] -= K[1] * P[0][1];

  pitch = gyro_angle_pitch;
  roll = gyro_angle_roll;

  icmAngleStruct angleStruct = {pitch, roll};
  return angleStruct;
}

float inaGetVBusVolt(){
  PCA9548ASelect(INA226_Address);
  float busVoltage = INA.getBusVoltage();
  return busVoltage;
}