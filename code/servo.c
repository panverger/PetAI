// Copyright (c) 2021 David G. Young
// Copyright (c) 2015 Damian Kołakowski. All rights reserved.
// cc servo.c -lmosquitto -l bcm2835 -o servo


#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>

#include <stdio.h>
#include <bcm2835.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <mosquitto.h>


// MQTT broker settings
struct mosquitto *mosq;
char *Host =  "test.mosquitto.org"; //"127.0.0.1";
int Port = 1883; //8081; //1883;
const char *USERNAME = "gataki";
const char *PASSWORD = "gataki";

// servo settings
int pinA = 18;
int pinB = 23;
int prevangle=0;
bool opened=false;
bool locked = false; 

int RSSI = 0;
int levelSignal = -70;

// Function to smoothly transition from value A to value B based on percentage change
float smooth_transition(float A, float B, float percentage) {
    return A + percentage * (B - A);
}


void move_servos(int angle, int delayTime) {
	int j, angleB;
	float hightimeA = 10.3*(float)angle + 546;
	float hightimeApre = 10.3*(float)prevangle + 546;
	angleB = 180-angle;
	float hightimeB = 10.3*(float)(180-angle) + 546;
	float hightimeBpre = 10.3*(float)(180-prevangle) + 546;
	int pulse = (int)abs(((angle - prevangle)*33)/180) ;
	for(j=0; j<pulse; j++)//exucting pulses
	{ 
				
		bcm2835_gpio_set(pinB);//B high
		bcm2835_delayMicroseconds(hightimeB);
		bcm2835_gpio_clr(pinB);//B low
		bcm2835_delayMicroseconds(20000 - hightimeB);//each pulse is 20ms
		
		bcm2835_gpio_set(pinA);//A high
		bcm2835_delayMicroseconds(hightimeA);
		bcm2835_gpio_clr(pinA);//A low
		bcm2835_delayMicroseconds(20000 - hightimeA);//each pulse is 20ms

		hightimeB = smooth_transition(hightimeB, hightimeBpre, pulse/50);
		hightimeA = smooth_transition(hightimeA, hightimeApre, pulse/50);
		delay(delayTime);
		}
	prevangle = angle;
	
}

void open_door() {
	if (!locked) { 
	printf("opening door\n");
	mosquitto_publish(mosq, NULL, "gatakia/servo", 6, "opened", 0, false);
	opened = true;
	move_servos(100,50);
	sleep(5);
	} else
	{
		printf("no move locked is true\n");
	}
}
	
void close_door() {
	if (!locked) { 
	printf("closing door\n");
	mosquitto_publish(mosq, NULL, "gatakia/servo", 6, "closed", 0, false);
	opened = false;
	move_servos(160,50);
	} else
	{
		printf("no move locked is true\n");
	}
}

void check_RSSI() {
	if (RSSI>levelSignal && !opened) {
		open_door();
	} 
	if (RSSI<=levelSignal && opened) {
		close_door();
	}
}

/* Callback function that will be called when a message is received. */
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    char *topicRSSI="gatakia/RSSI";
    char *topicCMD="gatakia/cmd";
    char *topicLOCKED="gatakia/locked";
    char *topicROT="gatakia/servoRot";
    
    printf("Received message on topic %s: %s\n", msg->topic, (char*)msg->payload);
    if ( strcmp(msg->topic, topicRSSI) == 0) {
		RSSI = atoi(msg->payload);
		//printf("RSSI: %d \n", RSSI);
		check_RSSI();
	}
    if ( strcmp(msg->topic, topicCMD) == 0) {
	//RSSI = atoi(msg->payload);
	printf("cmd: %s \n", msg->payload);
	//set_doors();
	if (strcmp(msg->payload,"open")==0) {
		printf("rcv open from cmd\n");
		open_door();
		}
	if (strcmp(msg->payload,"close")==0) {
		printf("rcv close from cmd\n");
		close_door();
		}
			
	}
	
     if ( strcmp(msg->topic, topicLOCKED) == 0) {
	if (strcmp(msg->payload,"true")==0) {
		printf("rcv locked true\n");
		locked = true;
		}
	if (strcmp(msg->payload,"false")==0) {
		printf("rcv locked false\n");
		locked = false;
		}
	}	
	
     if ( strcmp(msg->topic, topicROT) == 0) {
		int servo_angle = atoi(msg->payload);
		//printf("RSSI: %d \n", RSSI);
		move_servos(servo_angle,150);
	}
}

void on_connect(struct mosquitto *mosq, void *userdata, int rc)
{
    if (rc == 0) {
		printf("Connect ok \n");    
        mosquitto_publish(mosq, NULL, "gatakia/servo", 9, "listening", 0, false);
        rc = mosquitto_subscribe(mosq, NULL, "gatakia/#", 0); 
        if (rc!=MOSQ_ERR_SUCCESS) {
			printf("Error on subscribe");
		}     

    } else {
        printf("Connect failed: %s\n", mosquitto_strerror(rc));
	// Attempt to reconnect
        mosquitto_reconnect(mosq);
    }
}


int init_mqtt() {
    int rc;

    mosquitto_lib_init();

    mosq = mosquitto_new("user65424", true, NULL);

	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_message_callback_set(mosq, on_message);
	mosquitto_disconnect_callback_set(mosq, on_connect);
	
    rc = mosquitto_connect(mosq, Host, Port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        printf("Error: Unable to connect to MQTT broker: %s\n", mosquitto_strerror(rc));
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }
    // Set the username and password for the connection
    mosquitto_username_pw_set(mosq, "gataki", "gataki");

}


// Αποστολη μνμ σε MQTT broker
void send_msg( char *s)
{
	//char str[16]; // Allocate enough space for the converted string
	
    //sprintf(str, "%s \n", s); // %d is the format specifier for integers

    printf("->>>> %s \n", s);
	int ret = mosquitto_publish(mosq, NULL, "gatakia/servo", strlen(s), s, 0, false);
	if (ret>0) printf("Send error %d \n",ret);
	
}


int main()
{
    	
    // προετοιμασία servo
    if(!bcm2835_init()) return 1;
    int angleA, prevangleA, pulse;

    bcm2835_gpio_fsel(pinA, BCM2835_GPIO_FSEL_OUTP); //set pin 18 as output
    bcm2835_gpio_fsel(pinB, BCM2835_GPIO_FSEL_OUTP); //set pin 23 as output
    
   
    // δοκιμαστική κίνηση Servo
    move_servos(140, 100);
    move_servos(160, 100);
    
    int rc = init_mqtt();
    mosquitto_loop_forever(mosq,-1,1);
    
}
