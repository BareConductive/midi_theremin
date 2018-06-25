/*******************************************************************************
  Midi Theremin Code 
  ---------------------------------
   
  Midi_Theremin.ino - sketch turning the Touch Board into a MIDI Theremin when
  placed in Real Time MIDI mode. Soldering or use of Electric Paint is required.

  To do this, connect the two solder bridges with "MIDI" and "MIDI ON" printed
  next to the solder pads on the Touch Board. If you don't have a soldering iron
  you can use Electric Paint to connect the two copper pads by placing a small
  blob of the conductive paint across to form a bridge. 

  To create a custom interface for the theremin, connect 2 sensor pads to the
  electrodes to control pitch and volume (default electrodes E0 & E1)
  You can optionally connect 2 more pads for changing the midi instrument
  channel up and down (default electrodes E2 & E3).

  Once the Touch Board initialises, you have to calibrate the pitch & volume pads.
  Just touch the volume pad, then touch the pitch pad, and you are ready to play.
  (max and min levels are constantly updated, so touching sets the max levels)
  Then have fun playing!

  Misc:
  
  I used fscale (http://playground.arduino.cc/Main/Fscale) to logarithmically map
  proximity values to pitch and volume. The defaults are set to maximize
  sensitivity at large distances from the pads. (nonlinear_pitch & nonlinear_volume)
  Only melodic instruments are implemented, no percussion instruments.
  Default is high pitch and high volume when touch pads, but this can be reversed.
  (direction_pitch & direction_volume)
  Default starting instrument is ocarina

  Written by Nathan Tomlin (nathan.a.tomlin@gmail.com) with tons of code
  stolen from Bare Conductive "Midi_Piano.ino"
   
  "Midi_Piano.ino" (Bare Conductive code written by Stefan Dzisiewski-Smith and 
  Peter Krige. Much thievery from Nathan Seidle in this particular sketch. 
  Thanks Nate - we owe you a cold beer!)
   
  This work is licensed under a MIT license https://opensource.org/licenses/MIT
 
  Copyright (c) 2016, Bare Conductive
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*******************************************************************************/

// compiler error handling
#include "Compiler_Errors.h"

// serial rate
#define baudRate 57600

// include the relevant libraries
#include <MPR121.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(12, 10); //Soft TX on 10, we don't use RX in this code

//Touch Board pin setup
byte pin_pitch = 0;       // pin to control pitch
byte pin_volume = 1;      // pint to control volume
byte pin_instr_up = 2;    // pin to control instrument change up 
byte pin_instr_down = 3;  // pin to control instrument change down

// Theremin setup
byte min_note = 40;  // lowest midi pitch
byte max_note = 100;  // highest midi pitch
byte min_volume = 0;  // lowest volume
byte max_volume = 127;  // highest volume (max is 127)
byte direction_pitch = 0;  // 0 = highest pitch with touch, 1 = lowest pitch with touch
int nonlinear_pitch = -10;  // fscale 'curve' param, 0=linear, -10 = max change at large distance
byte direction_volume = 0;  // 0 = loudest volume with touch, 1 = lowest volume with touch
int nonlinear_volume = -10; // fscale 'curve' param, 0=linear, -10 = max change at large distance

// midi instrument setup
byte instrument = 80 - 1;  // starting instrument (-1 b/c I think instrument list is off by 1)
byte min_instrument = 0;  // 
byte max_instrument = 127;  // max is 127

// initialization stuff for proximity
int level_pitch = 0;
int level_pitch_old = 0;
int level_volume = 0;
int level_volume_old = 0;
byte note=0;
byte note_old=0;
byte volume = 0;
byte volume_old = 0;
int min_level_pitch = 1000; // dummy start values - updated during running
int max_level_pitch = 0; // dummy start values - updated during running
int min_level_volume = 1000; // dummy start values - updated during running
int max_level_volume = 0; // dummy start values - updated during running

//VS1053 setup
//byte note = 0; //The MIDI note value to be played
byte resetMIDI = 8; //Tied to VS1053 Reset line
byte ledPin = 13; //MIDI traffic inidicator
byte velocity = 60;  // midi note velocity for turning on and off

