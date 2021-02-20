#include "speksat.h"
#include <stdint.h>
/*Hello I'm brydon gibson and ~~I cheated and used someone else's library but I promise I did all the other stuff myself~~ fuck it I'll do it myself*/

/*IC2G2 = OC1B
IC3G2 = OC1A only one of these two can be on at a time

IC2G1 = PD7
IC3G1 = PD2 Only one of these can be on at a time, ICxG1 and ICxG2 cannot be on at the same time
*/


#define DIR_FORWARD 0
#define DIR_REVERSE 1

#define MOTOR_LEFT 0
#define MOTOR_RIGHT 1

#define NUM_MOTORS 2
#define NUM_USED_CHANNELS 3
#define LED_CHANNEL 3

//mixing is done in the channel order (TAER, but in this case TA[SW1]) and is a (float) multiplier of how much that channels' variance from 0 affects the motor (basic multiwii mixing)
//TODO: read the betaflight code because those guys actually know how to write code
#define M0_MMIX {1, 1, 0}
#define M1_MMIX {-1, -1, 0}
#define L_MMIX {0, 0, 1}

uint8_t pwmCntr = 0;
uint8_t ledCNTR = 0;

typedef struct motor{
  int high_PFET;
  int high_NFET;
  int low_PFET;
  int low_NFET;
  int spd;
  int dir;
  float mix[NUM_USED_CHANNELS];
}motor;

char serdata[NUM_SER_BYTES];
uint16_t channels[NUM_CHANNELS];


motor motors[2];
motor led; //cheating so I don't have to keep multiple copies of data

void setup() {
  cli();
  // put your setup code here, to run once:
  TCCR0A = 0;
  TCCR0B = 0;
  TCNT0 = 0;
  OCR0A = 124;
  // no prescaling - fully software PWM TCCR0B |= 1 << CS01; //clk/8 prescaling - potentially long interrupt
  TCCR0A |= 1 << WGM01;
  TIMSK0 |= 1 << OCIE0A;
  motors[0] = {.high_PFET = 7, .high_NFET = 10, .low_PFET = 2, .low_NFET = 9, .spd = 0, .dir = DIR_FORWARD, .mix = M0_MMIX};
  motors[1] = {.high_PFET = 6, .high_NFET = 4, .low_PFET =  5, .low_NFET = 3, .spd = 0, .dir = DIR_FORWARD, .mix = M1_MMIX};
  led = {.high_PFET = 8, .high_NFET = 0, .low_PFET =  0, .low_NFET = 0, .spd = 0, .dir = DIR_FORWARD, .mix = L_MMIX};   
  //protip - make the LED driver a "motor" with high_PFET == the dim pin and set dir to forward. spd is then LED brightness
  for (int i = 0; i < 2; ++i){
    pinMode(motors[i].high_PFET, OUTPUT);
    pinMode(motors[i].high_NFET, OUTPUT);
    pinMode(motors[i].low_PFET, OUTPUT);
    pinMode(motors[i].low_NFET, OUTPUT);
  }

  sei();
}

void mixmotors(motor m[], int len, uint16_t channels[]){
	int t;
	int mixsum;
	for (int i = 0; i < len; ++i){
		t = 0;
		mixsum = 0;
		for (int c = 0; c < NUM_USED_CHANNELS; ++c){
			mixsum += m[i].mix[c];
			t += round(m[i].mix[c] * (float)(channels[c] - 1024));
		}
		if (t < 0){
			m[i].dir = DIR_REVERSE;
		} else {
			m[i].dir = DIR_FORWARD;
		}
		m[i].spd = map(abs(t), 0, (1024 * mixsum), 0, 255);
	}

}

void writeMotor(int swPWMcounter, motor m){
  if (m.dir == DIR_FORWARD){ //power flows from BAT -> high_PFET -> MOTOR -> low_NFET -> GND
    digitalWrite(m.high_NFET, LOW); //turn off the high NFET
    digitalWrite(m.low_PFET, HIGH); //turn off the low PFET
    digitalWrite(m.low_NFET, HIGH); //turn on the low NFET
    if (swPWMcounter > m.spd){
      digitalWrite(m.high_PFET, HIGH); //turn off (temporarily) the high PFET
    } else {
      digitalWrite(m.high_PFET, LOW); //turn on (temporarily) the high PFET
    }
  }
  if (m.dir == DIR_REVERSE){ //power flows from BAT -> low_PFET -> MOTOR -> high_NFET -> GND
    digitalWrite(m.high_PFET, HIGH); //turn off the high PFET
    digitalWrite(m.low_NFET, LOW); //turn off the low PFET
    digitalWrite(m.high_NFET, HIGH); //turn on the high NFET
    if (swPWMcounter > m.spd){
      digitalWrite(m.low_PFET, HIGH); //turn off (temporarily) the low PFET
    } else {
      digitalWrite(m.low_PFET, LOW); //turn on (temporarily) the low PFET
    }
  }
}

void loop() {
  int newData = 0;

  if (!get_sat_data(serdata)){ // 0 is success
    process_channels(serdata, channels);
  }

  mixmotors(motors, NUM_MOTORS, channels);
  mixmotors(&led, 1, channels);
  for (int i = 0; i < NUM_MOTORS; ++i){
    writeMotor(pwmCntr, motors[i]);
  }
  if (ledCNTR >= led.spd){
  	digitalWrite(led.high_PFET, LOW);
  }
  OCR0A = motors[0].spd;
  OCR0B = motors[1].spd;
}

ISR(TIMER0_COMPA_vect){//motor 0's PWM is OCR0A
  digitalWrite(motors[0].high_PFET, HIGH);
  digitalWrite(motors[0].low_PFET, HIGH); //direction doesn't matter here, wasting a GPIO write but it's probably just as fast as a compare
}

ISR(TIMER0_COMPB_vect){ //motor 1's PWM is OCR0B
  digitalWrite(motors[1].high_PFET, HIGH);
  digitalWrite(motors[1].low_PFET, HIGH); //direction doesn't matter here, wasting a GPIO write but it's probably just as fast as a compare
}

ISR(TIMER0_OVF_vect){
 ledCNTR++;//softwarePWM for the LED, 500 ish hz is plenty
 if (ledCNTR == 0) digitalWrite(led.high_PFET, HIGH); 
 for (int i = 0; i < NUM_MOTORS; ++i){
  if (motors[i].spd > 0){
    if (motors[i].dir == DIR_FORWARD){
      digitalWrite(motors[i].high_PFET, LOW); //turn it on
    } else {
      digitalWrite(motors[i].low_PFET, LOW); //turn on reverse
    }
  } else {
    digitalWrite(motors[i].high_PFET, HIGH);
    digitalWrite(motors[i].low_PFET, HIGH); //turn everything off we don't want any current flowing. Trying to eliminate race conditions here since the other fets are modified outside of the ISR
  }
 }
}
