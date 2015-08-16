#include <Arduino.h>
#include "SoftwareSerial_fix.h"
#include "MeUsb.h"

#define MOTOR_1 1
#define MOTOR_2 2

MeUsb usb(10,9);

void setup() 
{
   Serial.begin(9600); 
   usb.init(USB1_0);
}

void loop()
{
  if(!usb.device_online)
  {
    usb.probeDevice(); 
    delay(100);
  }
  else
  {
    int len = usb.host_recv();
    if(len>4){
      parseJoystick(usb.RECV_BUFFER);
    }
  }
}
void parseJoystick(unsigned char * buf)
{
  uint8_t buttonCode = buf[4]&0xff;
  uint8_t buttonCode_ext = buf[5]&0xff;
  uint8_t joystickCodeL_V = buf[3]&0xff; //top 0 bottom ff
  uint8_t joystickCodeL_H = buf[2]&0xff; //left 0 right ff
  uint8_t joystickCodeR_V = buf[1]&0xff; //top 0 bottom ff
  uint8_t joystickCodeR_H = buf[0]&0xff; //left 0 right ff
  uint8_t directionButtonCode = (buttonCode&0xf);
  uint8_t rightButtonCode = (buttonCode&0xf0)>>4;
  switch(directionButtonCode){
    case 0:{
      //up
      runMotor(MOTOR_1,100);
      runMotor(MOTOR_2,-100);
      break;
    }
    case 1:{
      //up right
      break;
    }
    case 2:{
      //right
      runMotor(MOTOR_1,-100);
      runMotor(MOTOR_2,-100);
      break;
    }
    case 3:{
      //down right
      break;
    }
    case 4:{
      //down
      runMotor(MOTOR_1,-100);
      runMotor(MOTOR_2,100);
      break;
    }
    case 5:{
      //down left
      break;
    }
    case 6:{
      //left
      runMotor(MOTOR_1,100);
      runMotor(MOTOR_2,100);
      break;
    }
    case 7:{
      //up left
      break;
    }
    default:{
      // release;
      runMotor(MOTOR_1,0);
      runMotor(MOTOR_2,0);
    }
  }
  Serial.println(directionButtonCode);
}
void runMotor(int motor,int speed){
  int _dirPin;
  int _pwmPin;
  if(motor==MOTOR_1){
    _dirPin = 7;
    _pwmPin = 6;
  }else if(motor==MOTOR_2){
    _dirPin = 4;
    _pwmPin = 5;
  }
  pinMode(_dirPin,OUTPUT);
  pinMode(_pwmPin,OUTPUT);
  digitalWrite(_dirPin,speed>0?HIGH:LOW);
  analogWrite(_pwmPin,speed>0?speed:-speed);
}

