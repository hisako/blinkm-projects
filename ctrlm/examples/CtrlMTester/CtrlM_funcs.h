/*
 * CtrlM_funcs.h -- Arduino 'library' to control CtrlM
 * -------------
 *
 *
 */

#include "WProgram.h"
#include "wiring.h"
#include "Wire.h"

extern "C" { 
#include "utility/twi.h"  // from Wire library, so we can do bus scanning
}



// Call this first (when powering BlinkM from a power supply)
static void CtrlM_begin()
{
  Wire.begin();                // join i2c bus (address optional for master)
}

//
// each call to twi_writeTo() should return 0 if device is there
// or other value (usually 2) if nothing is at that address
// 
static void CtrlM_scanI2CBus(byte from, byte to, 
                             void(*callback)(byte add, byte result) ) 
{
  byte rc;
  byte data = 0; // not used, just an address to feed to twi_writeTo()
  for( byte addr = from; addr <= to; addr++ ) {
    rc = twi_writeTo(addr, &data, 0, 1);
    callback( addr, rc );
  }
}

//
//
static int8_t CtrlM_findFirstI2CDevice() 
{
  byte rc;
  byte data = 0; // not used, just an address to feed to twi_writeTo()
  for( byte addr=1; addr < 120; addr++ ) {  // only scan addrs 1-120
    rc = twi_writeTo(addr, &data, 0, 1);
    if( rc == 0 ) return addr; // found an address
  }
  return -1; // no device found in range given
}

// FIXME: make this more Arduino-like
static void CtrlM_startPowerWithPins(byte pwrpin, byte gndpin)
{
  DDRC |= _BV(pwrpin) | _BV(gndpin);  // make outputs
  PORTC &=~ _BV(gndpin);
  PORTC |=  _BV(pwrpin);
}

// FIXME: make this more Arduino-like
static void CtrlM_stopPowerWithPins(byte pwrpin, byte gndpin)
{
  DDRC &=~ (_BV(pwrpin) | _BV(gndpin));
}

//
static void CtrlM_startPower()
{
  CtrlM_startPowerWithPins( PORTC3, PORTC2 );
}

//
static void CtrlM_stopPower()
{
  CtrlM_stopPowerWithPins( PORTC3, PORTC2 );
}

// General version of CtrlM_beginWithPower().
// Call this first when CtrlM is plugged directly into Arduino
static void CtrlM_beginWithPowerPins(byte pwrpin, byte gndpin)
{
  CtrlM_startPowerWithPins(pwrpin,gndpin);
  delay(100);  // wait for things to stabilize
  Wire.begin();
}

// Call this first when CtrlM is plugged directly into Arduino
// FIXME: make this more Arduino-like
static void CtrlM_beginWithPower()
{
  CtrlM_beginWithPowerPins( PORTC3, PORTC2 );
}

// sends a generic command
static void CtrlM_sendCmd(byte addr, byte* cmd, int cmdlen)
{
  Wire.beginTransmission(addr);
  for( byte i=0; i<cmdlen; i++) 
    Wire.send(cmd[i]);
  Wire.endTransmission();
}

// receives generic data
// returns 0 on success, and -1 if no data available
// note: responsiblity of caller to know how many bytes to expect
static int CtrlM_receiveBytes(byte addr, byte* resp, byte len)
{
  Wire.requestFrom(addr, len);
  if( Wire.available() ) {
    for( int i=0; i<len; i++) 
      resp[i] = Wire.receive();
    return 0;
  }
  return -1;
}

// Sets the I2C address of the CtrlM.  
// Uses "general call" broadcast address
static void CtrlM_setAddress(byte newaddress)
{
  Wire.beginTransmission(0x00);  // general call (broadcast address)
  Wire.send('A');
  Wire.send(newaddress);
  Wire.send(0xD0);
  Wire.send(0x0D);  // dood!
  Wire.send(newaddress);
  Wire.endTransmission();
  delay(50); // just in case
}


// Gets the I2C address of the BlinKM
// Kind of redundant when sent to a specific address
// but uses to verify CtrlM communication
static int CtrlM_getAddress(byte addr)
{
  Wire.beginTransmission(addr);
  Wire.send('a');
  Wire.endTransmission();
  Wire.requestFrom(addr, (byte)1);  // general call
  if( Wire.available() ) {
    byte b = Wire.receive();
    return b;
  }
  return -1;
}

