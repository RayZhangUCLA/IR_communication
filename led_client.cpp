#include "IR_device.h"
#include "led_client.h"
#include <string>
#include <iostream>
#include <algorithm>
#include <Python.h>

using namespace std;

//compile command: g++ -std=c++11 -lmraa -o led_client test.cpp IR_device.cpp -I/usr/include/python2.7/ -lpython2.7
//argument: python_file_name python_function_name LED_index messege eg:./led_client BT_localization localization 1 ni_hao

bool get_location_from_rssi(int argc, char const* argv[], string& location_info){
	argc -= 2;
	setenv("PYTHONPATH",".",1);
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;
    int i;

    Py_Initialize();
    pName = PyString_FromString(argv[1]);
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, argv[2]);
        /* pFunc is a new reference */

        if (pFunc && PyCallable_Check(pFunc)) {
            pArgs = PyTuple_New(argc - 3);
            for (i = 0; i < argc - 3; ++i) {
                pValue = PyInt_FromLong(atoi(argv[i + 3]));
                if (!pValue) {
                    Py_DECREF(pArgs);
                    Py_DECREF(pModule);
                    fprintf(stderr, "Cannot convert argument\n");
                    return false;
                }
                /* pValue reference stolen here: */
                PyTuple_SetItem(pArgs, i, pValue);
            }
            pValue = PyObject_CallObject(pFunc, pArgs);
            const char* s = PyString_AsString(pValue);
            location_info = string(s);
            Py_DECREF(pArgs);
            if (pValue != NULL) {
                cout << "Result of call: " + location_info << endl;
                Py_DECREF(pValue);
            }
            else {
                Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                fprintf(stderr,"Call failed\n");
                return false;
            }
        }
        else {
            if (PyErr_Occurred())
                PyErr_Print();
            fprintf(stderr, "Cannot find function \"%s\"\n", argv[2]);
        }
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else {
        PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", argv[1]);
        return false;
    }
    Py_Finalize();
    return true;
}


void print_seperator(){
	for(int i=0; i<40;i++){
		cout << "==";
	}
	cout << endl;
}

int main(int argc, char const *argv[])
{
    if (argc < 5) {
        fprintf(stderr,"Usage: call pythonfile funcname [args]\n");
        return 1;
    }	
    int led_index = stoi(string(argv[3]));
    string msg = argv[4];
    replace(msg.begin(), msg.end(),'_', ' ');

    cout << "led_index = " << led_index << endl;
	cout << "msg you want to sent is " << msg << endl;
	cout << "LED " << led_index <<" is getting location information from RSSI...\n";
    string location_info;
    if(!get_location_from_rssi(argc, argv, location_info)){
    	cout << "cannot get location_info, abort" << endl;
    	return 1;
    }
    cout  << "location is " << location_info << endl;
    print_seperator();

    cout << "Sending information to server...\n" << endl;
    LEDSys led_client(led_index, location_info, msg);
    if(!led_client.sendInfo())
	{
		cout << "Cannot send information to server, abort\n";
		return 1;	
	}
	cout << "Information sent to server\n";
	print_seperator();

	cout << "Start broadcasting msg in IR...\n";
    cout << "Operating at " << INTERVAL << "ms intervals...\n";
    IR_device tmp = IR_device(3, "send");
    // string msg = "The Soviet Union";// had its roots in 1917 when the Bolsheviks, headed by Vladimir Lenin";//, led the October Revolution which overthrew the provisional government that had replaced the Tsar. ";
    cout << msg << endl;
    cout << msg.size() << endl;
    if(!tmp.send(msg)){
    	cout << "Problem broadcasting IR msg, abort\n";
    	return 1;
    }


    // cout << "Operating at " << INTERVAL << "ms intervals...\n";
    // IR_device tmp = IR_device(3, "recv");
    // string msg = tmp.recv();
    // cout << msg << endl;
    // // msg = tmp.recv();
    // // cout << msg << endl;
    cout << "program finished\n"; 
    return 0;
    
}