void setup(){
  Serial.begin(baudRate);
  
  // uncomment the line below if you want to see Serial data from the start
  //while (!Serial);
  
  //Setup soft serial for MIDI control
  mySerial.begin(31250);
   
  // 0x5C is the MPR121 I2C address on the Bare Touch Board
  if(!MPR121.begin(0x5C)){ 
    Serial.println("error setting up MPR121");  
    switch(MPR121.getError()){
      case NO_ERROR:
        Serial.println("no error");
        break;  
      case ADDRESS_UNKNOWN:
        Serial.println("incorrect address");
        break;
      case READBACK_FAIL:
        Serial.println("readback failure");
        break;
      case OVERCURRENT_FLAG:
        Serial.println("overcurrent on REXT pin");
        break;      
      case OUT_OF_RANGE:
        Serial.println("electrode out of range");
        break;
      case NOT_INITED:
        Serial.println("not initialised");
        break;
      default:
        Serial.println("unknown error");
        break;      
    }
    while(1);
  }
  
  // pin 4 is the MPR121 interrupt on the Bare Touch Board
  MPR121.setInterruptPin(4);
  // initial data update
  MPR121.updateTouchData();

  //Reset the VS1053
  pinMode(resetMIDI, OUTPUT);
  digitalWrite(resetMIDI, LOW);
  delay(100);
  digitalWrite(resetMIDI, HIGH);
  delay(100);
  
  // initialise MIDI
  setupMidi();
}

void loop(){
  
   MPR121.updateAll();
   //MPR121.updateFilteredData();
   
   // change instrument
   if (MPR121.isNewTouch(pin_instr_up)){
     change_instrument(int(1));
//     constrain(instrument,min_instrument,max_instrument);
     talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command(55,0) 
     noteOff(0, note_old, velocity);
     noteOff(0, note, velocity);  
     Serial.print("instrument up: ");
     Serial.println(instrument);
   }
   else if (MPR121.isNewTouch(pin_instr_down)){
     change_instrument(int(-1));
     talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command(55,0) 
     noteOff(0, note_old, velocity);
     noteOff(0, note, velocity);  
     Serial.print("instrument down: ");
     Serial.println(instrument);
   }
   
   // pitch
   level_pitch_old = level_pitch;
   level_pitch = MPR121.getFilteredData(pin_pitch);     
   if (level_pitch != level_pitch_old){
     // dynamically setup level max and mins
     if (level_pitch > max_level_pitch){
       max_level_pitch = level_pitch;
     }
     if (level_pitch < min_level_pitch){
       min_level_pitch = level_pitch;
     }   
     // turn off notes if level rises near baseline
     if (fscale(min_level_pitch,max_level_pitch,0,1,level_pitch,nonlinear_pitch) >= 0.95) {
       noteOff(0, note_old, velocity);
       noteOff(0, note, velocity);  
       note_old = note;
       Serial.println("All notes off");
     }
     // set note
     else {
       if (direction_pitch == 0){
         note = fscale(min_level_pitch,max_level_pitch,max_note,min_note,level_pitch,nonlinear_pitch);
       }
       else if (direction_pitch == 1){
         note = fscale(min_level_pitch,max_level_pitch,min_note,max_note,level_pitch,nonlinear_pitch);
       }
       if (note != note_old){
         noteOn(0, note, velocity);  // turn on new note
         noteOff(0, note_old, velocity);  // turn off old note
         note_old = note;
         Serial.print("Note on: ");
         Serial.print(note);
         Serial.print(", Note off ");
         Serial.println(note_old);
       }
     }
   }
   
   // volume
   level_volume_old = level_volume;
   level_volume = MPR121.getFilteredData(pin_volume);     
   if (level_volume != level_volume_old){
     // dynamically setup level max and mins
     if (level_volume > max_level_volume){
       max_level_volume = level_volume;
     }
     if (level_volume < min_level_volume){
       min_level_volume = level_volume;
     }   
     // set volume
     else {
       if (direction_volume == 0){
         volume = fscale(min_level_volume,max_level_volume,max_volume,min_volume,level_volume,nonlinear_volume);
       }
       else if (direction_volume == 1){
         volume = fscale(min_level_volume,max_level_volume,min_volume,max_volume,level_volume,nonlinear_volume);
       }
       if (volume != volume_old){
         talkMIDI(0xB0, 0x07, volume); //0xB0 is channel message, set channel volume to near max (127)
         volume_old = volume;
         Serial.print("Volume: ");
         Serial.println(volume);
       }
     }
   } 
   
   // loop delay
   delay(50);
}    