// Gets the CtrlM firmware version
static int CtrlM_getVersion(byte addr)
{
  Wire.beginTransmission(addr);
  Wire.send('Z');
  Wire.endTransmission();
  Wire.requestFrom(addr, (byte)2);
  if( Wire.available() ) {
    byte major_ver = Wire.receive();
    byte minor_ver = Wire.receive();
    return (major_ver<<8) + minor_ver;
  }
  return -1;
}

// Demonstrates how to verify you're talking to a CtrlM 
// and that you know its address
static int CtrlM_checkAddress(byte addr)
{
  //Serial.print("Checking CtrlM address...");
  int b = CtrlM_getAddress(addr);
  if( b==-1 ) {
    //Serial.println("No response, that's not good");
    return -1;  // no response
  } 
  //Serial.print("received addr: 0x");
  //Serial.print(b,HEX);
  if( b != addr )
    return 1; // error, addr mismatch 
  else 
    return 0; // match, everything okay
}

//
static void CtrlM_setSendAddress( byte addr, byte freem_addr, byte blinkm_addr)
{
  //{'@', i2c_addr, freem_addr, 0 }  -- set i2c addr & freem addr to send to
  Wire.beginTransmission(addr);
  Wire.send('@');
  Wire.send( freem_addr );
  Wire.send( blinkm_addr);
  Wire.send(0);
  Wire.endTransmission();  
}

//
static void CtrlM_writeFreeMAddress(byte addr, byte freem_addr )
{
  byte chksum = 0x55 + freem_addr + 0xff + 0xff + 0x00 + 0x00 + 0x00;
  Wire.beginTransmission(addr);
  Wire.send('!');
  Wire.send( 0x55 );       // byte0, start byte
  Wire.send( freem_addr ); // byte1, freem_addr (new addr)
  Wire.send( 0xff );       // byte2, blinkm_addr
  Wire.send( 0xff );       // byte3, blinkm_cmd
  Wire.send( 0x00 );       // byte4, blinkm_arg1
  Wire.send( 0x00 );       // byte5, blinkm_arg2
  Wire.send( 0x00 );       // byte6, blinkm_arg3
  Wire.send( chksum );     // byte7, checksum of bytes 0-6
  Wire.endTransmission();

}

// Set IR frequency value
static void CtrlM_setIRFreq(byte addr, uint16_t irfreq, byte duty_percent)
{
  Wire.beginTransmission(addr);
  Wire.send('#');
  Wire.send( (byte)(irfreq >> 8) );   // msb
  Wire.send( (byte)(irfreq & 0xff) ); // lsb
  Wire.send( duty_percent );
  Wire.endTransmission();  
}

static void CtrlM_turnIRLED(byte addr, byte on)
{
  Wire.beginTransmission(addr);
  Wire.send('%');
  Wire.send( on ); 
  Wire.send( 0 ); 
  Wire.send( 0 ); 
  Wire.endTransmission();  
}

static void CtrlM_sendIRCode( byte addr, uint8_t codetype, uint32_t code )
{
  Wire.beginTransmission(addr);
  Wire.send('$');
  Wire.send( codetype );
  Wire.send( (byte)((code >>24) & 0xff) );  // upper byte  
  Wire.send( (byte)((code >>16) & 0xff) );  // 
  Wire.send( (byte)((code >> 8) & 0xff) );  // 
  Wire.send( (byte)((code >> 0) & 0xff) );  // lower byte
  Wire.endTransmission();
}

// Sets the speed of fading between colors.  
// Higher numbers means faster fading, 255 == instantaneous fading
static void CtrlM_setFadeSpeed(byte addr, byte fadespeed)
{
  Wire.beginTransmission(addr);
  Wire.send('f');
  Wire.send(fadespeed);
  Wire.endTransmission();  
}

// Sets the light script playback time adjust
// The timeadj argument is signed, and is an additive value to all
// durations in a light script. Set to zero to turn off time adjust.
static void CtrlM_setTimeAdj(byte addr, byte timeadj)
{
  Wire.beginTransmission(addr);
  Wire.send('t');
  Wire.send(timeadj);
  Wire.endTransmission();  
}

