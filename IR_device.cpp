#include "IR_device.h"
#include <mraa/gpio.h>  // for gpio
#include <iostream>     // for cout
#include <sys/time.h>   // for setitimer
#include <signal.h>     // for signal 
#include <cstdlib>      // for exit
#include <bitset>       // for bitset
#include <string>

using namespace std;

//global variables
mraa_gpio_context gpio;
//for sending
volatile int* send_array;
volatile int send_counter;
volatile int send_array_size;
volatile int looping;
//for recving
string bits="";
char recv_array[257];
volatile char recv_byte;
volatile bool char_get;
volatile bool header_received;
volatile bool header_zero_received;
volatile bool header_one_received;
volatile bool addr_received;
volatile int index;
volatile int last_index;
volatile bool index_get;
volatile int bit_counter;
volatile int header_counter_zero;
volatile int header_counter_one;
volatile bool msg_recvd;
volatile int last_bit;
volatile int bytes_recved;
volatile int msg_leng;
volatile bool len_get;



void sig_handler_send(int signum){
    if(send_array != NULL){
        mraa_gpio_write(gpio, send_array[send_counter]); 
        send_counter++;
        send_counter = send_counter % send_array_size;
    }
}

void sig_handler_recv(int signum){
    if(!msg_recvd){
        int reading = !mraa_gpio_read(gpio);
        // cout << reading << endl;
        if(header_received){ 
            if(addr_received){
                char temp = reading + '0';
                bits += temp;
                bit_counter++;
                if(bit_counter == 8 && !char_get){
                    bitset<8> char_bitset(bits);
                    recv_byte = (char) char_bitset.to_ulong();
                    bits = "";
                    bit_counter = 0;
                    char_get = true;
                }else if(bit_counter == 8){
                    bitset<8> char_fliped_bitset(bits);
                    char temp = (char) char_fliped_bitset.flip().to_ulong();
                    bits = "";
                    bit_counter = 0;
                    if(temp == recv_byte){
                        recv_array[index] = recv_byte;
                        bytes_recved++;
                        // cout << "byte received\n";
                    }
                    header_received = false;
                    addr_received = false;
                    index_get = false;
                    char_get = false;
                    // cout << "byte recved = " << temp << endl;
                    if(bytes_recved >= msg_leng){
                        msg_recvd = true;    
                    }
                }

            }else{  //read addr first
                char temp = reading + '0';
                bits += temp;
                bit_counter++;
                if(bit_counter == 8 && !index_get){
                    bitset<8> index_bitset(bits);
                    index = index_bitset.to_ulong();
                    bits = "";
                    bit_counter = 0;
                    index_get = true;
                    // cout << "index = " << index << endl;
                }else if(bit_counter == 8){
                    bitset<8> index_fliped_bitset(bits);
                    int temp = index_fliped_bitset.flip().to_ulong();
                    bits = "";
                    bit_counter = 0;
                    if(temp == index)
                    {
                        // cout << "addr received\n";
                        addr_received = true;
                        if(!len_get){
                            if(index <= last_index){
                                msg_leng = last_index + 1;
                                // recv_array[msg_leng] = '\0';
                                len_get = true;
                                // cout << "len=" << msg_leng <<endl;
                            }else{
                                last_index = index;    
                            }    
                        }
                    }
                    else{
                        header_received = false;
                    }
                }
            }
        }else{
            //check 10 consecutive '0's and '1's
            if(reading == 0){
                if(last_bit == 0)
                    header_counter_zero++;
                else
                    header_counter_zero = 1;
                if(header_counter_zero == 10)
                    header_zero_received = true;
            }else{ // reading is '1'
                if(last_bit == 0 && header_zero_received){
                    header_counter_one = 1;
                }else if(last_bit == 1 && header_zero_received){
                    header_counter_one++;
                }else{
                    header_zero_received = false;
                }
                if(header_counter_one == 10){
                    // cout << "Header detected\n";
                    header_received = true;
                }
            }
            last_bit = reading; 
        }
    }
    
}

void intrrupt_handler(int signum)
{
    signal(SIGALRM, SIG_DFL);  
    looping = 0;
}


void byte_to_bits(char byte, int (&bits)[8],int (&bits_fliped)[8]){
    bitset<8> bit_set(byte);
    for(int i=8;i>0;i--){
        bits[8-i] = bit_set[i-1];
        bits_fliped[8-i] = !bit_set[i-1];
    }
}

