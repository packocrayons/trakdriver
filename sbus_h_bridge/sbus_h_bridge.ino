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

uint8_t pwmCntr = 0;

typedef struct motor{
  int high_PFET;
  int high_NFET;
  int low_PFET;
  int low_NFET;
  int spd;
  int dir;  
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
  motors[0] = {.high_PFET = 7, .high_NFET = 10, .low_PFET = 2, .low_NFET = 9, .spd = 0, .dir = DIR_FORWARD};
  motors[1] = {.high_PFET = 6, .high_NFET = 4, .low_PFET =  5, .low_NFET = 3, .spd = 0, .dir = DIR_FORWARD};  
  //protip - make the LED driver a "motor" with high_PFET == the dim pin and set dir to forward. spd is then LED brightness
  for (int i = 0; i < 2; ++i){
    pinMode(motors[i].high_PFET, OUTPUT);
    pinMode(motors[i].high_NFET, OUTPUT);
    pinMode(motors[i].low_PFET, OUTPUT);
    pinMode(motors[i].low_NFET, OUTPUT);
  }

  sei();
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
  //TODO: Serial handling for sbus
  //TODO: Mixing in the driver or in the tx?
  for (int i = 0; i < NUM_MOTORS; ++i){
    writeMotor(pwmCntr, motors[i]);
  }
}

isr(TIMER0_COMPA_vect){
  pwmCntr++; //will overflow at 255
}
