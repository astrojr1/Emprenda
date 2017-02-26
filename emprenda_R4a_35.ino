/*
  Effigy Labs
  for the Emprenda MIDI pitch bend pedal interface
  (C)2012-2017, Effigy Labs
  PUBLISHED AS FREE OPEN SOURCE SOFTWARE (FOSS) - see LICENSE.TXT
  Author: Jody Roberts

Disclaimers to those brave enough to try to use this software:
This program is primitive.  Do not expect all the correct techniques to be used.
I have certainly re-invented much needlessly.
Please share changes and improvements with the Community.
I am happy to discuss anything, for now, at Jody@effigylabs.com.
We'll have a web site and community forum soon.
Thanks
JR



  
  updates:
  Dec 13 2015 - for #32
  Aug 26 2016 - for #33 using R4a pcb
  Nov 28 2016 - for #34 concept
  Dec 14 2016 - for #34 working
  Feb 08 2017 - for #34b working 
  Feb 26 2017 - for #35-29 ROF / FOSS / USB VID/PID
   
  02/08/17 - updates:
  - remove OMNI broadcast
  - ensure sustain threahold to natural
  
  12/14/16 - updates: 
    fixed superfluous sensitivity traffic - so not re-sending sustain on or off after it is already on or off
    OTAs are .75" so maybe different performance curve - 
    

  08-26-16 updates:
    Now on ATMEGA32u4 mpu
    no separate usb chip
    separate serial (serial1) port from USB
    can now send actual MIDI out all the time regardless of debug mode
    pin mappings are different for most of the input and output circuits
    new quieter circuit? mo betta caps keep it quieter

    12-13-15 updates:
   fadespeed changed and moved up front
   sensitivity knob max var moved up front
   midi preset section moved up front
   all up-front moves can be generated in one block of code (one file / include /etc)

    This program must be compiled as a TeeOnArdu for the pin mappings to work.  On the Teensy 2.0:
    a0 = Sensitivty knob
    a1 = OTA3
    a2 = OTA2
    a3 = OTA1
   but on the Leonardo,
   A3 = Sensitivity knob,
   A2 = OTA3
   A1 = OTA2
   A0 = OTA1
   so the pin mappings will not work interchangeably.

   dummy pin - to avoid cross-talk influence between the last OTA read and the sensitivity knob.
   
  TO-DO:
    - program rewrite
    - use bounce objcts for de-jittering
    - MIDI command input
    x redo of debugging
    x change pin mappings
    x fix bug on sensitivity knob range so knob has full range of function in entire range of motion
    - fix phasing/LED
    - EEPROM - what to remember and where
        remembering of states, modes, and presets
        - MIDI channel
        - mode 1, mode 2, mode 3 settings
        - presets settings
        - whatever else is storable and retrievable in 1K
        - sensitivity knob setting
        - sensitivity knob and other settings override on manual touch
        - calibration results from previous calibrations 
    - mappings of pins early and late
    - fuller implementation of keeping the columns in the OTA updated, active, etc. for later use

  operation mode presets
  preset modes:  L - R - C
  mode 1: pitchbend(0+1) + mod
  mode 2: pitchbend(0+1) + breath
  mode 3: mod, expression, and breath

  sensor input operating range - 50-950  for phototransistors - #20+
                   50-600 for photoresistors     #19 and earlier
                   20-1023 for photoresisters in straight OTAs #30+

  updated max phase speeed to something that will flash reliably
  max sensitivity knob pot = 512

  main flow:

  
   setup:
   - initialize variables
   - calibrate OTAs and initialize OTA array
   - initialize sensitivity
   - select mode and enter main loop

   main loop:
   - if mode3, go directly to mode 3 processing
   - otherwise, process mode 1 or 2 which is pitch bend + a third middle function
   - in mode 1 or 2:
       - if either sensor is active, retrieve its value and process
       - otherwise, enter inner loop and wait until a sensor becomes active
         - while waiting, sensitivity knob and process the middle OTA
  - when processing an OTA:
      - check for jitter
      - check for centering
      - check for maxing
      - if none of these:
          - determine if a message should be sent:
              - if the OTAs mode is switched (on/off only):
                - if the sensor isn't already "on", process the message
              - otherwise, if the OTA mode is not switched, process the message
   spec:
   restore factory defaults
   change midi channel but do not save
   save midi channel to memory
   restore midi channel from memory
   define 3 modes
   define each mode
   define what each OTA does in a mode

   1-16 - change MIDI channel
   17 - save current midi channel to boot
   18 - restore saved midi channel from memory
   19 - save current sensitivity knob to boot
   20 - restore saved sensitivity knob from memory
   21 - save current mode to boot incl sensitivity knob settings and all function-specific values: threshold, .....
   22 -
   1 - go to mode 1
   2 - go to mode 2
   3 - go to mode 3
   4 - set preboot to mode 1
   5 - set preboot to mode 2
   6 - set preboot to mode 3
   7 -

     - ledFadeBrightness max brightness ****** 090816

   defining presets
   set mode 1 to be: OTA1+OTA2+OTA3 settings, OTA setting = function, threshold, lower range, upper range = 6 bytes x 3 = 18 bytes per ota, 54 per mode
   set mode 2 to be:
   set mode 3 to be:
*/

//#include <EEPROM.h>
#include <EEPROMex.h>
#include <EEPROMVar.h>

//#include <BOUNCE.h>

//////////////////////////DEBUG SECTION/////////////////////////////////////////////////////////
//comment out this for real operation when switching to MIDI device type enumeration on USB   //
//#define DEBUG                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////

// ****** commonly changed variables ****** - preset code section begin

// use this structure to include code that compiles only in non-debug mode i.e. midiSerial.* where the USB_MIDI must be defined in the environment
#ifdef DEBUG
int EffigyLabsDebuggerVersion = 3;
// enable debug output with dp and dpl functions - NOTE this code will not work here because you are not in setup() or loop()!
//dpl("Effigy Labs Debugger v3"); // include debugger printing etc. here
#endif

#ifndef DEBUG
// include midiSerial.*, etc. statements here      // enable debug output with dp and dpl functions
#endif

//boolean debug = false; // disable debug output and send real MIDI
///////////////////////////////////////////////////////////////


int nop; // for no op logic when debug include makes nothing and you must do something in  between an if and else.
int mode = 0;                         // 0 = mode is initially unset, or if mode = a supported mode, pedal enters that mode after calibration without interaction

// MIDI

int const MIDI_OMNI_MODE_ON = 0x7D;
int const MIDI_OMNI_MODE_OFF = 0x7C;
int CURRENT_MIDI_CHANNEL = 1;
int address = 0;


// channel 1 codes
int const MIDI_CHG = 0xB0;            // midi change command channel 1 dec 176
int const MIDI_SUSTAIN = 0x40;        // midi sustain channel 1, dec 64
int const MIDI_MOD_CMD = 0x01;        // midi MOD wheel control - CC1

int const MIDI_BREATH_CMD = 0x02;     // midi BREATH control - CC2
int const MIDI_XPRS_CMD = 0x0B;       // midi EXPRESSION control - CC7
int const MIDI_SUSTAINON_CMD = 0x7F;  // midi value for sustain on (>64)
int const MIDI_SUSTAINOFF_CMD = 0x00; // midi value for sustain off (<64)

/*
  future
  int const MIDI_CC3_CMD = 0x03; // CC3
  int const MIDI_PAN_CMD =  // CC
  int const MIDI_REVERB_CMD =  // CC91
  int const MIDI_PORTAMENTO_CMD = 0x05; // CC5
  int const MIDI_CHORUS_CMD = // CC93
  int const MIDI_HARMONIC_CMD = // CC
  int const MIDI_RELEASE_CMD = // CC72
  int const MIDI_VOLUME_CMD = 0x07; // CC7
  int const MIDI_ATTACK_CMD = // CC73
  int const MIDI_BRIGHT_CMD = // CC74
  int const MIDI_PHASER_CMD = // CC95
  int const MIDI_CELESTE_CMD = // CC94
  int const MIDI_EFFECTS_CMD = // CC91
  int const MIDI_FOOTPEDAL_CMD = 0x04; // CC4 (MSB)
*/


int const PITCH_DOWN = 0x111; // Effigy reference code
int const PITCH_UP = 0x112;   // Effigy reference code

// user-configurable section
// supply configurations for a preset here.
// prototypical - not implemented 3/30/2014 - JR
// this gets implemented in the eeprom - 12/14/16 - JR