// Fades to an RGB color
static void CtrlM_fadeToRGB(byte addr, byte red, byte grn, byte blu)
{
  Wire.beginTransmission(addr);
  Wire.send('c');
  Wire.send(red);
  Wire.send(grn);
  Wire.send(blu);
  Wire.endTransmission();
}

// Fades to an HSB color
static void CtrlM_fadeToHSB(byte addr, byte hue, byte saturation, byte brightness)
{
  Wire.beginTransmission(addr);
  Wire.send('h');
  Wire.send(hue);
  Wire.send(saturation);
  Wire.send(brightness);
  Wire.endTransmission();
}

// Sets an RGB color immediately
static void CtrlM_setRGB(byte addr, byte red, byte grn, byte blu)
{
  Wire.beginTransmission(addr);
  Wire.send('n');
  Wire.send(red);
  Wire.send(grn);
  Wire.send(blu);
  Wire.endTransmission();
}

// Fades to a random RGB color
static void CtrlM_fadeToRandomRGB(byte addr, byte rrnd, byte grnd, byte brnd)
{
  Wire.beginTransmission(addr);
  Wire.send('C');
  Wire.send(rrnd);
  Wire.send(grnd);
  Wire.send(brnd);
  Wire.endTransmission();
}
// Fades to a random HSB color
static void CtrlM_fadeToRandomHSB(byte addr, byte hrnd, byte srnd, byte brnd)
{
  Wire.beginTransmission(addr);
  Wire.send('H');
  Wire.send(hrnd);
  Wire.send(srnd);
  Wire.send(brnd);
  Wire.endTransmission();
}

//
static void CtrlM_getRGBColor(byte addr, byte* r, byte* g, byte* b)
{
  Wire.beginTransmission(addr);
  Wire.send('g');
  Wire.endTransmission();
  Wire.requestFrom(addr, (byte)3);
  if( Wire.available() ) {
    *r = Wire.receive();
    *g = Wire.receive();
    *b = Wire.receive();
  }
}

//
static void CtrlM_stopScript(byte addr)
{
  Wire.beginTransmission(addr);
  Wire.send('o');
  Wire.endTransmission();
}

//
static void CtrlM_playScript(byte addr, byte script_id, byte reps, byte pos)
{
  Wire.beginTransmission(addr);
  Wire.send('p');
  Wire.send(script_id);
  Wire.send(reps);
  Wire.send(pos);
  Wire.endTransmission();
}

static void CtrlM_stopCtrlMScript(byte addr)
{
  Wire.beginTransmission(addr);
  Wire.send('O');
  Wire.endTransmission();
}
static void CtrlM_playCtrlMScript(byte addr, byte reps, byte pos)
{
  Wire.beginTransmission(addr);
  Wire.send('P');
  Wire.send(0);
  Wire.send(reps);
  Wire.send(pos);
  Wire.endTransmission();
}


//
static void CtrlM_setStartupParams(byte addr, byte mode, byte script_id,
                                    byte reps, byte fadespeed, byte timeadj)
{
  Wire.beginTransmission(addr);
  Wire.send('B');
  Wire.send(mode);             // default 0x01 == Play script
  Wire.send(script_id);        // default 0x00 == script #0
  Wire.send(reps);             // default 0x00 == repeat infinitely
  Wire.send(fadespeed);        // default 0x08 == usually overridden by sketch 
  Wire.send(timeadj);          // default 0x00 == sometimes overridden by sketch
  Wire.endTransmission();
} 


// Gets digital inputs of the CtrlM
// returns -1 on failure
static int CtrlM_getInputsO(byte addr)
{
  Wire.beginTransmission(addr);
  Wire.send('i');
  Wire.endTransmission();
  Wire.requestFrom(addr, (byte)1);
  if( Wire.available() ) {
    byte b = Wire.receive();
    return b; 
  }
  return -1;
}

// Gets digital inputs of the CtrlM
// stores them in passed in array
// returns -1 on failure
static int CtrlM_getInputs(byte addr, byte inputs[])
{
  Wire.beginTransmission(addr);
  Wire.send('i');
  Wire.endTransmission();
  Wire.requestFrom(addr, (byte)4);
  while( Wire.available() < 4 ) ; // FIXME: wait until we get 4 bytes
    
  inputs[0] = Wire.receive();
  inputs[1] = Wire.receive();
  inputs[2] = Wire.receive();
  inputs[3] = Wire.receive();

  return 0;
}