// subfunctions

//void change_instrument(byte instrument,byte change) {
void change_instrument(int change) {
  if (change == 1){
    instrument++;
    if (instrument > max_instrument){
      instrument = min_instrument;
    }
  }
  else if (change == -1){
    instrument--;
    if (instrument < min_instrument){
      instrument = max_instrument;
    }
  }
}

// functions below are little helpers based on using the SoftwareSerial
// as a MIDI stream input to the VS1053 - all based on stuff from Nathan Seidle

//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

//Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
void talkMIDI(byte cmd, byte data1, byte data2) {
  digitalWrite(ledPin, HIGH);
  mySerial.write(cmd);
  mySerial.write(data1);

  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);

  digitalWrite(ledPin, LOW);
}

/*SETTING UP THE INSTRUMENT:
The below function "setupMidi()" is where the instrument bank is defined. Use the VS1053 instrument library
below to aid you in selecting your desire instrument from within the respective instrument bank
*/

void setupMidi(){
  
  //Volume
  talkMIDI(0xB0, 0x07, volume); //0xB0 is channel message, set channel volume to near max (127)
  
  //Melodic Instruments GM1
  //To Play "Electric Piano" (5):
  talkMIDI(0xB0, 0, 0x00); //Default bank GM1  
  //We change the instrument by changin the middle number in  the brackets 
  //talkMIDI(0xC0, number, 0); "number" can be any number from the melodic table below
  talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command(55,0) 
  
  //Percussion Instruments (GM1 + GM2) uncomment the code below to use
  // To play "Sticks" (31):
  //talkMIDI(0xB0, 0, 0x78); //Bank select: drums
  //talkMIDI(0xC0, 5, 0); //Set instrument number
  //Play note on channel 1 (0x90), some note value (note), middle velocity (60):
  //noteOn(0, 31, 60);
  //NOTE: need to figure out how to map this... or is it the same as white keys?
  
}