// default preset 1
int mode_1_1 = PITCH_DOWN;      // mode 1, OTA 1
int mode_1_1_min = 0x0000;      // mode 1 OTA 1 min - NOTE: must agree with mode or pedal will not function!
int mode_1_1_max = 0x2000;      // mode 1 OTA 1 min - NOTE: must agree with mode or pedal will not function!
int mode_1_2 = PITCH_UP;        // mode 1, OTA 2
int mode_1_2_min = 0x2000;      // mode 1 OTA 2 min - NOTE: must agree with mode or pedal will not function!
int mode_1_2_max = 0x3FFF;      // mode 1 OTA 2 min - NOTE: must agree with mode or pedal will not function!
int mode_1_3 = MIDI_SUSTAIN;    // mode 1, OTA 3
int mode_1_3_min = 0;           // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function! for an on-off function like sustain, use 0-127 for min-max
int mode_1_3_max = 127;         // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function!

int mode_2_1 = PITCH_DOWN;      // mode 2, OTA 1
int mode_2_1_min = 0x0000;      // mode 2 OTA 1 min - NOTE: must agree with mode or pedal will not function!
int mode_2_1_max = 0x2000;      // mode 2 OTA 1 min - NOTE: must agree with mode or pedal will not function!
int mode_2_2 = PITCH_UP;        // mode 2, OTA 2
int mode_2_2_min = 0x2000;      // mode 2 OTA 2 min - NOTE: must agree with mode or pedal will not function!
int mode_2_2_max = 0x3FFF;      // mode 2 OTA 2 min - NOTE: must agree with mode or pedal will not function!
int mode_2_3 = MIDI_MOD_CMD;    // mode 2, OTA 3
int mode_2_3_min = 0;           // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function! for an on-off function like sustain, use 0-127 for min-max
int mode_2_3_max = 127;         // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function!

int mode_3_1 = MIDI_MOD_CMD;    // mode 3, OTA 1
int mode_3_1_min = 0;           // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function! for an on-off function like sustain, use 0-127 for min-max
int mode_3_1_max = 127;         // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function!
int mode_3_2 = MIDI_BREATH_CMD; // mode 3. OTA 2
int mode_3_2_min = 0;           // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function! for an on-off function like sustain, use 0-127 for min-max
int mode_3_2_max = 127;         // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function!
int mode_3_3 = MIDI_XPRS_CMD;   // mode 3, OTA 3
int mode_3_3_min = 0;           // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function! for an on-off function like sustain, use 0-127 for min-max
int mode_3_3_max = 127;         // mode 1 OTA 3 min - NOTE: must agree with mode or pedal will not function!

// ****** commonly changed variables ****** - preset code section end

// global variables

int j = 0; // counter
int working_range = 0; //established at calibration time as the upper limit minus the maximum lower limit of all OTAs

// OTA Array
const int numberofOTAs = 3; // set the number of OTAs on the device
int    OTAs[numberofOTAs][12]; // second number is the number of columns or global variables we have at this point in time
float  OTAscaleratios[numberofOTAs];
int maxLowerLimit = 0;

// saved last values to prevent resends of same MIDI commands
int lastCmdType = 9999;
int lastCmdSubType = 9999;
int lastValue = 9999;
int lastPitchValue = 0x2000;

// named values for the columns in our table
const static int OTA_COL = 0;              // Pin mapping to atmega328 analog pins
const static int LOWERLIMIT_COL = 1;       // lower limit
const static int UPPERLIMIT_COL = 2;       // upper limit
const static int open_gate_COL = 3;         // currently open_gate?
const static int full_gate_COL = 4;            // currently full_gate?
const static int VALUE_COL = 5;            // current value measured
const static int LASTSENSORVALUE_COL = 6;  // last value measured
const static int WORKINGRANGE_COL = 7;     // set of messageable values
const static int SWITCH_COL = 8;           // if true, OTA operates on on-off only
const static int MIDI_CMD_COL = 9;         // the function assigned to this ota
const static int SENSITIVITY_COL = 10;     // current sensitivity of this ota
const static int ACTIVE_COL = 11;          // current sensitivity of this ota
//const static int WHATEVER_COL = 12;          // another column of OTA values

/* left-right locking:

  if the left or right OTA needs to lock out the other,
  it sets this and the other OTA will not proces in processOTA().

  if the OTA needs to exclude both other OTAs no matter what, it sets ALLOCK.  LRLOCK still allows the middle OTA (2) to process.  this is useful with pitch bend.

*/
int LRLOCK = 0;   // 0=not locked, 1 = left locked, 2 = right locked
boolean ALLOCK = false;

// generic for different modes
// pin mapping

// array reference to the OTA array
int left_ota = 0;
int right_ota = 1;
int middle_ota = 2;

// pin maps.  the An statics map to the pin numbers on the teensy 2.0
int OTA1 = A0;  //left/down
int OTA2 = A1;  //right/up
int OTA3 = A2;  //middle/effect
int sensorPin = OTA1;            // set to either the left or right sensor by the locker in LR mode - default = left or OTA1

const int numberOfSamples = 2000; // number of calibration samples to average, 
// currently at 2000 samples, approximately 1 second of clock time.  this is bypassed during mode switch, currently calibration only happens at setup.

int sensorPinActive = 0;    // which pedal is active; 1 = down, 2 = up, 0 = neither

// LED outputs
int pedalLedPin = 2;      // emitters
int commLedPin = 3;      // comm LED
int fadeLedPin = 6;      // fade LED, adds base current at variable brightness via pwm to let the comm led phase in an adjustable range off of black at the bottom
// allow analogwrite

//int faceboostPin = 9;     // controllable phase to allow mid-level phase blinking - not too dim or too bright

// LED blink parameters
int delayInterval = 0;             // time to wait between samples
long modeselectblinkinterval = 100; // indicator of mode cycle
int modeconfirmblinkspeed = 500; // first time is 1/2 second, after mode switch is very fast like 40
long setupblinkinterval = 250;

unsigned long  timer2 = 0;  // long-term timer for mode switch 2-second depress
long timer2_threshold=2000; //ms

// this provides a heartbeat phased blink effect for the ledon pin 9, so you can tell the software is running.
uint8_t hbval = 128;
int8_t hbdelta = 8;
int fadeSpeedMax = 40;
int fadeSpeedMin = 8;
int fadespeed = 40; // vary from 10 to 100 - when centered, cycle according to sensitivity.  when pedal is operating, vary according to OTA position.
// 40 turns out to be the max not 80 as was before
unsigned long cyclestart; // led fader timer
/*
  no-delay delay - bump every this many milliseconds
  - this equates to almost 18 frames per second,
  the min we can get away with: 1000ms / 18 (fps) = 55... so 60 is very slightly slower
*/
int delayint = 60;

//int fademax = 242;           // maximum brightness to output
//int fademin = 32;            // minimum brightness to output
int fademax = 200;           // maximum brightness to output
int fademin = 0;             // minimum brightness to output
int ledFadeBrightness = 200; // default value: start "on"    ******** fix this and other parts of this for proper phasing

// sensitivity knob input
int sensitivityKnobPin = A3;          // sensitivity knob input pin # for #33+ on atmega32u4 compiling as a teeonardu

int dummypin = A4;                    // unused pin.  read before reading the knob value.  this is necessary to discharge the hold/sample capacitor in the chip to get an 
                                      // accurate reading.  If this is not done, the knob value will be influenced by the last value read from the last OTA.  In this case, 
                                      // OTA3.  Prior to this, the knob will return a wrong reading proportional to the OTA3 
                                      // value (data and graph shown in spreadsheet test_log_#34_121416_5.xlsx

int sensitivityKnobMinValue = 0;      // knob's minimum reading
int sensitivityKnobValue = 255;       // default value = half-way
int sensitivityKnobMaxValue = 510;    // #34's maximum knob reading

int knobjitter = 4;                   // variance of sensitivity knob - to 4 for #34 12/24/16

//int knobjitter = 3;                   // variance of sensitivity knob - pped from 3 to 10 for #32
//int knobjitter = 3;                   // variance of sensitivity knob - pped from 3 to 10 for #32
                                      // set to 6 for #33 - sep 1 2016 - 3 expressed jitter in the middle of the range
                                      // set back to 3 for #34 after fixing the knob cross-talk issue between ota3 and the knob reading - idles nice and tight now

