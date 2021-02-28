#define DISABLE_INTERNAL_TIMER0_OVF_VECT

//#include "speksat.h"
#include "sbus.h"
#include <stdint.h>
/*Hello I'm brydon gibson and ~~I cheated and used someone else's library but I promise I did all the other stuff myself~~ fuck it I'll do it myself*/

/*IC2G2 = OC1B
IC3G2 = OC1A only one of these two can be on at a time

IC2G1 = PD7
IC3G1 = PD2 Only one of these can be on at a time, ICxG1 and ICxG2 cannot be on at the same time
*/

#define RX_GET_DATA get_sbus_data
#define RX_PROCESS_CHANNELS sbus_process_channels
#define RX_INIT sbus_init

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
#define L_MMIX { 1, 0, 0}

int ledToggle = 1;
int interrupted = 0;

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
  
  TCCR0A = 0;
  TCCR0B |= 1 << CS00;
  TCNT0 = 0;
  // no prescaling - fully software PWM TCCR0B |= 1 << CS01; //clk/8 prescaling - potentially long interrupt
  //TCCR0A |= 1 << WGM01;
  TIMSK0 |= 1 << OCIE0A | 1 << OCIE0B | 1 << TOIE0;
  motors[0] = {.high_PFET = 7, .high_NFET = 10, .low_PFET = 2, .low_NFET = 9, .spd = 0, .dir = DIR_FORWARD, .mix = M0_MMIX};
  motors[1] = {.high_PFET = 6, .high_NFET = 4, .low_PFET =  5, .low_NFET = 3, .spd = 0, .dir = DIR_FORWARD, .mix = M1_MMIX};
  led = {.high_PFET = 13, .high_NFET = 0, .low_PFET =  0, .low_NFET = 0, .spd = 0, .dir = DIR_FORWARD, .mix = L_MMIX};   
  //protip - make the LED driver a "motor" with high_PFET == the dim pin and set dir to forward. spd is then LED brightness
  for (int i = 0; i < 2; ++i){
    pinMode(motors[i].high_PFET, OUTPUT);
    pinMode(motors[i].high_NFET, OUTPUT);
    pinMode(motors[i].low_PFET, OUTPUT);
    pinMode(motors[i].low_NFET, OUTPUT);
  }
  pinMode(led.high_PFET, OUTPUT);

  RX_INIT();

  sei();
}

void mixmotors(motor m[], int len, uint16_t channels[]){
	int t;
	float mixsum;
	for (int i = 0; i < len; ++i){
		t = 0;
		mixsum = 0;
		for (int c = 0; c < NUM_USED_CHANNELS; ++c){
			mixsum += m[i].mix[c];
			t += (m[i].mix[c] * (channels[c] - CH_MID));
		}
		if (t < 0){
			m[i].dir = DIR_REVERSE;
		} else {
			m[i].dir = DIR_FORWARD;
		}
		m[i].spd = map(abs(t), 0, ((CH_MAX / 2) * mixsum), 0, 255);
	}

}

void writeMotor(motor m){
  if (m.dir == DIR_FORWARD){ //power flows from BAT -> high_PFET -> MOTOR -> low_NFET -> GND
    digitalWrite(m.high_NFET, LOW); //turn off the high NFET
    digitalWrite(m.low_PFET, HIGH); //turn off the low PFET
    digitalWrite(m.low_NFET, HIGH); //turn on the low NFET
  }
  if (m.dir == DIR_REVERSE){ //power flows from BAT -> low_PFET -> MOTOR -> high_NFET -> GND
    digitalWrite(m.high_PFET, HIGH); //turn off the high PFET
    digitalWrite(m.low_NFET, LOW); //turn off the low PFET
    digitalWrite(m.high_NFET, HIGH); //turn on the high NFET
  }
}

void loop() {
  if (!RX_GET_DATA(serdata)){ // 0 is success
    (RX_PROCESS_CHANNELS(serdata, channels));
    mixmotors(motors, NUM_MOTORS, channels);
    mixmotors(&led, 1, channels);
    for (int i = 0; i < 2; ++i){
      writeMotor(motors[i]);
    }
  } else {
    digitalWrite(13, ledToggle);
    ledToggle = ~ledToggle & 0x1;
  }
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
 if (ledCNTR == 0){
  digitalWrite(led.high_PFET, HIGH);
 }
 if (ledCNTR > led.spd){
  digitalWrite(led.high_PFET, LOW);
 }
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
    interrupted = 1;
  }
 }
 OCR0A = motors[0].spd;
 OCR0B = motors[1].spd;
}