void construct_packet(int index, char byte, int (&packet_bits)[Packet_size]){
    int counter=0;
    int bits[8];
    int bits_fliped[8];
    //add header: 10 '1's
    for(int j=0; j<10; j++){
        packet_bits[counter] = 1;
        counter++;            
    }

    //add addr in bits and fliped bits
    byte_to_bits((char) index, bits, bits_fliped);
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits[j];
        counter++;            
    }
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits_fliped[j];
        counter++;            
    }
    //add data in bits and fliped bits
    byte_to_bits(byte, bits, bits_fliped);
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits[j];
        counter++;            
    }
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits_fliped[j];
        counter++;            
    }
    //add end to packet
    packet_bits[counter] = 1;
    counter++;
    packet_bits[counter] = 1;
    counter++;
}

//for debugging
void print_send_array(){
    for(int i=0; i<send_array_size;i++){
        cout << send_array[i];
    }
    cout << endl;
}


IR_device::IR_device(int pin, string type){
    if(type == "send"){
        gpio = mraa_gpio_init(pin);
        mraa_gpio_dir(gpio, MRAA_GPIO_OUT);
        send_array = NULL;
        send_counter = 0;
        send_array_size = 0;
        looping = 1;
        signal(SIGINT, intrrupt_handler);
    }else if(type == "recv"){
        gpio = mraa_gpio_init(pin);
        mraa_gpio_dir(gpio, MRAA_GPIO_IN);
        bits="";
        char_get = false;
        header_received = false;
        header_zero_received = false;
        header_one_received = false;
        addr_received = false;
        index=-1;
        last_index = -1;
        index_get = false;
        bit_counter=0;
        header_counter_zero=0;
        header_counter_one=0;
        msg_recvd = false;
        last_bit = 1;
        bytes_recved=0;
        msg_leng=257;
        len_get = false;
    }else{
        cout << "Cannot create IR_device object. Type unknown. Please specify 'send' or 'recv'\n";
    }
}

IR_device::~IR_device(){
    mraa_gpio_close(gpio);
}


bool IR_device::send(string msg){
    if(msg.size()>256){
        cout << "Sorry, cannot send msg with length greater than 256\n";
        return false;
    }
    send_array_size = msg.size()*(Packet_size + 10);
    send_array = new int [send_array_size];
    int counter=0;
    if(!send_array)
    {
        cout << "cannot allocate new space\n";
        return false;
    }
    
    //construct IR bit streams
    int packet_bits[Packet_size];
    for(int i=0; i<msg.size();i++){ //parse message
        //construct packets
        construct_packet(i, msg[i], packet_bits);
        for(int j=0;j<Packet_size;j++){
            send_array[counter] = packet_bits[j];
            counter++;
        }

        //add slient intervals
        for(int j=0; j<10;j++){
            send_array[counter]=0;
            counter++;
        }
    }
    // cout << "bits stream constructed...\n";

    // print_send_array();
    
    //sending packets
    struct itimerval it_val;  /* for setting itimer */
    if (signal(SIGALRM, (void (*)(int)) sig_handler_send) == SIG_ERR) {
        perror("Unable to catch SIGALRM");
        return false;
    }
    it_val.it_value.tv_sec = INTERVAL/1000;
    it_val.it_value.tv_usec = (INTERVAL*1000) % 1000000;   
    it_val.it_interval = it_val.it_value;

    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        return false;
    }

    while(looping){};


    // cout << "send succssfully...\n";
    delete [] send_array;
    return true;
}

string IR_device::recv(){
    // cout << "registering timer\n";
    struct itimerval it_val;  /* for setting itimer */
    if (signal(SIGALRM, (void (*)(int)) sig_handler_recv) == SIG_ERR) {
        perror("Unable to catch SIGALRM");
        return "";
    }
    it_val.it_value.tv_sec = INTERVAL/1000;
    it_val.it_value.tv_usec = (INTERVAL*1000) % 1000000;   
    it_val.it_interval = it_val.it_value;

    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        return "";
    }

    while(!msg_recvd){};
    signal(SIGALRM, SIG_DFL);  
    string str="";
    for(int i=0;i<msg_leng;i++)
        str += recv_array[i];
    return str;
}