int lastSensitivityKnobValue = 9999;  // sensitivity change detection
int tripp = 0;                        // keeps track of min sens knob raw value
int trigger = 3;                      // keeps track of min sens knob raw value
int sensmintune = 999;                // keeps track of min sens knob raw value
int sensmaxtune = 0;                  // tracks max sens knob raw value

// mode switch button
int modeSwitchPin = 6;                // for atmega32u4 R4a+ boards
int modeswitchstate = 9999;

float temp_midivalue = 0;             // translated 0-127 MIDI message value
int midivalue = 0;                    // translated 0-127 MIDI message value

int ota_lowerlimit;                   // used in pitch latching module
int ota_upperlimit;                   // used in pitch latching module

// bounds and locking
boolean open_gate = true;         // centered
boolean full_gate = false;        // maxed

// pedal working range and scale
unsigned long timecounter = 0;    // general purpose loop counter
int counter = 0;                  // general purpose loop counter

int max_working_range = 900;      // new for #33 - defines largest range including everything - the lower limit + upper limit must fit in this

int upperlimit = 900;             // #33 OTAs max between 900-1000, about 100 lower.  also observed some type of recharge time on the OTAs?  Anyway this works fine
// maximum working range size out of 1023 minus the highest upper limit of all the otas, adjusted by the sensitivity knob
// changed from 600 to 900 for #20 phototransistors
// kept for #21 - 30 Mar 2014 - JR
// 1023 - upper limit - highest lower limit of all otas
// this number now equates to the highest range of the sensitivity knob
// map sensitivity knob max to 0 on this working range.
// sensitivity knob changes decrease under but never increase over this number

//int knobsensitivity = 2;              // sensitivity knob sensitivity dejitter factor
int sensitivity = 4;              // pedal sensitivity dejitter factor - nice and low for better/later pedals
                                      // this changes dynamically, the initial value is not used
                                      // setting to 4 just coz for #34
                                      
// changed from 3 to 2 for #33+ - 08/31/16 - JR
float mode_select_threshold = .25;            // single press on/off threshold


// porch size
// bigger value to lower_limit_pad_minimum sets the size of the smallest difference between the average (idle) of an OTA and it's lower limit trigger.
int lower_limit_pad_minimum = 10;  // program will pick idle+this value as lower limit, or, min lower limit value, higher of the two (12/19/16 - verify???)
// changed from 6 to 10 for #33 - 08/31/16 - JR on apparent floor noise from OTA1....
int minporchsize = 2; // 12/25/16 for #34 - changed to variable for minimum distance from idle value of an OTA to the beginning of the working range
// minimum invariable increase to set off a mode select switch - prevents very quiet pedals from being oversensitized by the threshold percentrage.
int minthreshold = 40;  // trying this for now for #33+
                        // lowered from 50 to 40 for #34

// MIDI pitch variables
int targetlower = 0x0000; // target (midi pitch) lowest value
int targetupper = 0x3FFF; // target (midi pitch) maximum value - 16384 values
int diff = 0;
float scaleratio = 0.0000; // used to scale ratio if upper or lower imits change
byte byte1 = 0;
byte byte2 = 0;
int pitchcmd = 0xE0;
int center = 0x2000;
int zerohex = 0x00;
int pitchvalue = 0;

// sensor tracking
int sensorValue = 0;  // variable to store the value coming from the sensor
//int transformedvalue = 0; // working range adjusted value
int lastsensorValue = 0;
//////////////////////
// Beginning of code//
// Initialization   //
//////////////////////
void setup() {
  // check eeprom restore flag to go into a mode immediately or mode select
  //  int tch = EEPROM.read(address);


  //  Set MIDI baud rate:
#ifdef DEBUG
  delay(5000);          // wait for connection, maybe serial console connection, etc.
  Serial.begin(9600);   // for nice texty output
  //delay(5000);        // wait for connection, maybe serial console connection, etc.
  dpl("Effigy Labs debugger v3");
  dp(numberofOTAs);
  dpl(" OTAs:");
#endif

  Serial1.begin(31250); // always output real MIDI out the DIN5 MIDI port, separate from USB port now.

  // set up the OTAs array
  // OTA_Col maps the pins for teeonardu, not Leonardo
  OTAs[left_ota][OTA_COL] = OTA1;
  OTAs[right_ota][OTA_COL] = OTA2;
  OTAs[middle_ota][OTA_COL] = OTA3;

  // OTA_Col maps the pins
  // default values for all OTAs
  for (counter = 0; counter < numberofOTAs; counter++) {
    OTAs[counter][open_gate_COL] = true;
    OTAs[counter][full_gate_COL] = false;
    OTAs[counter][SWITCH_COL] = false;
    OTAs[counter][ACTIVE_COL] = false;
  }

  // declare the ledPin as an OUTPUT:
  pinMode(commLedPin, OUTPUT);
  pinMode(pedalLedPin, OUTPUT);
  pinMode(fadeLedPin, OUTPUT);
  //digitalWrite(fadeLedPin, HIGH);  // turn on comm LED
  digitalWrite(commLedPin, HIGH);  // turn on comm LED
  digitalWrite(pedalLedPin, HIGH);  // turn on emitters

  // maybe don't need ********
  blinkCommLED(setupblinkinterval, 2); // indicate to user and give pedal sensors time to stabilize under illumination

  // establish the lower limits for the OTAs
#ifdef DEBUG
  //  delay(2000); // to switch to console if desired
  dpl("calibrating");
#endif
  calibrateOTAs();

  // read sensitivity knob and establish common upper limit
#ifdef DEBUG
  dpl("setting initial sensitivity");
#endif

  handleSensitivity();
  timer2 = millis(); // initialize timer2 (longer timer)


#ifndef DEBUG
  //if (usbMIDI.read() == true) handleMidiCmd(); // check for MIDI input
#endif

#ifdef DEBUG
  report();
#endif

  // select mode
#ifdef DEBUG
  dpl("selecting mode");
#endif
  selectMode();
  //analogWrite(commLedPin, 700);  // turn off bright led blink to the fader

  // turn OMNI mode on
//#ifdef DEBUG
//  dpl("sending OMNI mode ON.");
//#endif

  // not sure what this should do, omni is listening, not sending, so there should be a channel picked, yeah
  // removed 02/08/17
  //sendMidiCmd(MIDI_CHG, MIDI_OMNI_MODE_ON, 0);

#ifdef DEBUG
  dpl("entering main loop");
#endif
  cyclestart = millis(); // prime fade led timer
}
//////////////////////////////////
// main loop - mode is selected //
//////////////////////////////////
void loop() {

  // just go into mode 3 forever if mode3
  if (mode == 3) enterMode3(); // enter mode 3

  //
  // we're in mode 1 or 2 so go through pitch bend + middle process
  //
  handleModeSwitch();
  heartbeat();
  handleSensitivity();      // check the sensitivity knob and make adjustments if that is happenin    if(usbMIDI.read() == TRUE) handleMidiCmd(); // check for MIDI input
#ifndef DEBUG
  ////if (usbMIDI.read() == true) handleMidiCmd(); // check for MIDI input
#endif

  //fadeOnSensitivity();       // set fade cycle speed to reflect the sensitivity knob's value
  processOTA(middle_ota);   // turn sustain on or off etc.

  // else we are in mode 1 or 2 which is the original pitch bend emprenda program,
  // with the third OTA added as an additional function which is checked inside the original pitch bend loop
  // not open_gate, one of the sensors is active
  if (sensorPinActive > 0) { // there is a lock already
    sensorValue = analogRead(sensorPin);    // read the value from the active pin
  }
  else { // all are open_gate so wait until one uncenters
    //dp("sensorPinActive=");
    //dpl(sensorPinActive);
    do {
      handleSensitivity(); // handle sensitivity knob inside the inner loop
      handleModeSwitch(); // check for mode switch
#ifndef DEBUG
      //if (usbMIDI.read() == true) handleMidiCmd(); // check for MIDI input
#endif

      processOTA(middle_ota);       // turn sustain on or off etc.
      heartbeat();

      sensorValue = analogRead(OTA1);
      //dp("ota1 raw=");
      //dpl(sensorValue);
      if (sensorValue > (OTAs[left_ota][LOWERLIMIT_COL] + OTAs[left_ota][SENSITIVITY_COL])) {
        sensorPin = OTAs[left_ota][OTA_COL];
        ota_lowerlimit = OTAs[left_ota][LOWERLIMIT_COL];
        ota_upperlimit = OTAs[left_ota][UPPERLIMIT_COL];
        sensitivity = OTAs[left_ota][SENSITIVITY_COL];
        scaleratio = OTAscaleratios[left_ota];
        open_gate = false;

#ifdef DEBUG
        dpl("L OTA on");
#endif

        sensorPinActive = 1; // "left" - breaks when this value changes
        break;
      } // end check if L OTA is on

      sensorValue = analogRead(OTA2);
      if (sensorValue > (OTAs[right_ota][LOWERLIMIT_COL] + OTAs[right_ota][SENSITIVITY_COL])) {
        sensorPin = OTAs[right_ota][OTA_COL];
        ota_lowerlimit = OTAs[right_ota][LOWERLIMIT_COL];
        ota_upperlimit = OTAs[right_ota][UPPERLIMIT_COL];
        sensitivity = OTAs[right_ota][SENSITIVITY_COL];
        scaleratio = OTAscaleratios[right_ota];
        open_gate = false;

#ifdef DEBUG
        dpl("R OTA on");
#endif

        sensorPinActive = 2; // "right" - breaks on this value changing
        //break; don't break here or it never maxes out
      } // end check of if R OTA is on
    }
    while (sensorPinActive == 0);
  }
  // we now have a sensor value

  if (sensorValue < ota_lowerlimit) {

#ifdef DEBUG
    if (sensorPinActive == 1) dpl("L OTA now open gate");
    if (sensorPinActive == 2) dpl("R OTA now open gate");
#endif

    sensorPinActive = 0;
    if (!open_gate) {
      // the value is from the actively bending sensor - else it's a coming-off-center signal
      open_gate = true;
      // set last value to the center
      //lastsensorValue = lowerlimit;
      //sensorValue = lowerlimit;
      lastsensorValue = sensorValue;

      sendPitch(0x2000);
      fadeOnSensitivity();

      return;
    } // if we are open_gate and were already open_gate, do nothing
  } // centering

  // handle maxing out
  if (sensorValue > ota_upperlimit) {
    if (!full_gate) {
      full_gate = true;
      //sensorValue = upperlimit;
      lastsensorValue = sensorValue;

#ifdef DEBUG
      if (sensorPinActive == 1) dpl ("L OTA now full gate");
      if (sensorPinActive == 2) dpl ("R OTA now full gate");
#endif

      if (sensorPinActive == 1) sendPitch(0x0000); // all the way down
      if (sensorPinActive == 2) sendPitch(0x3FFF); // all the way up
      fadeOnPosition(1, 0, 1); // fade at max speed
    }
    return; // we were full_gate so either way exit
  } // maxing

  // do nothing if not changed - filter out noise.  do here before overhead of pitch conversion and also
  // to incrase sensitivity granularity.

  // if not jitter
  if ((sensorValue < (lastsensorValue - sensitivity)) ||
      (sensorValue > (lastsensorValue + sensitivity))) {

    if (open_gate) {
#ifdef DEBUG
      if (sensorPinActive == 1) dpl ("L OTA leaving open gate");
      if (sensorPinActive == 2) dpl ("R OTA leaving open gate");
#endif
      open_gate = false;
    }
    if (full_gate) {
#ifdef DEBUG
      if (sensorPinActive == 1) dpl ("L OTA leaving full gate");
      if (sensorPinActive == 2) dpl ("R OTA leaving full gate");
#endif
      full_gate = false;
    }

    // if sensorValue is in the working range now, and it's not jitter, send pitch messages
    if (sensorPinActive == 1) sendPitch(0x2000 - ((sensorValue - ota_lowerlimit) * scaleratio));
    if (sensorPinActive == 2) sendPitch(0x2000 + ((sensorValue - ota_lowerlimit) * scaleratio));
    fadeOnPosition(sensorValue, ota_lowerlimit, ota_upperlimit);

    lastsensorValue = sensorValue;
  }

} // loop

