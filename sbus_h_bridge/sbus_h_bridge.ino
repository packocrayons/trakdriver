#include <SBUS.h>
/*Hello I'm brydon gibson and I cheated and used someone else's library but I promise I did all the other stuff myself*/

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
#define NUM_CHANNELS 3
#define LED_CHANNEL 3

//mixing is done in the channel order (TAER, but in this case TA[SW1]) and is a (float) multiplier of how much that channels' variance from 0 affects the motor (basic multiwii mixing)
//TODO: read the betaflight code because those guys actually know how to write code
#define M0_MMIX {1, 1, 0}
#define M1_MMIX {-1, -1, 0}
#define L_MMIX {0, 0, 1}

SBUS sbus(Serial);

uint8_t pwmCntr = 0;

typedef struct motor{
  int high_PFET;
  int high_NFET;
  int low_PFET;
  int low_NFET;
  int spd;
  int dir;
  float mix[NUM_CHANNELS];
}motor;

void setup() {
  cli();
  // put your setup code here, to run once:
  TCCR0A = 0;
  TCCR0B = 0;
  TCNT0 = 0;
  OCR0A = 124;
  // no prescaling - fully software PWM TCCR0B |= 1 << CS01; //clk/8 prescaling - potentially long interrupt
  TCCR0A |= 1 << WGM01;
  TIMSK0 |= 1 << OCIE0A
  motor motors[2];
  motor led; //cheating so I don't have to keep multiple copies of data
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
  sbus.begin();
}

void mixmotors(motor m. int len){
	int t;
	int mixsum;
	for (int i = 0; i < len; ++i){
		t = 0;
		mixsum = 0;
		for (int c = 0; c < NUM_CHANNELS; ++c){
			mixsum += m[i].mix[c];
			t += round(m[i].mix[c] * (float)(sbus.getChannel(c) - 1024));
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
    if (swPWMcounter > spd){
      digitalWrite(m.high_PFET, HIGH); //turn off (temporarily) the high PFET
    } else {
      digitalWrite(m.high_PFET, LOW); //turn on (temporarily) the high PFET
    }
  }
  if (m.dir == DIR_REVERSE){ //power flows from BAT -> low_PFET -> MOTOR -> high_NFET -> GND
    digitalWrite(m.high_PFET, HIGH); //turn off the high PFET
    digitalWrite(m.low_NFET, LOW); //turn off the low PFET
    digitalWrite(m.high_NFET, HIGH); //turn on the high NFET
    if (swPWMcounter > spd){
      digitalWrite(m.low_PFET, HIGH); //turn off (temporarily) the low PFET
    } else {
      digitalWrite(m.low_PFET, LOW); //turn on (temporarily) the low PFET
    }
  }
}

void loop() {

  sbus.process(); //get new channel values (if available)

  mixmotors(motors, NUM_MOTORS);
  mixmotors(led, 1);
  for (int i = 0; i < NUM_MOTORS; ++i){
    writeMotor(pwmCntr, motors[i]);
  }
  if (ledCNTR >= led.spd){
  	digitalWrite(led.high_PFET, LOW);
  }
  OCR0A = motor[0].spd;
  OCR0B = motor[1].spd;
}

isr(TIMER0_COMPA_vect){//motor 0's PWM is OCR0A
  digitalWrite(motor[0].high_PFET, HIGH);
  digitalWrite(motor[0].low_PFET, HIGH); //direction doesn't matter here, wasting a GPIO write but it's probably just as fast as a compare
}

isr(TIMER0_COMPB_vect){ //motor 1's PWM is OCR0B
  digitalWrite(motor[1].high_PFET, HIGH);
  digitalWrite(motor[1].low_PFET, HIGH); //direction doesn't matter here, wasting a GPIO write but it's probably just as fast as a compare
}

isr(TIMER0_OVF_vect){
 ledCNTR++;//softwarePWM for the LED, 500 ish hz is plenty
 if (ledCNTR == 0) digitalWrite(led.high_PFET, HIGH); 
 for (int i = 0; i < NUM_MOTORS; ++i){
  if (motor[i].spd > 0){
    if (motor[i].dir == DIR_FORWARD){
      digitalWrite(motor[i].high_PFET, LOW); //turn it on
    } else {
      digitalWrite(motor[i].low_PFET, LOW); //turn on reverse
    }
  } else {
    digitalWrite(motor[i].high_PFET, HIGH);
    digitalWrite(motor[i].low_PFET, HIGH); //turn everything off we don't want any current flowing. Trying to eliminate race conditions here since the other fets are modified outside of the ISR
  }
 }
}