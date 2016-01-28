#include "PS2X_lib.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include "pins_arduino.h"



static byte enter_config[]={0x01,0x43,0x00,0x01,0x00};
static byte set_mode[]={0x01,0x44,0x00,0x01,0x03,0x00,0x00,0x00,0x00};
static byte set_bytes_large[]={0x01,0x4F,0x00,0xFF,0xFF,0x03,0x00,0x00,0x00};
static byte exit_config[]={0x01,0x43,0x00,0x00,0x5A,0x5A,0x5A,0x5A,0x5A};
static byte enable_rumble[]={0x01,0x4D,0x00,0x00,0x01};

boolean PS2X::NewButtonState() {
   return ((last_buttons ^ buttons) > 0);

}

boolean PS2X::NewButtonState(unsigned int button) {
  return (((last_buttons ^ buttons) & button) > 0);
}

boolean PS2X::ButtonPressed(unsigned int button) {
  return(NewButtonState(button) & Button(button));
}

boolean PS2X::ButtonReleased(unsigned int button) {
  return((NewButtonState(button)) & ((~last_buttons & button) > 0));
}
  
boolean PS2X::Button(uint16_t button) {
   return ((~buttons & button) > 0);
}

unsigned int PS2X::ButtonDataByte() {
   return (~buttons);
}

byte PS2X::Analog(byte button) {
  return PS2data[button];
}
unsigned char PS2X::_gamepad_shiftinout (char byte) {
   unsigned char tmp = 0;
   for(i=0;i<8;i++) {
	if(CHK(byte,i)) SET(*_cmd_oreg,_cmd_mask);
	  else  CLR(*_cmd_oreg,_cmd_mask);
	CLR(*_clk_oreg,_clk_mask);
	delayMicroseconds(CTRL_CLK);
	  if(CHK(*_dat_ireg,_dat_mask)) SET(tmp,i);
	SET(*_clk_oreg,_clk_mask);
   }
   SET(*_cmd_oreg,_cmd_mask);
   delayMicroseconds(CTRL_BYTE_DELAY);
   return tmp;
}

void PS2X::read_gamepad() {
    read_gamepad(false, 0x00);
}


