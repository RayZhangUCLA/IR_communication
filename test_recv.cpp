#include "IR_device.h"
#include <string>
#include <iostream>
#include <unistd.h>

using namespace std;


int main(int argc, char const *argv[])
{
	usleep(5000000);
    cout << "Operating at " << INTERVAL << "ms intervals...\n";
    IR_device tmp = IR_device(3, "recv");
    string msg = tmp.recv();
    cout << msg << endl;
    // msg = tmp.recv();
    // cout << msg << endl;
    usleep(5000000);
    msg = tmp.recv();
    cout << msg << endl;
    cout << "program finished\n"; 
    return 0;
}
