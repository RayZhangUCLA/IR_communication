#ifndef IR_device_H
#define IR_device_H

#include <mraa/gpio.h>  // for gpio
#include <string>

using namespace std;

#define INTERVAL 10/* finest time interval in milliseconds*/
#define Packet_size 44 //Header:10 Addr:16 Data:16 End:2

class IR_device
{
public:
    IR_device(int pin, string type);
    ~IR_device();
    bool send(string msg); // send msg through infrared, see protocols for more details
    string recv(); //return empty string if nothing received
private:
};


#endif