void enterMode3() {
  // ensure initialize
  // loop through each OTA
  //Serial.println("in mode 3");

  do {
    // process the OTAs
    for (counter = 0; counter < numberofOTAs; counter++) {
      processOTA(counter);
      heartbeat();
    }
    // check the sensitivity knob
    handleSensitivity();

#ifndef DEBUG
    //if (usbMIDI.read() == true) handleMidiCmd(); // check for MIDI input
#endif

    handleModeSwitch();
  }
  while (mode == 3);

}
/*
  Effigy Labs
  Function library v1.1 January 29 2013, August 26, 2016

  OTA handling
  void calibrateOTAs() { - calculate nominal values and lower limits for OTAs
  void processOTA(int OTA) { - get OTA value and output a MIDI message
  void selectMode() { - accept input from pedal and set mode based on first OTA pressed

  void handleSensitivity() {

  void report() { - state of all OTAs and sensitivity knob

  MIDI Output
  void sendMidiCmd(int cmdType, int cmdSubType, int value)
  void sendPitch(int pitchvalue)

  LED Blinking functions
  void blinkCommLED(int time, int repetitions) {
  void blinkLED(int LED, int time, int repetitions) {
  void blinkPedalLED(int time, int repetitions) {

  Debugging for types, sensitive to global debug flag
  void dp(String value) {
  void dp(float value) {
  void dp(int value) {
  void dp(long value) {
  void dpl(String value) {
  void dpl(float value) {
  void dpl(int value) {
  void dpl(long value) {

  State Checks, both active and passive
  boolean isActive(int OTA) {
  boolean isActive(int OTA, int value) {
  boolean isopen_gate(int OTA) {
  boolean isopen_gate(int OTA, int value) {
  boolean isJitter(int OTA) {
  boolean isJitter(int OTA, int val) {
  boolean isfull_gate(int OTA) {
  boolean isfull_gate(int OTA, int value) {


*/