/*MIDI INSTRUMENT LIBRARY: 

MELODIC INSTRUMENTS (GM1) 
When using the Melodic bank (0x79 - same as default), open chooses an instrument and the octave to map 
To use these instruments below change "number" in talkMIDI(0xC0, number, 0) in setupMidi()


1   Acoustic Grand Piano       33  Acoustic Bass             65  Soprano Sax           97   Rain (FX 1)
2   Bright Acoustic Piano      34  Electric Bass (finger)    66  Alto Sax              98   Sound Track (FX 2)
3   Electric Grand Piano       35  Electric Bass (pick)      67  Tenor Sax             99   Crystal (FX 3)
4   Honky-tonk Piano           36  Fretless Bass             68  Baritone Sax          100  Atmosphere (FX 4)
5   Electric Piano 1           37  Slap Bass 1               69  Oboe                  101  Brigthness (FX 5)
6   Electric Piano 2           38  Slap Bass 2               70  English Horn          102  Goblins (FX 6)
7   Harpsichord                39  Synth Bass 1              71  Bassoon               103  Echoes (FX 7)
8   Clavi                      40  Synth Bass 2              72  Clarinet              104  Sci-fi (FX 8) 
9   Celesta                    41  Violin                    73  Piccolo               105  Sitar
10  Glockenspiel               42  Viola                     74  Flute                 106  Banjo
11  Music Box                  43  Cello                     75  Recorder              107  Shamisen
12  Vibraphone                 44  Contrabass                76  Pan Flute             108  Koto
13  Marimba                    45  Tremolo Strings           77  Blown Bottle          109  Kalimba
14  Xylophone                  46  Pizzicato Strings         78  Shakuhachi            110  Bag Pipe
15  Tubular Bells              47  Orchestral Harp           79  Whistle               111  Fiddle
16  Dulcimer                   48  Trimpani                  80  Ocarina               112  Shanai
17  Drawbar Organ              49  String Ensembles 1        81  Square Lead (Lead 1)  113  Tinkle Bell
18  Percussive Organ           50  String Ensembles 2        82  Saw Lead (Lead)       114  Agogo
19  Rock Organ                 51  Synth Strings 1           83  Calliope (Lead 3)     115  Pitched Percussion
20  Church Organ               52  Synth Strings 2           84  Chiff Lead (Lead 4)   116  Woodblock
21  Reed Organ                 53  Choir Aahs                85  Charang Lead (Lead 5) 117  Taiko
22  Accordion                  54  Voice oohs                86  Voice Lead (Lead)     118  Melodic Tom
23  Harmonica                  55  Synth Voice               87  Fifths Lead (Lead 7)  119  Synth Drum
24  Tango Accordion            56  Orchestra Hit             88  Bass + Lead (Lead 8)  120  Reverse Cymbal
25  Acoustic Guitar (nylon)    57  Trumpet                   89  New Age (Pad 1)       121  Guitar Fret Noise
26  Acoutstic Guitar (steel)   58  Trombone                  90  Warm Pad (Pad 2)      122  Breath Noise
27  Electric Guitar (jazz)     59  Tuba                      91  Polysynth (Pad 3)     123  Seashore 
28  Electric Guitar (clean)    60  Muted Trumpet             92  Choir (Pad 4)         124  Bird Tweet
29  Electric Guitar (muted)    61  French Horn               93  Bowed (Pad 5)         125  Telephone Ring
30  Overdriven Guitar          62  Brass Section             94  Metallic (Pad 6)      126  Helicopter
31  Distortion Guitar          63  Synth Brass 1             95  Halo (Pad 7)          127  Applause
32  Guitar Harmonics           64  Synth Brass 2             96  Sweep (Pad 8)         128  Gunshot  

PERCUSSION INSTRUMENTS (GM1 + GM2)

When in the drum bank (0x78), there are not different instruments, only different notes.
To play the different sounds, select an instrument # like 5, then play notes 27 to 87.
 
27  High Q                     43  High Floor Tom            59  Ride Cymbal 2         75  Claves 
28  Slap                       44  Pedal Hi-hat [EXC 1]      60  High Bongo            76  Hi Wood Block
29  Scratch Push [EXC 7]       45  Low Tom                   61  Low Bongo             77  Low Wood Block
30  Srcatch Pull [EXC 7]       46  Open Hi-hat [EXC 1]       62  Mute Hi Conga         78  Mute Cuica [EXC 4] 
31  Sticks                     47  Low-Mid Tom               63  Open Hi Conga         79  Open Cuica [EXC 4]
32  Square Click               48  High Mid Tom              64  Low Conga             80  Mute Triangle [EXC 5]
33  Metronome Click            49  Crash Cymbal 1            65  High Timbale          81  Open Triangle [EXC 5]
34  Metronome Bell             50  High Tom                  66  Low Timbale           82  Shaker
35  Acoustic Bass Drum         51  Ride Cymbal 1             67  High Agogo            83 Jingle bell
36  Bass Drum 1                52  Chinese Cymbal            68  Low Agogo             84  Bell tree
37  Side Stick                 53  Ride Bell                 69  Casbasa               85  Castanets
38  Acoustic Snare             54  Tambourine                70  Maracas               86  Mute Surdo [EXC 6] 
39  Hand Clap                  55  Splash Cymbal             71  Short Whistle [EXC 2] 87  Open Surdo [EXC 6]
40  Electric Snare             56  Cow bell                  72  Long Whistle [EXC 2]  
41  Low Floor Tom              57  Crash Cymbal 2            73  Short Guiro [EXC 3]
42  Closed Hi-hat [EXC 1]      58  Vibra-slap                74  Long Guiro [EXC 3]

*/


float fscale( float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve){

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;


  // condition curve parameter
  // limit range

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  /*
   Serial.println(curve * 100, DEC);   // multply by 100 to preserve resolution  
   Serial.println();
   */

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin){
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float

  /*
  Serial.print(OriginalRange, DEC);  
   Serial.print("   ");  
   Serial.print(NewRange, DEC);  
   Serial.print("   ");  
   Serial.println(zeroRefCurVal, DEC);  
   Serial.println();  
   */

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax ) {
    return 0;
  }

  if (invFlag == 0){
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else     // invert the ranges
  {  
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}
