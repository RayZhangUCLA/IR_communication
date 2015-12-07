#include "IR_device.h"
#include <mraa/gpio.h>  // for gpio
#include <mraa/pwm.h>  // for gpio
#include <iostream>     // for cout
#include <sys/time.h>   // for setitimer
#include <signal.h>     // for signal 
#include <cstdlib>      // for exit
#include <bitset>       // for bitset
#include <string>
#include <queue>        // for queue
#include <stdint.h>     // for uint_t
#include <set>          // for set

using namespace std;

//global variables
mraa_gpio_context gpio;
mraa_pwm_context pwm;
//for sending
volatile int* send_array;
volatile int send_counter;
volatile int send_array_size;
volatile int looping;
//for recving
volatile bool msg_recvd = false;
queue<uint8_t> recv_queue;
volatile bool handler_registered = false;


uint8_t crc_cksum(uint32_t const message)
{
    /*
     * Initially, the dividend is the remainder.
     */
    uint32_t remainder = message;
    /*
     * For each bit position in the message....
     */
    for (uint8_t bit = 24; bit > 0; --bit)
    {
        if (remainder & 0x800000){
            remainder ^= POLYNOMIAL;
        }
        remainder = (remainder << 1) & 0xFFFFFF;
    }
    return (remainder >> 19);
}  


void sig_handler_send(int signum){
    if(send_array != NULL){
        if(send_array[send_counter]){
            mraa_pwm_write(pwm, 0.5); 
        }else{
            mraa_pwm_write(pwm, 0);     
        }
        send_counter++;
        send_counter = send_counter % send_array_size;
    }
}

void sig_handler_void(int signum){
}

void sig_handler_recv(int signum){
    if(!msg_recvd)
    {
        recv_queue.push((uint8_t) !mraa_gpio_read(gpio));
    }
}


void intrrupt_handler(int signum)
{
    signal(SIGALRM, SIG_DFL);  
    looping = 0;
}


string byte_to_bits(char byte, int (&bits)[8],int (&bits_fliped)[8]){
    bitset<8> bit_set(byte);
    for(int i=8;i>0;i--){
        bits[8-i] = bit_set[i-1];
        bits_fliped[8-i] = !bit_set[i-1];
    }
    return bit_set.to_string();
}

void construct_packet(int index, char byte1, char byte2, int (&packet_bits)[Packet_size]){
    int counter=0;
    int bits[8];
    int bits_fliped[8];
    string index_data1_data2="";

    //add header: 10 '1's
    for(int j=0; j<10; j++){
        packet_bits[counter] = 1;
        counter++;            
    }

    //add addr in bits
    index_data1_data2 += byte_to_bits((char) index, bits, bits_fliped);
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits[j];
        counter++;            
    }
    //add data in bits 
    index_data1_data2 += byte_to_bits(byte1, bits, bits_fliped);
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits[j];
        counter++;            
    }
    index_data1_data2 += byte_to_bits(byte2, bits, bits_fliped);
    for(int j=0; j<8; j++){
        packet_bits[counter] = bits[j];
        counter++;            
    }

    //add crc to packet
    cout << "index_data1_data2=" << index_data1_data2 << endl;
    bitset<24> crc_set(index_data1_data2);
    // cout << "crc = " << crc_set << endl;
    uint8_t crc = crc_cksum((uint32_t) crc_set.to_ulong());
    cout << "crc_cksum = " << (int) crc << endl;
    for(int i=0; i<5;i++){
        packet_bits[counter] = (crc & (0x10 >> i)) > 0;    
        counter++;
    }

    //add end
    packet_bits[counter] = 1;
}

//for debugging
void print_send_array(){
    for(int i=0; i<send_array_size;i++){
        cout << send_array[i];
    }
    cout << endl;
}


IR_device::IR_device(int pin, string type){
    this->type = type;
    if(type == "send"){
        pwm = mraa_pwm_init(3);
        if (pwm == NULL) {
            cout << "pwm initialization error\n";
        }
        mraa_pwm_period_us(pwm, 26);
        mraa_pwm_enable(pwm, 1);
        send_array = NULL;
        send_counter = 0;
        send_array_size = 0;
        looping = 1;
        signal(SIGINT, intrrupt_handler);
    }else if(type == "recv"){
        gpio = mraa_gpio_init(pin);
        mraa_gpio_dir(gpio, MRAA_GPIO_IN);
    }else{
        cout << "Cannot create IR_device object. Type unknown. Please specify 'send' or 'recv'\n";
    }
}

IR_device::~IR_device(){
    if(this->type == "send")
        mraa_pwm_close(pwm);
    else
        mraa_gpio_close(gpio);
}


