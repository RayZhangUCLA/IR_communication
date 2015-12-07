//
//  car_motion.c
//  
//
//  Created by mingrui shi on 11/7/15.
//
//

#include "car_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "IR_device.h"
#include <mraa/pwm.h>
#include <string>
#include <iostream>

#define CENTER 0.0535f
#define Rr 34 // right R
#define Rl 30 // left R
#define V 14 // straight speed
#define Vr 12.10 // turning speed
#define SPEED 50
#define PI 3.1415926535
#define MAXBUFSIZ 1024

using namespace std;

mraa_pwm_context speed_pwm_in1, speed_pwm_in2, turn_pwm;

void speed_control(mraa_pwm_context in1, mraa_pwm_context in2, float speed) {
    speed = speed/100;
    if (speed >= 0) {
        mraa_pwm_write(in2, 1.0f);
        mraa_pwm_write(in1, 1.0f - speed);
    }
    else if (speed < 0) {
        mraa_pwm_write(in1, 1.0f);
        mraa_pwm_write(in2, 1.0f + speed);
    }
}

typedef struct coordinate{
    double x;
    double y;
} Coordinate;

double caculateMotionTime(Coordinate current, Coordinate destination, double offset, IR_device* ir){
    
    cout << "entering calculateMotionTime\n";
    double dx = (destination.x - current.x); // x distance between destination and current
    double dy = (destination.y - current.y); // y distance between destination and current
    int turn = 0; // 0 means turn left 1 means turn right
    double alpha = atan(dy/dx); // angle between current and destinatnion
    if(dx < 0)
        alpha += PI;
    if(alpha > PI)
        alpha -= 2*PI;
    double beta = offset - alpha;
    if(beta > PI)
        beta -= 2*PI;
    else if(beta < -PI)
        beta += 2*PI;
    // determine turn direction
    if(beta > 0)
        turn = 1;
    double R = (turn)?Rr:Rl;
    if (beta < 0)
        beta = -beta;
    double d_2 = dx * dx + dy * dy;
    double l_2 = R * R + d_2 - 2 * R * sqrt(d_2)*sin(beta);
    double straight = sqrt(l_2 - R*R);
    double theta2 = acos((l_2 + R*R - d_2) /2/sqrt(l_2)/R);
    double theta1 = acos(R / sqrt(l_2));
    double turnangle;
    //case 1
    if(beta < PI/2)
    {
        turnangle = theta2 - theta1;
    //case 2
    } else {
        turnangle = 2*PI - theta1 -theta2;
    }
    double round = turnangle*R;
    
    // caculate new offset
    if(turn)
    {
        offset -= turnangle;
    } else {
        offset += turnangle;
    }
    
    if (offset > PI) {
        offset -= 2*PI;
    } else if(offset < -PI)
        offset += 2*PI;
    
    double curveTime = round / Vr, straightTime = straight / V;
    
    //compensate for angle difference
    float turnCtrl = CENTER;
    if (turn)
        turnCtrl += 0.015f;
    else
        turnCtrl -= 0.0145f;
    mraa_pwm_write(turn_pwm, turnCtrl);
    usleep(100000);
    

    printf("%.4f, %.4f", curveTime, straightTime);
    printf(" %.4f\n" , offset);    
    //go for the curve
    speed_control(speed_pwm_in1, speed_pwm_in2, SPEED);
    usleep(curveTime * 1000000);
    
    //go straight
    turnCtrl = CENTER;
    mraa_pwm_write(turn_pwm, turnCtrl);
    usleep(100000);
    speed_control(speed_pwm_in1, speed_pwm_in2, SPEED);
    usleep(straightTime * 1000000);
    speed_control(speed_pwm_in1, speed_pwm_in2, 0.0f);
    cout << "ready to recv ir\n";
    string msg = ir->recv();
    cout << msg << endl;
    return offset;
}


int main(){
    float speed, turn;
    char speed_user_input[MAXBUFSIZ];
    char turn_user_input[MAXBUFSIZ];
    speed_pwm_in1 = mraa_pwm_init(3);
    speed_pwm_in2 = mraa_pwm_init(5);
    turn_pwm = mraa_pwm_init(6);
    
    if (speed_pwm_in1 == NULL || speed_pwm_in2 == NULL || turn_pwm == NULL) {
        fprintf(stderr, "Failed to initialized.\n");
        return 1;
    }
    
    mraa_pwm_period_us(speed_pwm_in1,870); //1150Hz
    mraa_pwm_enable(speed_pwm_in1, 1);
    mraa_pwm_period_us(speed_pwm_in2,870);
    mraa_pwm_enable(speed_pwm_in2, 1);
    
    mraa_pwm_period_ms(turn_pwm,20);
    mraa_pwm_enable(turn_pwm, 1);
    
    mraa_pwm_write(turn_pwm, CENTER);
    mraa_pwm_write(speed_pwm_in1, 1.0f);
    mraa_pwm_write(speed_pwm_in2, 1.0f);
    
    char ch = getchar();
    
    IR_device *ir = new IR_device(3, "recv");
    

    Coordinate try1, try2, try3, try4;
    try1.x = 0;
    try1.y = 0;
    try2.x = -60;
    try2.y = 60;
    try3.x = -120;
    try3.y = 100;
    try4.x = 0;
    try4.y = 0;
    double newOffset = caculateMotionTime(try1, try2, PI / 2, ir);
    // cout << "ready to sleep\n";
    // usleep(500000);
    newOffset =  caculateMotionTime(try2, try3, newOffset, ir);
    caculateMotionTime(try3, try4, newOffset, ir);
    cout << "deleting ir\n";
    delete ir;
    cout << "program finished\n";
}
