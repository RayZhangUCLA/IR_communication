#include "IR_device.h"
#include <string>
#include <iostream>

using namespace std;


int main(int argc, char const *argv[])
{
    cout << "Operating at " << INTERVAL << "ms intervals...\n";
    IR_device tmp = IR_device(3, "send");
    string msg = "The Soviet Union";// had its roots in 1917 when the Bolsheviks, headed by Vladimir Lenin";//, led the October Revolution which overthrew the provisional government that had replaced the Tsar. ";
    cout << msg << endl;
    cout << msg.size() << endl;
    tmp.send(msg);
    return 0;
}
