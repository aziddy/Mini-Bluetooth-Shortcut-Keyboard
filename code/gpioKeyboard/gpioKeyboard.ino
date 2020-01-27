  /*********************************************************************
 This is an example for our nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
  This example shows how to send HID (keyboard/mouse/etc) data via BLE
  Note that not all devices support BLE keyboard! BLE Keyboard != Bluetooth Keyboard
*/

#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined(ARDUINO_ARCH_SAMD)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#include "Adafruit_BLEBattery.h"
#include "BluefruitConfig.h"
#include "keycode.h"

/*=========================================================================
    APPLICATION SETTINGS

    FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
   
                              Enabling this will put your Bluefruit LE module
                              in a 'known good' state and clear any config
                              data set in previous sketches or projects, so
                              running this at least once is a good idea.
   
                              When deploying your project, however, you will
                              want to disable factory reset by setting this
                              value to 0.  If you are making changes to your
                              Bluefruit LE device via AT commands, and those
                              changes aren't persisting across resets, this
                              is the reason why.  Factory reset will erase
                              the non-volatile memory where config data is
                              stored, setting it back to factory default
                              values.
       
                              Some sketches that require you to bond to a
                              central device (HID mouse, keyboard, etc.)
                              won't work at all with this feature enabled
                              since the factory reset will clear all of the
                              bonding data stored on the chip, meaning the
                              central device won't be able to reconnect.
    MINIMUM_FIRMWARE_VERSION  Minimum firmware version to have some new features
    -----------------------------------------------------------------------*/
    #define FACTORYRESET_ENABLE         0
/*=========================================================================*/



/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

Adafruit_BLEBattery battery(ble);



// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

typedef struct
{
  uint8_t modifier;   /**< Keyboard modifier keys  */
  uint8_t reserved;   /**< Reserved for OEM use, always set to 0. */
  uint8_t keycode[6]; /**< Key codes of the currently pressed keys. */
} hid_keyboard_report_t;

// Report that send to Central every scanning period
hid_keyboard_report_t keyReport = { 0, 0, { 0 } };

// Report sent previously. This is used to prevent sending the same report over time.
// Notes: HID Central intepretes no new report as no changes, which is the same as
// sending very same report multiple times. This will help to reduce traffic especially
// when most of the time there is no keys pressed.
// - Init to different with keyReport
hid_keyboard_report_t previousReport = { 0, 0, { 1 } };

const int E = A5;     // the number of the pushbutton pin
const int PU = A1;     // the number of the pushbutton pin
const int CM2 =  13;     // the number of the pushbutton pin
const int CM1 = 11;     // the number of the pushbutton pin
const int PD = 2;     // the number of the pushbutton pin
const int bracket = 3;     // the number of the pushbutton pin

#define HID_KEY_WINDOWS-CMD_MODIFIER = 0x08; 
#define VBATPIN A9


// GPIO corresponding to HID keycode (not Ancii Character)
int inputPins[6]     = { E        , PU               , CM2        , CM1      , PD       , bracket   };
int inputKeycodes[6] = { HID_KEY_2, HID_KEY_PAGE_DOWN, HID_KEY_5  , HID_KEY_Z, HID_KEY_Z, HID_KEY_4 };

double measuredvbat = 0;
/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  //while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit HID Keyboard Example"));
  Serial.println(F("---------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    ble.factoryReset();
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();



  /* Enable HID Service if not enabled */
  int32_t hid_en = 0;
  
  ble.sendCommandWithIntReply( F("AT+BleHIDEn"), &hid_en);

  if ( !hid_en )
  {
    Serial.println(F("Enable HID Service (including Keyboard): "));
    ble.sendCommandCheckOK(F( "AT+BleHIDEn=On" ));

    /* Add or remove service requires a reset */
    Serial.println(F("Performing a SW reset (service changes require a reset): "));
    !ble.reset();
  }
  
  Serial.println();
  Serial.println(F("Go to your phone's Bluetooth settings to pair your device"));
  Serial.println(F("then open an application that accepts keyboard input"));
  Serial.println();

  // Set up input Pins
  for(int i=0; i< 6; i++)
  {
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  // Enable Battery service and reset Bluefruit
  battery.begin(true);
  
}

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/
void loop(void)
{
  /* scan GPIO, since each report can has up to 6 keys
   * we can just assign a slot in the report for each GPIO 
   */
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  

  battery.update(33);
   
  if ( ble.isConnected() )
  {
    for(int i=0; i<6; i++)
    {
      // GPIO is active low     
      if ( digitalRead(inputPins[i]) == LOW ){
        if(i==3){
          Serial.println(measuredvbat);
          keyReport.modifier = 0b00001010;
        } else if(i==1){
          keyReport.modifier = 0x00;
        }else{
          keyReport.modifier = 0x08;
        }
        keyReport.keycode[i] = inputKeycodes[i];
      }else
      {
        keyReport.modifier = 0x00;
        keyReport.keycode[i] = 0;
      }

      // Only send if it is not the same as previous report
      if ( memcmp(&previousReport, &keyReport, 8) ){
        // Send keyboard report
        ble.atcommand("AT+BLEKEYBOARDCODE", (uint8_t*) &keyReport, 8);
        // copy to previousReport
        memcpy(&previousReport, &keyReport, 8);
      }
    }
  }

  // scaning period is 10 ms
  delay(10);
}