// read a sensor and take action based on the mode.
//  ignore jitter
//  center or max sending the appropriate on or off midi messages
//  if not jitter or center or max then send some midi message
void processOTA(int OTA) {
  int OtaValue = analogRead( OTAs[OTA][OTA_COL]);

  // noise?
  if (isJitter(OTA, OtaValue)) {
    //Serial.print("jitter");
    return;
  }
  else {
    // it's not jitter so save this value for the next one
    //Serial.println("not jitter");
    OTAs[OTA][LASTSENSORVALUE_COL] = OtaValue;
    // and don't return
  }

  // not noise so check for stuff

  // check for centering / open gate
  if (isopen_gate(OTA, OtaValue)) {
    if (OTAs[OTA][open_gate_COL] == false) {
#ifdef DEBUG
      dp("ota ");
      dp(OTA);
      dpl(" now open gate");
#endif
      OTAs[OTA][open_gate_COL] = true;
      OTAs[OTA][full_gate_COL] = false;
      // deactivate depending on the preset
      fadeOnSensitivity();
      switch (OTAs[OTA][MIDI_CMD_COL]) {
        case MIDI_SUSTAIN:
#ifdef DEBUG
          dpl("midi: sustain off");
#endif
          if(OTAs[OTA][ACTIVE_COL] == true) {
            sendMidiCmd(MIDI_CHG, MIDI_SUSTAIN, MIDI_SUSTAINOFF_CMD );
            fadeOnPosition(1, 0, 1); // turn on
          }

          break;
        case MIDI_MOD_CMD:
#ifdef DEBUG
          dpl("midi: mod off");
#endif
          if(OTAs[OTA][ACTIVE_COL] == true) {
            sendMidiCmd(MIDI_CHG, MIDI_MOD_CMD, 0);
            fadeOnPosition(1, 0, 1); // turn on
          }
          break;
        case MIDI_BREATH_CMD:
#ifdef DEBUG
          dpl("midi: breath off");
#endif
          if(OTAs[OTA][ACTIVE_COL] == true) {
            sendMidiCmd(MIDI_CHG, MIDI_BREATH_CMD, 0);
            fadeOnPosition(1, 0, 1); // turn on
          }

          break;
        case MIDI_XPRS_CMD:
#ifdef DEBUG
          dpl("midi: expression off");
#endif
          if(OTAs[OTA][ACTIVE_COL] == true) {
            sendMidiCmd(MIDI_CHG, MIDI_XPRS_CMD, 0);
            fadeOnPosition(1, 0, 1); // turn on
          }
          break;
        default:
          nop = 0;
#ifdef DEBUG
          dpl("the MIDI command was not in the list!");
#endif

      OTAs[OTA][ACTIVE_COL] = false; // this is done at the end so things can check to not turn off what's already off

      }
      return; // we open_gate so we're done for this pass
    }
    else {
      //Serial.println(" and were open_gate before");
      return; // return and finish if we were open_gate and were already open_gate
    }

  } // end of check for centering

  // check for full gate
  if (isfull_gate(OTA, OtaValue)) {
    if (OTAs[OTA][full_gate_COL] == false) {
#ifdef DEBUG
      dp("ota ");
      dp(OTA);
      dpl(" now full gate");
#endif
      OTAs[OTA][full_gate_COL] = true;
      OTAs[OTA][open_gate_COL] = false;
      OTAs[OTA][ACTIVE_COL] = false;
      // activate depending on the preset
      switch (OTAs[OTA][MIDI_CMD_COL]) {
        case MIDI_SUSTAIN:
#ifdef DEBUG
          dpl("midi: sustain on");
#endif
          // send sustain only if sustain not already on
          sendMidiCmd(MIDI_CHG, MIDI_SUSTAIN, MIDI_SUSTAINON_CMD );
          fadeOnPosition(1, 0, 1); // turn on full gate

          break;
        case MIDI_MOD_CMD:
#ifdef DEBUG
          dpl("midi: mod 127");
#endif
          sendMidiCmd(MIDI_CHG, MIDI_MOD_CMD, 127);
          fadeOnPosition(1, 0, 1);

          break;
        case MIDI_BREATH_CMD:
#ifdef DEBUG
          dpl("midi: breath 127");
#endif
          sendMidiCmd(MIDI_CHG, MIDI_BREATH_CMD, 127);
          fadeOnPosition(1, 0, 1); // turn on full gate

          break;
        case MIDI_XPRS_CMD:
#ifdef DEBUG
          dpl("midi: expression 127");
#endif
          sendMidiCmd(MIDI_CHG, MIDI_XPRS_CMD, 127);
          fadeOnPosition(1, 0, 1); // turn on max
          break;
        default:
          nop = 0;
#ifdef DEBUG
          dpl("the MIDI command was not in the list!");
#endif
      }
      //Serial.println("and were full_gate already");
      return; // we full_gate so that's it for this pass
    }
    else {
      //Serial.print("and were full_gate before");
      return; // return and finish if we were open_gate and were already open_gate
    }
  } // end of handle full gate

  // we're not full_gate and not open_gate and not jitter so we should send a message
  OTAs[OTA][full_gate_COL] = false;
  OTAs[OTA][open_gate_COL] = false;

  if (!(OTAs[OTA][MIDI_CMD_COL] == MIDI_SUSTAIN)) {
    // calculate the output midi value for the non-binary (all except sustain) funcs for now
    temp_midivalue = (OtaValue - OTAs[OTA][LOWERLIMIT_COL]) / (float) OTAs[OTA][WORKINGRANGE_COL] * 127;
    midivalue = (int) temp_midivalue; // truncate or maybe round
  }

  // change fade speed
  fadeOnPosition(midivalue, 0, 127); // fade speed based on

  //dp("midi:fade on position (0-127): ");
  //dpl(midivalue);

  switch (OTAs[OTA][MIDI_CMD_COL]) {
    case MIDI_SUSTAIN:
      if (!OTAs[OTA][ACTIVE_COL] == true) { // if sustain is already on don't turn it on again with the *LOCK variables
        OTAs[OTA][ACTIVE_COL] = true;

#ifdef DEBUG
        dpl("midi: sustain leaving open gate");
#endif

        sendMidiCmd(MIDI_CHG, MIDI_SUSTAIN, MIDI_SUSTAINON_CMD);
      }
      break;
    case MIDI_MOD_CMD:

#ifdef DEBUG
      dp("midi: mod ");
      dpl(midivalue);
#endif

      sendMidiCmd(MIDI_CHG, MIDI_MOD_CMD, midivalue);

      break;
    case MIDI_BREATH_CMD:
#ifdef DEBUG
      dp("midi: breath ");
      dpl(midivalue);
#endif
      sendMidiCmd(MIDI_CHG, MIDI_BREATH_CMD, midivalue);
      break;
    case MIDI_XPRS_CMD:
#ifdef DEBUG
      dp("midi: expression ");
      dpl(midivalue);
#endif
      sendMidiCmd(MIDI_CHG, MIDI_XPRS_CMD, midivalue);
      break;
    default:
      nop = 0;
#ifdef DEBUG
      dpl("the MIDI command was not in the list!");
#endif

  }

}
///////////////////////
// end of main loop  //
///////////////////////

////////////////////////////
////// methods follow //////
////////////////////////////

// set or change the mode explicitly
void selectMode(int newmode) {
  mode = newmode;
}

// blink LEDs until one OTA is pressed and set the mode
void selectMode() {

#ifdef DEBUG
  dpl("selectMode");
#endif
  //int modeselectblinkinterval = 100; // turn on and off every this many ms
  
  // if mode = 99 then make the blinker very fast
  if(mode ==                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         99) {
    modeconfirmblinkspeed = 30;
    mode = 0;
  }

  //  int blinkcounter = 0;
  int blinkstate = LOW;

  // variables for sensor inputs
  int OTA1InitialValue = 0;
  int OTA2InitialValue = 0;
  int OTA3InitialValue = 0;

  // get initial values
  OTA1InitialValue = analogRead(OTA1);
  OTA2InitialValue = analogRead(OTA2);
  OTA3InitialValue = analogRead(OTA3);
  timecounter = millis(); // initialize timer for blink

      //digitalWrite(commLedPin, HIGH);

  do {
    // get inputs from sensors
    int ov = analogRead(OTA1);
    if (ov > (max(OTA1InitialValue,minthreshold)) + (OTA1InitialValue * mode_select_threshold) ) {
#ifdef DEBUG
      dpl("mode 1 selected");
#endif
      // OTAs 0 and 1 will be pitch bend
      OTAs[middle_ota][MIDI_CMD_COL] = MIDI_SUSTAIN;
      // set the threshold for trigger here *****
      mode = 1;

      // indicate mode 1 selected on the comm LED
      digitalWrite(fadeLedPin, LOW);  // turn on led
      digitalWrite(commLedPin, LOW);  // turn on led
      delay(250);
      blinkCommLED(modeconfirmblinkspeed, 1);
      digitalWrite(fadeLedPin, LOW);  // turn on led
      digitalWrite(commLedPin, LOW);  // turn off led
      delay(100);
      digitalWrite(fadeLedPin, HIGH);  // turn on led
      digitalWrite(commLedPin, HIGH);  // turn on led

      break;

    }

    ov = analogRead(OTA2);
    //dp("ota2=");
    //dpl(ov);
    if (ov > (max(OTA1InitialValue,minthreshold)) + ( OTA2InitialValue * mode_select_threshold) ) {

#ifdef DEBUG
      dpl("mode 2 selected");
#endif

      // this is what mode 2 is, defaulting to L-R=pitch and middle OTA = MOD.  not sure about the switch_col for now - 090616


      // OTAs 0 and 1 will be pitch bend
      OTAs[middle_ota][MIDI_CMD_COL] = MIDI_MOD_CMD;
      // no threshold
      // no lower range limit
      // no upper range limit
      // no compression (a la what the sensitivity knob does, e.g. map a section of this effect to a section of the ota range, or map the entire effect to a section of the ota range
      // ??????????
      OTAs[2][SWITCH_COL] = true; // this means turn it either on or off without graduating

      digitalWrite(fadeLedPin, LOW);  // turn off led
      digitalWrite(commLedPin, LOW);  // turn off led
      delay(250);
      blinkCommLED(modeconfirmblinkspeed, 2);
      digitalWrite(fadeLedPin, LOW);  // turn off led
      digitalWrite(commLedPin, LOW);  // turn off led
      delay(100);
      digitalWrite(fadeLedPin, HIGH);  // turn on led
      digitalWrite(commLedPin, HIGH);  // turn on led
      mode = 2;
      break;
    }
    ov = analogRead(OTA3);
    //dp("ota3=");
    //dpl(ov);
    if (analogRead(OTA3) > (max(OTA1InitialValue,minthreshold)) + ( OTA3InitialValue * mode_select_threshold) ) {

#ifdef DEBUG
      dpl("mode 3 selected");
#endif

      OTAs[left_ota][MIDI_CMD_COL] = MIDI_MOD_CMD;
      OTAs[right_ota][MIDI_CMD_COL] = MIDI_XPRS_CMD;
      OTAs[middle_ota][MIDI_CMD_COL] = MIDI_BREATH_CMD;
      digitalWrite(fadeLedPin, LOW);  // turn on led
      digitalWrite(commLedPin, LOW);  // turn on led
      delay(250);
      blinkCommLED(modeconfirmblinkspeed, 3);
      digitalWrite(fadeLedPin, LOW);  // turn off led
      digitalWrite(commLedPin, LOW);  // turn off led
      delay(100);
      digitalWrite(fadeLedPin, HIGH);  // turn on led
      digitalWrite(commLedPin, HIGH);  // turn on led
      mode = 3;
      break;
    }

    // handle blinking
    //Serial.println(millis()-counter);
    if ((int) (millis() - timecounter) > modeselectblinkinterval) {

#ifdef DEBUG
      dp(".");
#endif
      blinkstate = !blinkstate;
      digitalWrite(fadeLedPin, blinkstate);   // blink
      
      if(blinkstate == HIGH) {
         digitalWrite(commLedPin, HIGH); //replace HIGH with half-high
      } else {
        digitalWrite(commLedPin, blinkstate);   // which should be LOW at all times      
      }
      //digitalWrite(pedalLedPin, blinkstate);  // blink
      timecounter = millis(); // reset
    }

  } // end of the mode select do loop
  while (mode == 0);

} // selectMmode


