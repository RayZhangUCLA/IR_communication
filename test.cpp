#include "IR_device.h"
#include <string>
#include <iostream>

using namespace std;


int main(int argc, char const *argv[])
{
    cout << "Operating at " << INTERVAL << "ms intervals...\n";
    IR_device tmp = IR_device(3, "recv");
    string msg = tmp.recv();
    cout << msg << endl;
    // msg = tmp.recv();
    // cout << msg << endl;
    cout << "program finished\n"; 
    return 0;
}
