#include "speksat.h"
#include <stdint.h>
#include <Arduino.h>

/* I'm confused on how the frame contains 7 channels, but there's 8 channels. Are the last two lower refresh rate and alternate? Only hardware will tell */

void speksat_init(){
  Serial.begin(115200);
}

int get_sat_data(char bytes[]){
  if (Serial.available() >= NUM_SER_BYTES){
  	for (int i = 0; i < NUM_SER_BYTES; ++i){
  		bytes[i] = Serial.read();
  	}
  	return 0;
  }
  return 1;
}

int process_channels(char serdata[], uint16_t channels[]){
	int chId;
	int channel;
	for (int i = NON_CH_FRAMES; i < (NUM_DATA_FRAMES * 2) + NON_CH_FRAMES; i += 2){ //start at 2, go to 16, by 2s
		channel = (serdata[i] << 8) + serdata[i]; //2 + 3, 4 + 5, etc
		chId = ((channel & 0x7FFF) >> 11);
		if (chId > NUM_CHANNELS){
			return -1; //this might triggrer on channel 8
		}
		channels[chId] = channel & (0x7FF);
	}
}