// calibrate the OTAs at startup
void calibrateOTAs() {
  // read the sensors for a while
  //   determine the lower limit and the noise level
  //   max and min value, average value - for now
  //   average value becomes the nominal limit.
  //   nominal limit + 2x the average max differeance = lower limit

  //   evidence testing #31 indicated a more static approach may be better:
  //   idling at 25-30 - porch break at about 50-52 -
  // sample input from the OTAs
  // set for each OTA:
  // nominal value
  // sensitivity
  // lower limit
  int OTA1avg = 0;
  int OTA2avg = 0;
  int OTA3avg = 0;

  int OTA1min = 1024;
  int OTA1max = 0;
  int OTA2min = 1024;
  int OTA2max = 0;
  int OTA3min = 1024;
  int OTA3max = 0;
  int tempSensorValue = 0;
  int OTA1InitialValue = 0;
  int OTA2InitialValue = 0;
  int OTA3InitialValue = 0;

  // get initial values
  OTA1InitialValue = analogRead(OTA1);
  OTA2InitialValue = analogRead(OTA2);
  OTA3InitialValue = analogRead(OTA3);

  timecounter = millis(); // initialize timer for blink

  // gather some statistical data
#ifdef DEBUG
  dp("calibrating OTAs using ");
  dp(numberOfSamples);
  dpl(" samples");
#endif

  // get inputs from sensors and update counters and bounds
  for (int i = 0; i < numberOfSamples; i++) {
    tempSensorValue = analogRead(OTA1);
    OTA1avg += tempSensorValue;
    OTA1max = max(OTA1max, tempSensorValue);
    OTA1min = min(OTA1min, tempSensorValue);

    tempSensorValue = analogRead(OTA2);
    OTA2avg += tempSensorValue;
    OTA2max = max(OTA2max, tempSensorValue);
    OTA2min = min(OTA2min, tempSensorValue);

    tempSensorValue = analogRead(OTA3);
    OTA3avg += tempSensorValue;
    OTA3max = max(OTA3max, tempSensorValue);
    OTA3min = min(OTA3min, tempSensorValue);
  }

  // calculate average positions (idle value)
  OTA1avg = OTA1avg / numberOfSamples;
  OTA2avg = OTA2avg / numberOfSamples;
  OTA3avg = OTA3avg / numberOfSamples;

  //set the sensitivities
  OTAs[left_ota][SENSITIVITY_COL] = (OTA1max - OTA1min) + minporchsize;
  OTAs[right_ota][SENSITIVITY_COL] = (OTA2max - OTA2min) + minporchsize;
  OTAs[middle_ota][SENSITIVITY_COL] = (OTA3max - OTA3min) + minporchsize;

#ifdef DEBUG
  dpl("OTA1, OTA2, OTA3");
  dp("avg:");
  dp(OTA1avg);
  dp(", ");
  dp(OTA2avg);
  dp(", ");
  dpl(OTA3avg);

  dp("min:");
  dp(OTA1min);
  dp(", ");
  dp(OTA2min);
  dp(", ");
  dpl(OTA3min);

  dp("max:");
  dp(OTA1max);
  dp(", ");
  dp(OTA2max);
  dp(", ");
  dpl(OTA3max);


  dp("snx:");
  dp(OTAs[left_ota][SENSITIVITY_COL]);
  dp(", ");
  dp(OTAs[right_ota][SENSITIVITY_COL]);
  dp(", ");
  dpl(OTAs[middle_ota][SENSITIVITY_COL]);
#endif

  //set the lower limits
  OTAs[left_ota][LOWERLIMIT_COL] = max(OTA1max + ((OTA1max - OTA1min)), OTA1max + lower_limit_pad_minimum);
  OTAs[right_ota][LOWERLIMIT_COL] = max(OTA2max + ((OTA2max - OTA2min)), OTA2max + lower_limit_pad_minimum);
  OTAs[middle_ota][LOWERLIMIT_COL] = max(OTA3max + ((OTA3max - OTA3min)), OTA3max + lower_limit_pad_minimum);

  // find max lower limitset this to the highest value of all the lower limits, used to calculate the upper limits
  for (j = 0; j < numberofOTAs; j++) {
    maxLowerLimit = max(OTAs[j][LOWERLIMIT_COL], maxLowerLimit);
  }

  /*
    int t2 = OTA2avg + (2 * (OTA2max - OTA2avg));
    if(t2 < lower_limit_pad_minimum) {
       t2 = lower_limit_pad_minimum;
    }
    OTAs[right_ota][LOWERLIMIT_COL] = t2;

    int t3 = OTA3avg + (2 * (OTA3max - OTA3avg));
    if(t3 < lower_limit_pad_minimum) {
       t3 = lower_limit_pad_minimum;
    }
    OTAs[middle_ota][LOWERLIMIT_COL] = t3 + 20; // -jr-changed to 20 for #21+ "+ 30; // the +30 to reduce oversensitivity on the middle OTA since it has an experimental hard candy (that is the theory, or maybe the middle OTA is specially sensitive because of it's placement/amount of centeredness in the body / being between the other two / etc specialness. "
  */
#ifdef DEBUG
  dp("ll:");
  dp(OTAs[left_ota][LOWERLIMIT_COL]);
  dp(", ");
  dp(OTAs[right_ota][LOWERLIMIT_COL]);
  dp(", ");
  dpl(OTAs[middle_ota][LOWERLIMIT_COL]);

  // the upper limits are set in handleSensitivity()  by reading the sensitivity knob
  dpl("calibrated.");
  report();
#endif

}


// debugging report for the OTA table
void report() {
  // dump debug of OTA Array

#ifdef DEBUG
  dp("currently in mode ");
  dp(mode);
  dp(" on MIDI channel ");
  dp(CURRENT_MIDI_CHANNEL);
  dp("\t");
  dp("max working range: ");
  dp(working_range);
  dp("\t");
  dp("max lower limit: ");
  dpl(maxLowerLimit);

  dpl("OTA\tACT\tLL\tUL\tCTR\tMAX\tVAL\tPRV\tRNG\tSW\tScalar\tMIDI\tSnx");
  for (int i = 0; i < numberofOTAs; i++) {

    dp(OTAs[i][OTA_COL]);
    dp("\t");

    dp(OTAs[i][ACTIVE_COL]);
    dp("\t");

    dp(OTAs[i][LOWERLIMIT_COL]);
    dp("\t");

    dp(OTAs[i][UPPERLIMIT_COL]);
    dp("\t");

    dp(OTAs[i][open_gate_COL]);
    dp("\t");

    dp(OTAs[i][full_gate_COL]);
    dp("\t");

    dp(OTAs[i][VALUE_COL]);
    dp("\t");

    dp(OTAs[i][LASTSENSORVALUE_COL]);
    dp("\t");

    dp(OTAs[i][WORKINGRANGE_COL]);
    dp("\t");

    dp(OTAs[i][SWITCH_COL]);
    dp("\t");

    dp(OTAscaleratios[i]); // different array to hold a float
    dp("\t");

    dp(OTAs[i][MIDI_CMD_COL]);
    dp("\t");

    dp(OTAs[i][SENSITIVITY_COL]);
    dpl("\t");

  }
#endif

}
// sensitivty knob varies the size of the working range via the upperlimit variable.  A smaller working range means maxing at a lower setting.
// So it takes less of a push to max as sensitivity increases.
// sensitivity knob value of zero is least sensitive and sets the upperlimit at lowerlimit + 2, so it's essentially on/off.
// box 4's sensitivity knob ranges from 300 to 660 for a total of 360 graduations of sensitivity
// #20 knob goes from 0 to about 500 so just adding a x2 factor and limiting to extend the range...12/7/13
// #21 knob goes from 0 to 506 - it's a 100K pot - fix the x2 algorithm above b/c it only responded to 5 and above settings on teh knob - 1-4 didn't change anything
// #32 knob goes from 0 to 496

