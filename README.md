# LIN2LINrepeater
takes openpilot panda LIN data, buffers it, repackages it, and sends it on honda accord serial steering K-LIN

**below taking from comments in the code

 all input pins are INPUT_PULLUP (pulled up internallY) and active low (except rotary pot input)
7: Force LKAS ON  - 
12: set Rotary POT current pos as center  -
13: used for LED when LKAS is on  - 
A0: Enable Rotary as INPUT  -
A4: send sendDebug on serial  - 
A5: rotary pot input

OP to LIN2LIN data structure (it still sends 4 bytes, but the last 2 are 0x00).
i will prolly change that later to only 2 bytes, but this code will not need to be changed
byte1: 01A0####    #### = is big_steer   A = Active   first 2 bits is the byte counter
byte2: 10A#####    ##### = is little steer  ***/
