#include <MIDI.h>
int pwm_pins[6] = {4, 5, 6, 7, 8, 9}; //the DOs assigned to the channels
int note_vals[6] = {100, 101, 102, 103, 104, 105}; //the midi notes assigned to the channels
int channel_count = 6; //number of channels (and notes)

//momentary values
int note_val_now;
int note_vel_now;
int pwm_value_now;

int midi_channel_now; //midi channel of momentary messages. this is a central variable that defines the flow
byte midi_type_now;

//variables for note repeat functionality
float freq_now = 440; //place holder
int channel_active[6] = {0, 0, 0, 0, 0, 0}; //expresses which of the channels plays a note at the moment
int channel_index_now = 0; // for iterating in tone generation
int channel_pwms[6] = {0, 0, 0, 0, 0, 0}; //the volume of each note
int channel_timeinterval[6] = {100, 100, 100, 100, 100, 100}; //a state change every x ms

//variables for time counter and notes played
unsigned long time_now; //now time for every loop
unsigned long time_last[6] = {0, 0, 0, 0, 0, 0};
unsigned long time_next[6] = {0, 0, 0, 0, 0, 0};
int this_event = 1;
int next_event[6] = {0, 0, 0, 0, 0, 0};

//variables for safety functionality that will turn down all channels that have been on for longer than 1000 ms
unsigned long time_high[6] = {0, 0, 0, 0, 0, 0};

// yet another savety routine. In case the device has not received any midi for longer than 5 sec, it will also switch off all channnels once
unsigned long time_lastcommand;
int communication_active=1;

MIDI_CREATE_DEFAULT_INSTANCE();

//setup here
void setup() {
  //set pinmodes for all buttons
  for (int thisPin = 0; thisPin < channel_count; thisPin++) {
    pinMode(pwm_pins[thisPin], OUTPUT);
  }
  
  //Serial.begin(9600);
  MIDI.begin(MIDI_CHANNEL_OMNI);      // Launch MIDI and listen to all incoming events
  //MIDI.begin(1); // Launch MIDI and listen to incoming events from channel 1
}


//loop here
void loop() {
  time_now = millis();
  
  // Check if we have received a message
  if (MIDI.read())
    {
      midi_channel_now = MIDI.getChannel(); // find out which channel the message comes from 
      midi_type_now = MIDI.getType();

      //harmony handling
      if (midi_channel_now != 1) {
        switch(midi_type_now) {
          case midi::NoteOn: {
            channel_index_now = midi_channel_now - 2; //minus two because the notes start with midi channel 2 but the index is zero in this case
            
            note_val_now = MIDI.getData1(); //data 1 is note, data2 intensity
            note_vel_now = MIDI.getData2(); 

            //note val will be used to compute the frequency in normal maths: hz = 440* 2^((note-69)/12)
            freq_now = pow (2.0, ((float)(note_val_now-69)/12.0)); //to yield ms we need to say 1/freq_now * 1000 
            
            channel_timeinterval[channel_index_now] = (1/freq_now)*100; //normally 100 but we transpose everything down an octave
            //strictly speaking it is even two ocatves down because it changes phase one 
            
            channel_active[channel_index_now] = 1; //placeholder for array
            channel_pwms[channel_index_now] = map(note_vel_now, 0, 128, 0, 256); //variante 3 ohen analog
            
            communication_active=1; //log that communication is on
            time_lastcommand=time_now;
                 
            break;
           }
        
          case midi::NoteOff: {
            note_val_now = MIDI.getData1(); //data 1 is note, data2 intensity
            channel_active[channel_index_now] = 0;
            analogWrite(pwm_pins[channel_index_now], 0); //make sure it turns off
            next_event[channel_index_now] = 1; //makes sure that when note is repeated it start with an "on"
            break; 
           } 
         }
      }
     

      //drum functionality handling
      if (midi_channel_now==1) {
        switch(midi_type_now) {
          case midi::NoteOn: {
            
            note_val_now = MIDI.getData1(); //data 1 is note, data2 intensity
            note_vel_now = MIDI.getData2(); 
            //pwm_value_now = map(note_vel_now, 0, 127, 0, 256); //variante 3 ohen analog
            pwm_value_now = map(note_vel_now, 0, 128, 0, 256); //variante 3 ohen analog

            //now go through the note values and check which one it is
            for (int thisChannel = 0; thisChannel < channel_count; thisChannel++) {
              if (note_val_now == note_vals[thisChannel]) {
                analogWrite(pwm_pins[thisChannel], pwm_value_now);
                time_high[thisChannel] = time_now; //log on event   
              }
            
            }
            communication_active=1; //log that communication is on
            time_lastcommand=time_now;
            break;
          }
        
          case midi::NoteOff: {
            note_val_now = MIDI.getData1(); //data 1 is note, data2 intensity
            for (int thisChannel = 0; thisChannel < channel_count; thisChannel++) {
              if (note_val_now == note_vals[thisChannel]) {
                 analogWrite(pwm_pins[thisChannel], 0);  
                }
            }
            break;
         } 
      }
    }
   }
    

    //now iterate through the vector of notes and check if any is supposed to play. This is for the harmony functionality but also switching off after 1000ms
    if (communication_active==1) {
      for (int thisChannel = 0; thisChannel < channel_count; thisChannel++) {
                
                if (channel_active[thisChannel]==1) {
                  if (time_now > time_next[thisChannel]) {
                    this_event = next_event[thisChannel];
            
                    if (this_event==1) {
                      analogWrite(pwm_pins[thisChannel], channel_pwms[thisChannel]);
                      time_high[thisChannel] = time_now; //log on event
                    }
  
  
                    if (this_event==0) {
                      analogWrite(pwm_pins[thisChannel], 0);
                    }
  
            
                    //now set new values
                    time_next[thisChannel] = time_now + channel_timeinterval[thisChannel];
                    next_event[thisChannel] = !this_event;
                  }    
               }
         }
      }

       //this is a safety feature that iterates through the notes and checks if they have been "on" for more then a second. if true, they are truned off
       for (int thisChannel = 0; thisChannel < channel_count; thisChannel++) {
          if (time_now > time_high[thisChannel] + 1000) {
              analogWrite(pwm_pins[thisChannel], 0);
            }    
        }  
        
        //second savety routine: switch off all channels after not having received midi commands in a while
        if (communication_active==1) {
          if (time_now > time_lastcommand + 3000) {
            for (int thisChannel = 0; thisChannel < channel_count; thisChannel++) {
                analogWrite(pwm_pins[thisChannel], 0);
            }
            communication_active=0; //avoid that this is being repeated in every loop
          }
        }    
}