void handleSensitivity() {

  //////////////////// handle first-time initialization
  if (lastSensitivityKnobValue == 9999) {
    //Serial.println("first time through sensitivity loop");
    tripp = trigger + 1; // set off the loop if first time through
  }

  // read the knob value
  sensitivityKnobValue = analogRead(dummypin); // read something besides OTA3 to decouple the hold/sample capcitor's influence on the OTA3 and the knob.
  sensitivityKnobValue = analogRead(sensitivityKnobPin);

  //sensitivityKnobValue = min(sensitivityKnobMinValu4e, sensitivityKnobValue);
  if (sensitivityKnobValue < sensitivityKnobMinValue) {
    // ignore the odd low reading to gracefully cap off the lower end of the knob
    sensitivityKnobValue = sensitivityKnobMinValue;
    return;
  }

  if (sensitivityKnobValue > sensitivityKnobMaxValue) {
    // ignore the odd high reading to gracefull cap off the high end of the knob
    sensitivityKnobValue = sensitivityKnobMaxValue;
    return;
  }

  // take off the bottom of the knob's minimum value - box-specific?  calibrate?  how?  adjust the vref?
  //sensitivityKnobValue = sensitivityKnobValue - sensitivityKnobMinValue;

  // if this is the first passs it will trip at least once
  if ((sensitivityKnobValue < (lastSensitivityKnobValue - knobjitter)) ||
      (sensitivityKnobValue > (lastSensitivityKnobValue + knobjitter))) {
    tripp++;
    if (tripp > trigger) { // activate a change
      tripp = 0;

#ifdef DEBUG
      dp("k rv: ");
      dp(sensitivityKnobValue);
      dp(", last rv: ");
      dpl(lastSensitivityKnobValue);
#endif

      lastSensitivityKnobValue = sensitivityKnobValue; // reset for next

      /*
        set the upper limit and therefore the size of the working range, using the sensitivity knob

        equation to derive ratio of knob range to working sensor range:
          knob value / knob range = % activated
          % activated x target range size = upper limit
        on the emprenda board #4, the sensitivity knob range is equal directly to the working range of the sensor.
        the uppperlimit is the size of the working range at it's maximum
      */

      // sv = sensitivity value

#ifdef DEBUG
      dp("upper limit: ");
      dp(upperlimit);

      dp(", knob: ");
      dp(sensitivityKnobValue);

      dp(", knob min: ");
      dp(sensitivityKnobMinValue);

      dp(", knob max: ");
      dp(sensitivityKnobMaxValue);
#endif
      // map sensitivity knob max range to ota working range
      float sv = (float) sensitivityKnobValue / (sensitivityKnobMaxValue - sensitivityKnobMinValue);
      //float sv = upperlimit * (sensitivityKnobValue / (sensitivityKnobMaxValue - sensitivityKnobMinValue));

      //sv is the sensitivity knob's value expressed as a percentage
      // so now the upper limit is set as a percentage of the max working range
      // the max working range is the max ota reading value e.g. 900, minus the largest lower limit of all the OTAs.

      // new upper limit should be sv times the max working range .....***** FIX HERE
      working_range = upperlimit - maxLowerLimit;
      int new_upper_limit = sv * working_range;


      //new_upper_limit = (int) upperlimit * sv;

      // if it's zero then has to be 1 to avoid division by zero math
      if (new_upper_limit < 2) new_upper_limit = 2;
      // sv is now the normalized upper limit to give to the OTAs

      //diff = upperlimit - lowerlimit;  // = working range
      //Serial.print("diff now ");
      //Serial.println(diff);

      scaleratio = (float) center / new_upper_limit; // number of pitch places per sensor place

#ifdef DEBUG
      dp(", sv: ");
      dp(sv);
      dp(", new ul: ");
      dp(new_upper_limit);
      dp(", sr: ");
      dpl(scaleratio);
#endif

      // somewhere in here we need to calculate the new space we have to work in and set a max working range that includes the lower limit and does not exceed upper_limit

      // update the OTA array
      for (j = 0; j < numberofOTAs; j++) {
        OTAs[j][UPPERLIMIT_COL] = OTAs[j][LOWERLIMIT_COL] + new_upper_limit;
        OTAs[j][WORKINGRANGE_COL] = OTAs[j][UPPERLIMIT_COL] - OTAs[j][LOWERLIMIT_COL];
        OTAscaleratios[j] = (float) center / OTAs[j][WORKINGRANGE_COL];
      }
#ifdef DEBUG
      report();
#endif
      fadeOnSensitivity();

    }
  }
  else {
    tripp = 0;  // if we didn't trip, reset
  }
}


void handleModeSwitch() {
  modeswitchstate = analogRead(modeSwitchPin);
  
  //#ifdef DEBUG
  //  dp("mode switch value=");
  //  dpl(modeswitchstate);
  //#endif
  if (modeswitchstate < 100) {
#ifdef DEBUG
    dpl("mode switch depressed.");
#endif
    //mode = 0; // reset mode
    // make the mode selection confirmation blink very fast

   checktimer2(); // check if long-term timer is tripped and do special stuff if so

    mode = 99;
    selectMode(); // indicate to selectmode that it is the mode switch switch not just the first boot select
  }
}

void checktimer2() {
  if((millis() - timer2) > timer2_threshold) {
    timer2 = millis();
    // do whatever we do when we hit the long timer
    // special blink mode
    // select bank
    // Once bank is selected, then go into mode select 
  }

}

/*
  set fade cycle speed to reflect the sensitivity knob's value - if the pedal is operating and fade cycle is ota-based,
  the fade cycle speed will reflect the sensitivity knob's position rather than the OTA position until the OTA position changes again
  which should not be a problem unless the OTA is, say, locked on with the sentivivity knob all the way up, or, being played by a robot,
  who happens to care about the exact fade speed.  either way it's good enough for now.  -JR
*/
void fadeOnSensitivity() {
  fadespeed = map(sensitivityKnobValue, sensitivityKnobMinValue, sensitivityKnobMaxValue, fadeSpeedMax, fadeSpeedMin);
  //dp("fadeOnSensitivity fadespeed:");
  //dpl(fadespeed);
}

void fadeOnPosition(int value, int min, int max) {
  fadespeed = map(value, min, max, fadeSpeedMin, fadeSpeedMax);
  //dp("fadeOnPosition fadespeed:");
  //dpl(fadespeed);
}


// send a MIDI message
void sendMidiCmd(int cmdType, int cmdSubType, int value)
{
  if ( (cmdType == lastCmdType) &&
       (cmdSubType == lastCmdSubType) &&
       (value == lastValue) ) {
    return;
  }
  lastCmdType = cmdType;
  lastCmdSubType = cmdSubType;
  lastValue = value;

  //usbMIDI.sendNoteOn(pitch, velocity, 1);
#ifndef DEBUG
  if (cmdType == MIDI_CHG) usbMIDI.sendControlChange(cmdSubType, min(127, value), CURRENT_MIDI_CHANNEL); // **** fix this to send the code based on selected channel
#endif

  // write normal MIDI out the DIN-5 MIDI port
  Serial1.write(cmdType); // midi command type - MIDI_CHG, or something else
  Serial1.write(cmdSubType); // midi command: MOD wheel, breath, sustain, expression
  Serial1.write(min(127, value)); // 0-127
}
/*
  byte 1 = b0 - for channel 1
  byte 2 = below
  byte 3 = parameter specific

  MOD - #x01          - 1 	00000001 	01 	Modulation Wheel or Lever 	0-127 	MSB
  BREATH - #x02       - 2 	00000010 	02 	Breath Controller 	0-127 	MSB
  Expression - #11 (0x0B) - 11 	00001011 	0B 	Expression Controller 	0-127 	MSB
  SUSTAIN  - #64 (x40) - 64 	01000000 	40 	Damper Pedal on/off (Sustain) 	≤63 off, ≥64 on

  sustain off
  chgcmd, sustaincmd, 0 (any value < 63)
  sustain on
  chgcmd, sustaincmd, 128 (any value > 64)
  expression, breath, and mod
  chgcmd, expresscmd/breathcmd/modcmd, mod value 0-127

  NO xxxxxmidi byte = int value >> 1;
  resulting in 0xxxxxxx where xxxxxxx = the msb of the value

  if a = int = 127 = 64 + 32 + 4 = 01111111
  just make a value < 128 and send that value directly?
*/


