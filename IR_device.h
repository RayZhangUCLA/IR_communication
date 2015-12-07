#ifndef IR_device_H
#define IR_device_H

#include <mraa/gpio.h>  // for gpio
#include <string>

using namespace std;

#define INTERVAL 8/* finest time interval in milliseconds*/
#define Packet_size 40 //Header:10 Addr:8 Data:16 CRC:5 End:1
#define POLYNOMIAL (0x2D << 18)  /* 6-bits polynomials 101101 followed by 0's */

class IR_device
{
public:
    IR_device(int pin, string type);
    ~IR_device();
    bool send(string msg); // send msg through infrared, see protocols for more details
    string recv(); //return empty string if nothing received
private:
	string type;
};


#endif