void PS2X::read_gamepad(boolean motor1, byte motor2) {
  double temp = millis() - last_read;
  
  if (temp > 1500) //waited to long
    reconfig_gamepad();
    
  if(temp < read_delay)  //waited too short
    delay(read_delay - temp);
    
    
  last_buttons = buttons; //store the previous buttons states

if(motor2 != 0x00)
  motor2 = map(motor2,0,255,0x40,0xFF); //noting below 40 will make it spin
    
    SET(*_cmd_oreg,_cmd_mask);
    SET(*_clk_oreg,_clk_mask);
    CLR(*_att_oreg,_att_mask); // low enable joystick
    delayMicroseconds(CTRL_BYTE_DELAY);
    //Send the command to send button and joystick data;
    char dword[9] = {0x01,0x42,0,motor1,motor2,0,0,0,0};
    byte dword2[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	#ifdef PS2X_COM_DEBUG
    Serial.println("OUT:IN");
	#endif
    for (int i = 0; i<9; i++) {
	  PS2data[i] = _gamepad_shiftinout(dword[i]);
	  #ifdef PS2X_COM_DEBUG
		Serial.print(dword[i], HEX);
		Serial.print(":");
		Serial.println(PS2data[i], HEX);
	  #endif
    }
    if(PS2data[1] == 0x79) {  //if controller is in full data return mode, get the rest of data
       for (int i = 0; i<12; i++) {
	  PS2data[i+9] = _gamepad_shiftinout(dword2[i]);
	  #ifdef PS2X_COM_DEBUG
		Serial.print(dword[i], HEX);
		Serial.print(":");
		Serial.println(PS2data[i], HEX);
	  #endif
       }
    }
    
    SET(*_att_oreg,_att_mask); // HI disable joystick
    
   buttons = *(uint16_t*)(PS2data+3);   //store as one value for multiple functions
   last_read = millis();
}

byte PS2X::config_gamepad(uint8_t clk, uint8_t cmd, uint8_t att, uint8_t dat) {
  
 _clk_mask = maskToBitNum(digitalPinToBitMask(clk));
 _clk_oreg = portOutputRegister(digitalPinToPort(clk));
 _cmd_mask = maskToBitNum(digitalPinToBitMask(cmd));
 _cmd_oreg = portOutputRegister(digitalPinToPort(cmd));
 _att_mask = maskToBitNum(digitalPinToBitMask(att));
 _att_oreg = portOutputRegister(digitalPinToPort(att));
 _dat_mask = maskToBitNum(digitalPinToBitMask(dat));
 _dat_ireg = portInputRegister(digitalPinToPort(dat));
  

  pinMode(clk, OUTPUT); //configure ports
  pinMode(att, OUTPUT);
  pinMode(cmd, OUTPUT);
  pinMode(dat, INPUT);

  digitalWrite(dat, HIGH); //enable pull-up 
    
   SET(*_cmd_oreg,_cmd_mask); // SET(*_cmd_oreg,_cmd_mask);
   SET(*_clk_oreg,_clk_mask);
   
   //new error checking. First, read gamepad a few times to see if it's talking
   read_gamepad();
   read_gamepad();
   
   //see if it talked
   if(PS2data[1] != 0x41 && PS2data[1] != 0x73 && PS2data[1] != 0x79){ //see if mode came back. If still anything but 41, 73 or 79, then it's not talking
      #ifdef PS2X_DEBUG
		Serial.println("Controller mode not matched or no controller found");
		Serial.print("Expected 0x41 or 0x73, got ");
		Serial.println(PS2data[1], HEX);
	  #endif
	 
	 return 1; //return error code 1
	}
  
  //try setting mode, increasing delays if need be. 
  
  for(int y = 0; y <= 10; y++)
  {
   sendCommandString(enter_config, sizeof(enter_config));
   sendCommandString(set_mode, sizeof(set_mode));
   sendCommandString(exit_config, sizeof(exit_config));
   
   read_gamepad();
   
    if(PS2data[1] == 0x73)
      break;
      
    if(y == 10){
		#ifdef PS2X_DEBUG
		Serial.println("Controller not accepting commands");
		Serial.print("mode stil set at");
		Serial.println(PS2data[1], HEX);
		#endif
      return 2; //exit function with error
	  }
    
    read_delay += 10; //add 10ms to read_delay
  }
   
 return 0; //no error if here
}

void PS2X::sendCommandString(byte string[], byte len) {
  
   CLR(*_att_oreg,_att_mask); // low enable joystick
  for (int y=0; y < len; y++)
    _gamepad_shiftinout(string[y]);
    
   SET(*_att_oreg,_att_mask); //high disable joystick  
   delay(read_delay);                  //wait a few
}

 
uint8_t PS2X::maskToBitNum(uint8_t mask) {
    for (int y = 0; y < 8; y++)
    {
      if(CHK(mask,y))
        return y;
    }
    return 0;
}

void PS2X::enableRumble() {
  
     sendCommandString(enter_config, sizeof(enter_config));
     sendCommandString(enable_rumble, sizeof(enable_rumble));
     sendCommandString(exit_config, sizeof(exit_config));
     en_Rumble = true;
  
}

void PS2X::enablePressures() {
  
     sendCommandString(enter_config, sizeof(enter_config));
     sendCommandString(set_bytes_large, sizeof(set_bytes_large));
     sendCommandString(exit_config, sizeof(exit_config));
     en_Pressures = true;
}

void PS2X::reconfig_gamepad(){
  
   sendCommandString(enter_config, sizeof(enter_config));
   sendCommandString(set_mode, sizeof(set_mode));
   if (en_Rumble)
      sendCommandString(enable_rumble, sizeof(enable_rumble));
   if (en_Pressures)
      sendCommandString(set_bytes_large, sizeof(set_bytes_large));
   sendCommandString(exit_config, sizeof(exit_config));
   
}
