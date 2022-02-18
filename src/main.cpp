/*
  This is the source for the EMMMA-K "Master". This processor handles the touch pins for the
  first 11 Kalimba keys and creates USB-MIDI (see the build flags in platformio.ini). It
  also requests data from the "Slave" over serial 2 and then receives data for the status of the
  other 11 touch pins, 6 for the remaining Kalimba keys and the remaining 5 for function control
  (not being read or used right now).

  The Master also receives telemetry data on Serial 3 from an orientation sensor which is a flight 
  controller for a quadcopter. This gives roll, pitch and yaw information which can be used for special
  effects (experimental). As an initial proof of concept I use the pitch axis to do pitch bends.
  This effect is enabled by a touch key on the back of the case.
*/

#include <touchablePin.h>

// pins (11 local)
touchablePin pins[11];
uint8_t pinNumbers[] = {15,4,16,3,17,23,18,1,19,0,22};

// notes (17 total) and scales (choose one)
uint8_t majorscale[] = {2, 2, 1, 2, 2, 2, 1};
uint8_t minorscale[] = {2, 1, 2, 2, 1, 2, 2};
uint8_t pentascale[] = {2, 2, 3, 2, 3};
uint8_t minorpentascale[] = {3, 2, 2, 3, 2};
uint8_t minorbluesscale[] = {3, 2, 1, 1, 3, 2};

bool notesOn[] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
//uint8_t midiValues[] = {60,62,64,65,67,69,71,72,74,76,77,79,81,83,84,86,88}; // C-Maj
//uint8_t midiValues[] = {60,62,64,67,69,72,74,76,79,81,84,86,88,91,93,96,98}; // C-Pentatonic
//uint8_t midiValues[] = {60,63,65,67,70,72,75,77,79,82,84,87,89,91,94,96,99}; // C-Minor-Pentatonic
uint8_t midiValues[] = {60,62,63,65,67,68,70,72,74,75,77,79,80,82,84,86,87}; // Cm (Corona Antibodies 2)
//uint8_t midiValues[] = {61,63,64,66,68,69,71,73,75,76,78,80,81,83,85,87,88}; // C#m (Corona Antibodies 3 - Moonlight Sonata)

// Root note offsets for key change
#define C       0
#define Csharp  1
#define D       2
#define Dsharp  3
#define E       4
#define F       5
#define Fsharp  6
#define G       7
#define Gsharp  8
#define A       9
#define Asharp  10
#define B       11

#define OCTAVE 12
#define KEY G - OCTAVE // choose desired key and octave offset here

bool enableBend = false;

void scaleToMidiValues(uint8_t *scale, uint8_t size)
{
  for(int i = 1;  i < 17; i++)
    midiValues[i] = midiValues[i - 1] + scale[(i - 1) % size];
}

void serial3state()
{
  while(Serial3.available())
  {
    // This is for reading the telemetry data from the flight controller

    uint8_t c = Serial3.read(); 

    static int state = 0;
    static uint8_t idx = 0;
    static int8_t rpy[6];

    switch(state)
    {
      case 0:
        if(c == '$')
            state = 1;
         
        break;

      case 1:
        if(c == 'T')
            state = 2;
        else
            state = 0;
         
        break;

      case 2:
        if(c == 'A')
            state = 3;
        else
            state = 0;
         
        break;

      case 3:
        if(idx++ <  6)
            rpy[idx]  = c;
        else
            state = 4;
        break;

      case 4:
        int16_t p = rpy[1];
        int16_t r = rpy[3];
        int16_t y = rpy[5];

        // Use the pitch axis for pitch bend
        static int16_t lastp = p;
        if(enableBend)
        {
          if(p > 0)
            usbMIDI.sendPitchBend(p << 9, 1);
          else
            usbMIDI.sendPitchBend(0, 1);   

          lastp = p;
        }
        else if(lastp)
          usbMIDI.sendPitchBend(0, 1);

        // You could experiment with the roll access here
        static int16_t lastr = r;
        if(r != lastr)
        {
          //usbMIDI.sendControlChange(11, 255 - abs(r), 1);
          lastr = r;
        }

        idx = 0;
        state = 0;
        break;
    }
  }
}

void setup() 
{ 
  for(int i = 0; i < 11; i++)
      pins[i].setPin(pinNumbers[i]);

  Serial2.begin(115200);
  Serial3.begin(9600);
  //usbMIDI.sendNoteOff(midiValues[7] - OCTAVE * 3, 99, 1); // this was for Hurdy Gurdy drone string
}

void loop() 
{
  Serial2.write(0xA5); // ask for data from the slave
  
  for(int i = 0; i < 11; i++)
  {
    serial3state();

    if(pins[i].isTouched())
    {
      if(!notesOn[i])
      {
        usbMIDI.sendNoteOn(midiValues[i] + KEY, 99, 1); 
        notesOn[i] = true;
      }
    }
    else
    {
      if(notesOn[i])
      {
        usbMIDI.sendNoteOff(midiValues[i] + KEY, 99, 1); 
        notesOn[i] = false;
      }
    }      
  }

  if(Serial2.available())
  {
    uint8_t p = Serial2.read(); // get the touched byte from the slave
#if 1
    if(p & 0x40)
      enableBend = true;
      //enableBend = false; // use this line if the pitch bend gets annoying :-)
    else
      enableBend = false;
#else // this is for the Hurdy Gurdy drone string which replaces the pitch bend enable function
    static bool droneOn = false;
    static bool droneToggle = false;
    if(p & 0x40 && droneToggle)
    {
      if(!droneOn)
      {
        droneOn = true;
        droneToggle  = false;
        usbMIDI.sendNoteOn(midiValues[7] - OCTAVE * 3, 99, 1); 
      }
      else
      {
        droneOn = false;
        droneToggle  = false;
        //usbMIDI.sendNoteOff(midiValues[7] - OCTAVE * 3, 99, 1); 
      }
    }
    else
    {
      droneToggle = true;
    }
#endif    
    for(int i = 0; i < 6; i++)  // process those remote pins
    {
      if(p & (1 << i))
      {
        if(!notesOn[i + 11])
        {
          usbMIDI.sendNoteOn(midiValues[i + 11] + KEY, 99, 1); 
          notesOn[i + 11] = true;
        }
      }
      else
      {
        if(notesOn[i + 11])
        {
          usbMIDI.sendNoteOff(midiValues[i +  11] + KEY, 99, 1); 
          notesOn[i + 11] = false;
        }  
      }    
    }
  }

  while(usbMIDI.read()) {} // this clears any incoming MIDI if any (need to do this)
}