// send pitch bend values out the MIDI port
void sendPitch(int pitchvalue)
{
  if (pitchvalue == lastPitchValue) return;
  lastPitchValue = pitchvalue;
  /*
    input - 16 bit value
    output - 14 bit value in midi format: 2 bytes, leading zero bit,
  */
  byte2 = pitchvalue << 1;
  byte2 = byte2 >> 1;
  byte1 = pitchvalue >> 7;

  //  usbMIDI.sendNoteOn(pitch, velocity, 1);

  //usbMIDI.sendNoteOn(pitch, velocity, 1);

  // always write real midi out the DIN-5 port
  Serial1.write(pitchcmd);
  Serial1.write(byte2);
  Serial1.write(byte1 );

#ifndef DEBUG
  usbMIDI.sendPitchBend(pitchvalue, CURRENT_MIDI_CHANNEL);
#endif

#ifdef DEBUG
  dp("midi: p ");
  dp(pitchvalue);
  dp(", 0x");
  Serial.print(pitchvalue, HEX);

  dp(", ");
  Serial.print(byte1, HEX);
  dp(" ");
  Serial.println(byte2, HEX);
#endif
}  // sendpitch


boolean isJitter(int OTA) {
  int v = analogRead(OTAs[OTA][OTA_COL]);
  if (v < (OTAs[OTA][LASTSENSORVALUE_COL] - OTAs[OTA][SENSITIVITY_COL])  ||
      (v > (OTAs[OTA][LASTSENSORVALUE_COL] + OTAs[OTA][SENSITIVITY_COL])))
  {
    return false; // it's not jitter
  }
  else {
    return true; // it is jitter
  }
} // isJitter


int getSensorValue(int OTA) {
  int temp;
  temp = analogRead(OTAs[OTA][OTA_COL]);           // read sensor pin value
  OTAs[OTA][LASTSENSORVALUE_COL] = OTAs[OTA][VALUE_COL];  // save old value
  OTAs[OTA][VALUE_COL] = temp;                     // put new value in table
  return temp;
}

// is the OTA being pressed but not full_gate out? - and use the sensor value
boolean isJitter(int OTA, int val) {

  if ((val < (OTAs[OTA][LASTSENSORVALUE_COL] - OTAs[OTA][SENSITIVITY_COL]))  ||
      (val > (OTAs[OTA][LASTSENSORVALUE_COL] + OTAs[OTA][SENSITIVITY_COL])))
  {
    return false; // it's not jitter
  }
  else {
    return true; // it is jitter
  }
} // isJitter


// is the OTA being pressed but not full_gate out? - and use the functions to actively read the sensor values
boolean isActive(int OTA) {
  if ( (isfull_gate(OTA) || isopen_gate(OTA)) ) {
    return false;
  }
  else {
    return true;
  }
} // isActive


// is active and supply a sensor value
boolean isActive(int OTA, int value) {
  if ( (isfull_gate(OTA, value) || isopen_gate(OTA, value)) ) {
    return false;
  }
  else {
    return true;
  }
} // isActive


// is the OTA full_gate out? - and use the value supplied by the caller
boolean isfull_gate(int OTA, int value) {
  if ( value > OTAs[OTA][UPPERLIMIT_COL] ) {
    return true;
  }
  else {
    return false;
  }
} // isfull_gate


// is the OTA full_gate out? - and actively read if full_gate
boolean isfull_gate(int OTA) {
  if ( analogRead(OTAs[OTA][OTA_COL]) > OTAs[OTA][UPPERLIMIT_COL] ) {
    return true;
  }
  else {
    return false;
  }
} // isfull_gate


// is the OTA open_gate? - and actively query if open_gate
boolean isopen_gate(int OTA) {
  if ( analogRead(OTAs[OTA][OTA_COL]) < OTAs[OTA][LOWERLIMIT_COL] ) {
    return true;
  }
  else {
    return false;
  }
} // isopen_gate


// is the OTA open_gate? - and use the sensor value provided by the caller
boolean isopen_gate(int OTA, int value) {
  if ( value < OTAs[OTA][LOWERLIMIT_COL] ) {
    return true;
  }
  else {
    return false;
  }
} // isopen_gate


// blink any LED
void blinkLED(int LED, int time, int repetitions) {
  for (int t = 0; t < repetitions; t ++) {
    digitalWrite(LED, LOW);  // turn off led
    delay(time);
#ifdef DEBUG
    dp(".");
#endif
    digitalWrite(LED, HIGH);  // turn on led
    delay(time);
  }
} //blinkLED


void blinkCommLED(int time, int repetitions) {
  // turn off fading for hard blinking
  for (int t = 0; t < repetitions; t ++) {
    digitalWrite(commLedPin, LOW);  // turn off led
    digitalWrite(fadeLedPin, LOW);  // turn off led
    delay(time);
#ifdef DEBUG
    dp(".");
#endif
    digitalWrite(commLedPin, HIGH);  // turn on led
    digitalWrite(fadeLedPin, HIGH);  // turn on led
    delay(time);
  }
} //blinkCommLED

void blinkPedalLED(int time, int repetitions) {
  for (int t = 0; t < repetitions; t ++) {
    digitalWrite(pedalLedPin, LOW);  // turn off led
    delay(time);
#ifdef DEBUG
    dp(".");
#endif
    digitalWrite(pedalLedPin, HIGH);  // turn on led
    delay(time);
  }
} // blinkPedalLED

// this provides a heartbeat on pin 9, so you can tell the software is running.
void heartbeat() {
  // no-delay delay

  if ((int)(millis() - cyclestart) >= delayint) {
    cyclestart = millis();
    if ((hbval > fademax) || (hbval < fademin)) {
      fadespeed = -fadespeed; // flip increment counter - possible bugs here b/c the come back into the range may not be how we went out
    }
    //    if (hbval > fademax) fadespeed = -fadespeed;
    //    if (hbval < fademin) fadespeed = -fadespeed;
    hbval += fadespeed;
    //if(hbval > fademax) hbval = fademax;  // constrain top but not bottom so the flipper won't be thrown off
    ledFadeBrightness = constrain(hbval, fademin, fademax);
    analogWrite(fadeLedPin, ledFadeBrightness);
  }
}

#ifdef DEBUG

//debug-sensitive print
void dp(String value) {
  Serial.print(value);
}

//debug-sensitive print
void dp(int value) {
  Serial.print(value);
}

// debug-sensitive println
void dpl(String value) {
  Serial.println(value);
}
// debug-sensitive println
void dpl(int value) {
  Serial.println(value);
}
// debug-sensitive println
void dp(float value) {
  Serial.print(value);
}
// debug-sensitive println
void dpl(float value) {
  Serial.println(value);
}
// debug-sensitive println
void dp(long value) {
  Serial.print(value);
}
// debug-sensitive println
void dpl(long value) {
  Serial.println(value);
}
#endif

#ifndef DEBUG

// interrupt handling for MIDI input
void handleMidiCmd() {
  // program schedule
  // lots of code that only compiles as a midi device, incl midi input
  int cmdType = usbMIDI.getType(); // type 7 = sysex
  if (cmdType == 7) {
    nop = 0;

    //usbMIDI.getData2()
    //usbMIDI.getSysExArray();

/*
    usbMIDI.getSysExArray();
    int cmdSz = usbMIDI.getData1(); // size of bytes in command
    byte[] sar = usbMIDI.getSysExArray();

    blinkCommLED(100, cmdSz);

    for (j = 0; j < cmdSz; j++) {
      usbMIDI.getSysExArray()
      blinkCommLED(100, sar[j]);
      }
*/   }

  }
  //int[] cmdAr = usbMIDI.getData2(); // size of bytes in command
#endif

#ifdef DEBUG
  //dp("MIDI input: ");
  //dp(cmdSz);
  ///dpl(" bytes");
  //for (j = 0;j< cmdSz;j++) {
  //  dp(
  //}
  //}
#endif


