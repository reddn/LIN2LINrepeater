#include <Arduino.h>
#include <CustomSoftwareSerial.h>
#include <Rotary.h>

// #define Serialwrite Serial.write
#define Serialwrite customSerial->write  //on change fix setup
#define DEBUG 1

CustomSoftwareSerial* customSerial;
Rotary r = Rotary(2, 3);

uint8_t lkas_off_array[][4] =  {{0x20,0x80,0xc0,0xa0},{0x00,0x80,0xc0,0xc0}};
uint8_t counterbit = 0;
uint8_t buff[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t buffi = 0;
bool lkas_active = false;
uint8_t errorcount = 0;
uint8_t sendlast = 0;
uint8_t buffilastsent = 0xff;
uint8_t createdMsg[4][5]; //3 buffers of 5 bytes, the 4 bytes is the frame, the 5th byte is the index index off buff where the frame was made from (note the buff is 2 bytes)
uint8_t lastCreatedMsg = 0xff;
uint8_t lastCreatedMsgSent = 0xff;
volatile uint8_t  sendArrayFlag = 0;
int16_t rotaryCounter = 450;  // min is 0 max is 906 -- default sent to center
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
void setupTimersOld();
void checkAndRunSendArrayFlag();
void checkForRightCounterInPreCreatedMsg();


void setup() {
	Serial.begin(9600,SERIAL_8E1);
	customSerial = new CustomSoftwareSerial(9, 10, true); // rx, tx, inverse logic (default false)
	customSerial->begin(9600, CSERIAL_8E1);         // Baud rate: 9600, configuration: CSERIAL_8N1

	cli();  // no interrupts
	// setupTimer1();
	setupTimersOld();
	sei();//allow interrupts

	pinMode(13,OUTPUT);
}

// OP to LIN2LIN data structure (it still sends 4 bytes, but the last 2 are 0x00)
// b01A0####    ####is big_steer   A = Active   first 2 bits is the byte counter
// b10A#####    ##### is little steer
void loop() {
	checkForRightCounterInPreCreatedMsg();
	checkAndRunSendArrayFlag();
	handleRotary();

	checkAndRunSendArrayFlag();

	while(Serial.available()){
		uint8_t temp = Serial.read();
		Serial.print(temp);

		if(temp == 0x00) break;
		uint8_t active =  (temp >> 5) & 0x01; //if bb1bbbbb ... 1 is active bit
		if(!active) {
			lkas_active = false;
			break;
		}
		uint8_t msgnum = (temp >> 6); //its 1 or 2
		putByteInNextBuff(&msgnum,&temp);
	} // end of while

}  // end of loop

void checkAndRunSendArrayFlag(){
	if(errorcount > 1) {
		lkas_active = false;
		errorcount = 0;
		sendArrayFlag = 0;
	}
	if(sendArrayFlag){
		if(lkas_active){ //if lkas_active need to send the live data, if not, send
			sendLKASOnArray();
		}else {
			sendLKASOffArray();
		}
		sendArrayFlag = 0;
	}
}

void putByteInNextBuff(uint8_t *msgnum, uint8_t *temp){
	uint8_t buffievenodd = buffi % 2; //buffi is the next byte we are expecting, count starts at 0 .  ie  byte 1 counter is 1, goes in an even buffi (starting at 0)
	switch(*msgnum){
		case 1:  //byte counter from serial message starts at 1
			if(buffievenodd == 1){  //something went wrong with last send, just reset everthing and increase error count
				errorcount++;
				buff[buffi-1] = *temp;
			} else {
				if(buffi > 4) buffi = 0; //this should nto happen, just in case tho
				buff[buffi] = *temp;
				buffi++;
			}
			break;
		case 2: //byte counter starts at 1. so counter == 2 is the 2nd byte
			if(buffievenodd == 0){  //something went wrong with last send, just reset everthing and increase error count.
				errorcount++; //expecting the 1st byte not 2nd, add error and return
			} else {
				buff[buffi] = *temp;
				uint8_t buffisub = buffi - 1;
				lastCreatedMsg = lastCreatedMsg < 3 ? lastCreatedMsg + 1 : 0;
				createSerialMsg(&buffisub, &lastCreatedMsg);
				if(buffi <5) buffi++; else buffi = 0;//assign buffi the next even, unless its 5, then go to 0
			}

			break;
	}
}

// void timeToSendSerial(){  //not used 1/19/19
// 		uint8_t buffievenodd = buffi % 2;
// 		uint8_t bufftosend;
// 		if(buffievenodd){ //is odd
// 			if(bufftosend == 1) bufftosend =4;
// 			else bufftosend = buffi -1;
// 		} else {  //is even
// 			bufftosend = buffi;
// 		}
// 		uint8_t byte0 = ((buff[buffi] & 0xF) | (counterbit << 0x5));
// 		uint8_t byte1 = (buff[buffi+1] & 0x1F) | 0xA0;
// 		uint8_t byte2 = 0x80;
// 		Serialwrite(byte0);
// 		uint16_t total = byte0 + byte1 + byte2;
// 		Serialwrite(byte1);
// 		uint8_t byte3 = chksm((uint16_t*)&total);
// 		Serialwrite(byte2);
// 		Serialwrite(byte3);
// 		if(counterbit) counterbit = 0x00; else counterbit = 0x01;
// }

void createSerialMsg(uint8_t *localbuffi, uint8_t *msgi){ //array index of buff (even)
	createdMsg[*msgi][0] = (buff[*localbuffi] & 0xF) | (counterbit << 0x5);
  createdMsg[*msgi][1] = (buff[*localbuffi+1] & 0x1F) | 0xA0; //cratedMsgmsg [2] is hard set to 0x80
	createdMsg[*msgi][2] = 0x80;
	createdMsg[*msgi][3] = chksm((uint8_t*)&msgi);
	createdMsg[*msgi][4] = *localbuffi;
	lastCreatedMsg = *msgi;
}

void sendLKASOffArray(){
	Serialwrite(lkas_off_array[counterbit][0]);
	Serialwrite(lkas_off_array[counterbit][1]);
	Serialwrite(lkas_off_array[counterbit][2]);
	Serialwrite(lkas_off_array[counterbit][3]);
	counterbit = counterbit > 0 ? 0x00 : 0x01;
	// if(counterbit) counterbit = 0x00; else counterbit = 0x01;
}

void sendLKASOnArray(){
	if(lastCreatedMsg == lastCreatedMsgSent){  //TODO: allow 1 resend of last data, but needs to be recreated w new counter /checksum
		errorcount++;
		createSerialMsg(&createdMsg[lastCreatedMsgSent][4], &lastCreatedMsg);
	} else errorcount = 0;
	Serialwrite(createdMsg[lastCreatedMsg][0]);
	Serialwrite(createdMsg[lastCreatedMsg][1]);
	Serialwrite(createdMsg[lastCreatedMsg][2]);
	Serialwrite(createdMsg[lastCreatedMsg][3]);
	lastCreatedMsgSent = lastCreatedMsg;
	counterbit = counterbit > 0 ? 0x00 : 0x01;
}


void handleRotary(){  // min is 0 max is 906
	rotaryCounter = analogRead(A5);

}

void checkForRightCounterInPreCreatedMsg(){
	if(lastCreatedMsg != lastCreatedMsgSent){
		if((createdMsg[lastCreatedMsg][0] >> 5) != counterbit){
			createSerialMsg(&createdMsg[lastCreatedMsg][4], &lastCreatedMsg);
		}
	}

}


/*** CHECKSUMS ***/
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


/*** TIMERS AND INTERRUPT FUNCTS ***/

ISR(TIMER2_COMPA_vect) {sendArrayFlag = 1;}//
ISR(TIMER1_COMPA_vect){sendArrayFlag = 1;}//
ISR(TIMER0_COMPA_vect){sendArrayFlag = 1;}//

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