bool IR_device::send(string msg){
    if(msg.size()>512){
        cout << "Sorry, cannot send msg with length greater than 256\n";
        return false;
    }
    if(msg.size()%2 == 1)
        msg += " ";
    send_array_size = (msg.size()/2)*(Packet_size + 10);
    send_array = new int [send_array_size];
    int counter=0;
    if(!send_array)
    {
        cout << "cannot allocate new space\n";
        return false;
    }
    
    //construct IR bit streams
    int packet_bits[Packet_size];
    for(int i=0; i<msg.size();i+=2){ //parse message
        //construct packets
        construct_packet(i/2, msg[i],msg[i+1], packet_bits);
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
    cout << "bits stream constructed...\n";

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

    // local variables
    string addr_bits="";
    string byte1_bits="";
    string byte2_bits="";
    string crc_bits="";
    char recv_array[512];
    bool byte1_get = false;
    bool byte2_get = false;
    bool header_received = false;
    bool header_zero_received = false;
    bool header_one_received = false;
    bool addr_received = false;
    int index=-1;
    int last_index = -1;
    bool index_get = false;
    int bit_counter=0;
    int header_counter_zero=0;
    int header_counter_one=0;
    int last_bit = 1;
    int msg_leng=512;
    bool len_get = false;
    msg_recvd = false;
    set<int> recv_byte_set; 

    cout << "registering timer\n";
    struct itimerval it_val;  /* for setting itimer */
    if(!handler_registered){
        if (signal(SIGALRM, (void (*)(int)) sig_handler_recv) == SIG_ERR) {
            perror("Unable to catch SIGALRM");
            return "";
        }
        handler_registered = true;
    }
    it_val.it_value.tv_sec = INTERVAL/1000;
    it_val.it_value.tv_usec = (INTERVAL*1000) % 1000000;   
    it_val.it_interval = it_val.it_value;

    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        return "";
    }

    cout << "registering timer finished...\n";
    while(!msg_recvd){
        while(!recv_queue.empty()){
            uint8_t reading = recv_queue.front();
            recv_queue.pop();
            // cout << (int) reading << endl;
            if(header_received){ 
                if(addr_received){
                    char temp = reading + '0';
                    if(!byte1_get)
                        byte1_bits += temp;
                    else if(!byte2_get)
                        byte2_bits += temp;
                    else
                        crc_bits += temp;
                    bit_counter++;
                    // cout << "bit_counter = " << bit_counter << endl;
                    if(!byte1_get){
                       if(bit_counter == 8){
                            bit_counter = 0;
                            byte1_get = true;
                            cout << "Byte1 recved!\n";
                       }
                    }else{
                        if(!byte2_get){
                            if(bit_counter == 8){
                                bit_counter = 0;
                                byte2_get = true;
                                cout << "Byte2 recved!\n";    
                            }
                        }else{
                            if(bit_counter == 5){
                                bitset<24> a_byte1_2(addr_bits+byte1_bits+byte2_bits);
                                uint8_t crc_cacl = crc_cksum((uint32_t) a_byte1_2.to_ulong());
                                bitset<5> crc(crc_bits);                        
                                uint8_t crc_recv = crc.to_ulong();
                                cout << "crc = " << (int) crc_recv;  
                                if(crc_recv == crc_cacl){
                                    bitset<8> addr(addr_bits);
                                    bitset<8> byte1(byte1_bits);
                                    bitset<8> byte2(byte2_bits);
                                    int index = addr.to_ulong();
                                    cout << "index = "<< index << endl;
                                    char data1 = (char) byte1.to_ulong();
                                    cout << "data1=" << data1 << endl;
                                    recv_array[index*2] = data1;
                                    data1 = (char) byte2.to_ulong();
                                    cout << "data2=" << data1 << endl;
                                    recv_array[index*2+1] = data1;
                                    recv_byte_set.insert(index);
                                    if(!len_get){ //should be put 
                                        // cout << "index = "<< index << endl;
                                        if(index <= last_index){
                                            msg_leng = last_index + 1;
                                            len_get = true;
                                            cout << "len=" << msg_leng*2 <<endl;
                                        }else{
                                            last_index = index;    
                                        }    
                                    }
                                    cout << "crc matched\n";
                                }
                                if(recv_byte_set.size() >= msg_leng){
                                    cout << "cancelling timer\n";
                                    it_val.it_value.tv_sec = 0;
                                    it_val.it_value.tv_usec = 0;   
                                    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                                            perror("error cancelling setitimer()");
                                            return "";
                                        }
                                    msg_recvd = true;    
                                }                       
                                bit_counter = 0;
                                header_received = false;
                                addr_received = false;
                                byte1_get = false;
                                byte2_get = false;
                                addr_bits = "";
                                byte1_bits = "";
                                byte2_bits = "";
                                crc_bits = "";
                            }
                        }
                    }
                }else{  //read addr first
                    char temp = reading + '0';
                    addr_bits += temp;
                    bit_counter++;
                    if(bit_counter == 8){
                        bit_counter = 0;
                        cout << "addr received\n";
                        addr_received = true;
                        // cout << "index = " << index << endl;
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
                        cout << "Header detected\n";
                        header_received = true;
                    }
                }
                last_bit = reading; 
            }
        }
    }
    
    string str="";
    for(int i=0;i<msg_leng*2;i++)
        str += recv_array[i];
    return str;
}

