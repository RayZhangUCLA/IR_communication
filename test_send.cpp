#include "IR_device.h"
#include <string>
#include <iostream>

using namespace std;


int main(int argc, char const *argv[])
{
    cout << "Operating at " << INTERVAL << "ms intervals...\n";
    IR_device tmp = IR_device(3, "send");
    string msg = "This is supposed to be a very long message. Anyway, I am just bored. Guess how it may go in the reciving end? I don't know.~";
    cout << msg << endl;
    cout << msg.size() << endl;
    tmp.send(msg);
    return 0;
}
