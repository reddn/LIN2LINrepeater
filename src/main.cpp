#include <Arduino.h>
#include <CustomSoftwareSerial.h>


#include <Rotary.h>

// #define Serialwrite Serial.write
#define Serialwrite customSerial->write  //on change fix setup

CustomSoftwareSerial* customSerial;
uint8_t lkas_off_array[][4] =  {{0x20,0x80,0xc0,0xa0},{0x00,0x80,0xc0,0xc0}};
uint8_t counterbit = 0;

Rotary r = Rotary(2, 3);

uint8_t buff[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t buffi = 0;
uint8_t data[4];
uint8_t last_processed[2];
volatile bool lkas_active = false;
uint8_t errorcount = 0;
uint8_t sendlast = 0;
uint8_t buffilastsent = 0xff;
uint8_t createdMsg[3][4] = {{0x00,0x00,0x80,0x00}, {0x00,0x00,0x80,0x00}};
volatile uint8_t lastCreatedMsg = 0xff;
volatile uint8_t lastCreatedMsgSent = 0xff;
volatile uint8_t  sendArrayFlag = 0;
volatile uint8_t runCounter = 0;
volatile int16_t rotaryCounter = -1;
uint16_t mainCounter = 0;

void putByteInNextBuff(uint8_t *msgnum, uint8_t *temp);
void sendArray(uint8_t*);
void sendLKASOffArray();
void sendLKASOnArray();
void createSerialMsg(uint8_t*, uint8_t*);
uint8_t chksm(uint8_t*);
uint8_t chksm(uint16_t*);
void timeToSendSerial();
void handleRotary();

ISR(PCINT2_vect) {
	Serial.println("in interrupt");
	unsigned char result = r.process();
	if (result == DIR_NONE) {
		// do nothing
	}
	else if (result == DIR_CW) {
		Serial.println("ClockWise");
		rotaryCounter++;
	}
	else if (result == DIR_CCW) {
		Serial.println("CounterClockWise");
		rotaryCounter--;
	}
}

// b01A0####  setup() is at the bottom
// b10A#####
void loop() {
	handleRotary();


	if(errorcount >2) {
		lkas_active = false;
		errorcount = 0;
	}
	if(sendArrayFlag){
		if(lkas_active){ //if lkas_active need to send the live data, if not, send
			sendLKASOnArray();
		}else {
			sendLKASOffArray();
		}
		sendArrayFlag = 0;
	}
	while(Serial.available()){
		uint8_t temp = Serial.read();
		Serial.print(temp);
		return;

		if(temp == 0x00) break;
		uint8_t active =  (temp >> 5) & 0x01; //if **1***** ie active
		if(!active) {
			lkas_active = false;
			break;
		}
		uint8_t msgnum = (temp >> 6); //is byte 1 or 2
		putByteInNextBuff(&msgnum,&temp);
	}
}

void putByteInNextBuff(uint8_t *msgnum, uint8_t *temp){
	uint8_t buffievenodd = buffi % 2;
	switch(*msgnum){
		case 0:
			if(buffievenodd == 1){  //something went wrong with last send, just reset everthing and increase error count
				errorcount++;
				buff[buffi-1] = *temp;
			} else {
				if(buffi > 4) buffi = 0; //this should nto happen, just in case tho
				buff[buffi] = *temp;
				buffi++;
			}
			break;
		case 1:
			if(buffievenodd == 0){  //something went wrong with last send, just reset everthing and increase error count
				errorcount++; //expecting the 1st byte not 2nd, add error and return
			} else {
				buff[buffi] = *temp;
				if(buffi <5) buffi++; else buffi = 0;//assign buffi the next even, unless its 5, then go to 0
			}
			break;
	}
}

void timeToSendSerial(){
		uint8_t buffievenodd = buffi % 2;
		uint8_t bufftosend;
		if(buffievenodd){ //is odd
			if(bufftosend == 1) bufftosend =4;
			else bufftosend = buffi -1;
		} else {  //is even
			bufftosend = buffi;
		}
		uint8_t byte0 = ((buff[buffi] & 0xF) | (counterbit << 0x5));
		uint8_t byte1 = (buff[buffi+1] & 0x1F) | 0xA0;
		uint8_t byte2 = 0x80;
		Serialwrite(byte0);
		uint16_t total = byte0 + byte1 + byte2;
		Serialwrite(byte1);
		uint8_t byte3 = chksm((uint16_t*)&total);
		Serialwrite(byte2);

		Serialwrite(byte3);
		if(counterbit) counterbit = 0x00; else counterbit = 0x01;
}


void createSerialMsg(uint8_t *localbuffi, uint8_t *msgi){ //array index of buff (even)
	createdMsg[*msgi][0] = (buff[*localbuffi] & 0xF) | (counterbit << 0x5);
  createdMsg[*msgi][1] = (buff[*localbuffi+1] & 0x1F) | 0xA0; //cratedMsgmsg [2] is hard set to 0x80
	//createdMsg[*msgi][3] =
	createdMsg[*msgi][3] = chksm((uint8_t*)&msgi);
	lastCreatedMsg = *msgi;

}

uint8_t chksm(uint8_t *msgi){
	uint16_t local = createdMsg[*msgi][0] + createdMsg[*msgi][1] + createdMsg[*msgi][2] ;
	local = local % 512;
	local = 512 - local;
	return (uint8_t)(local % 256);
}

uint8_t chksm(uint16_t *input){
	uint16_t local = *input % 512;
	local = 512 - local;
	return (uint8_t)(local % 256);
}

void sendLKASOffArray(){
	Serialwrite(lkas_off_array[counterbit][0]);
	Serialwrite(lkas_off_array[counterbit][1]);
	Serialwrite(lkas_off_array[counterbit][2]);
	Serialwrite(lkas_off_array[counterbit][3]);
	if(counterbit) counterbit = 0x00; else counterbit = 0x01;
	digitalWrite(6, counterbit);
	// if(runCounter > 88) {
	// 	digitalWrite(13, counterbit);
	// 	Serial.print("runCounter is ");
	// 	Serial.println(counterbit);
	// 	runCounter = 0;
	// }
	// runCounter++;
}

void sendLKASOnArray(){
	if(lastCreatedMsg == lastCreatedMsgSent){
		sendLKASOffArray();
		lkas_active = false;
		return;
	}
	Serialwrite(createdMsg[lastCreatedMsg][0]);
	Serialwrite(createdMsg[lastCreatedMsg][1]);
	Serialwrite(createdMsg[lastCreatedMsg][2]);
	Serialwrite(createdMsg[lastCreatedMsg][3]);
	lastCreatedMsgSent = lastCreatedMsg;
	if(counterbit) counterbit = 0x00; else counterbit = 0x01;
}

ISR(TIMER2_COMPA_vect) {sendArrayFlag = 1;}//timer2 interrupt every 11.4 ms
ISR(TIMER1_COMPA_vect){sendArrayFlag = 1;}//timer2 interrupt every 11.4 ms
ISR(TIMER0_COMPA_vect){sendArrayFlag = 1;}//timer2 interrupt every 11.4 ms

void setupTimersOld(){
	  TCCR2A = 0;// set entire TCCR2A register to 0
	  TCCR2B = 0;// same for TCCR2B
	  TCNT2  = 0;//initialize counter value to 0
	  // set compare match register for 8khz increments
	  OCR2A = 229;// = (16*10^6) / (87.4hz*1024) - 1 (must be <256)  177== 11.4ms apart 87.2 hz
	  // turn on CTC mode
	  TCCR2A |= 0b10; //(1 << WGM21);
	  // Set CS21 bit for 1024 prescaler
	  TCCR2B |= 0b111; //(1 << CS12) | (1 << CS10;
	  // enable timer compare interrupt
	  TIMSK2 |= (1 << OCIE2A);
}

void setupTimer1WebSite(){
	TCCR2A = 0; // set entire TCCR2A register to 0
	TCCR2B = 0; // same for TCCR2B
	TCNT2  = 0; // initialize counter value to 0
	// set compare match register for undefined Hz increments
	OCR2A = 0x80 ;
	// turn on CTC mode
	TCCR2B |= (1 << WGM21);
	// Set CS22, CS21 and CS20 bits for 1 prescaler
	TCCR2B |= (0 << CS22) | (0 << CS21) | (1 << CS20);
	// enable timer compare interrupt
	TIMSK2 |= (1 << OCIE2A);

}

void setupTimer1(){
	TIMSK1 |= (1 << OCIE1A);

	  TCCR1A = 0;// set entire TCCR2A register to 0
	  TCCR1B = 0;// same for TCCR2B
	  TCNT1  = 0;//initialize counter value to 0
	  // set compare match register for 8khz increments
	  OCR1A = 48000 ;// = (16*10^6) / (87.4hz*1024) - 1 (must be <256) // mod from 3690 to half, then 66%
	  // turn on CTC mode
	  TCCR1A |= 0b10; //(1 << WGM21);
	  // Set CS21 bit for 1024 prescaler
	  TCCR1B |= (1 << CS11) | (1 << CS10);

	  // enable timer compare interrupt
}

void setupTimer2(){

	  TCCR2A = 0;// set entire TCCR2A register to 0
	  TCCR2B = 0;
	  TCNT2  = 0;//initialize counter value to 0
	  // set compare match register for 8khz increments
	  OCR2A = 236;// = (16*10^6) / (87.4hz*1024) - 1 (must be <256)
		OCR2B = 236;
		// turn on CTC mode
	  TCCR2A |= 0x2;
	  // Set CS21 bit for 1024 prescaler
	  TCCR2B |= 0b111; //(1 << CS11) | (1 << CS10);

	  // enable timer compare interrupt
	  TIMSK2 |= 0x80;
}

void handleRotary(){
	// if (digitalRead(4 == HIGH)) rotaryCounter = 0;
	if(rotaryCounter > 0){
		// analogWrite(13, rotaryCounter / 32);
		digitalWrite(13,HIGH);
	}else if(rotaryCounter < 0) {
		if((millis() % 2000) >1000){
			// analogWrite(13, rotaryCounter/ 32);
			digitalWrite(13,HIGH);
		}else{
			digitalWrite(13, LOW);
		}
	}

}

#define encoder0PinA  3
#define encoder0PinB  2
uint8_t n = 0;
uint8_t encoder0PinALast = 0;

void softEncoder(){
	n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(encoder0PinB) == LOW) {
      rotaryCounter--;
    } else {
      rotaryCounter++;
    }
    Serial.print (rotaryCounter);
    Serial.print ("/");
  }
  encoder0PinALast = n;
}

void setup() {
	pinMode(6, OUTPUT);
	Serial.begin(9600,SERIAL_8E1);
	customSerial = new CustomSoftwareSerial(9, 10, true); // rx, tx, inverse logic (default false)
	customSerial->begin(9600, CSERIAL_8E1);         // Baud rate: 9600, configuration: CSERIAL_8N1

	cli();
	// setupTimer1();
	setupTimersOld();
	// r.begin();
	// PCICR |= (1 << PCIE2);
	// PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
	sei();//allow interruptskh,ya
	pinMode(13,OUTPUT);
	pinMode(4,INPUT);
}