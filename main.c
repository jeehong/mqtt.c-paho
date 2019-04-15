/*******************************************************************************
 * Copyright (c) 2012, 2016 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - change delimiter option from char to string
 *    Al Stockdill-Mander - Version using the embedded C client
 *    Ian Craggs - update MQTTClient function names
 *******************************************************************************/
#include <stdio.h>
#include <memory.h>
#include "MQTTClient.h"

#include <stdio.h>
#include <signal.h>

#include <sys/time.h>
#include <time.h>

#include "main.h"

volatile int toStop = 0;

struct opts_struct
{
    char* clientid;
    int nodelimiter;
    char* delimiter;
    enum QoS qos;
    char* username;
    char* password;
    char* host;
    int port;
    int showtopics;
} opts =
{
    (char*)"stdout-subscriber", 0, (char*)"\n", QOS2, NULL, NULL, (char*)"localhost", 1883, 0
};

Network n;
MQTTClient c;


void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

void messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;
	static unsigned int cnt = 0;

	cnt++;
	printf("[cnt=%d]", cnt);
	if (opts.showtopics)
		printf("%.*s\t", md->topicName->lenstring.len, md->topicName->lenstring.data);
	if (opts.nodelimiter)
		printf("%.*s", (int)message->payloadlen, (char*)message->payload);
	else
		printf("%.*s%s", (int)message->payloadlen, (char*)message->payload, opts.delimiter);
	//fflush(stdout);
}

unsigned int fill_send_buffer(char * b)
{
    struct timeval tv;
    struct tm *tm;
    float t;

    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    t = cputemp_read();
    sprintf(b, "[%d-%d-%d %d %02d:%02d:%02d %03ld]temperature is %3.3fC",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_wday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000, t);

    return (strlen(b));
}

int main(int argc, char** argv)
{
	int rc = 0;
	unsigned char buf[100];
	unsigned char readbuf[100];
    char buffer[100];
    MQTTMessage msg;

	char* topic = "temp";

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	opts.host = "94.191.29.119";
	opts.port = 1883;
	system("/usr/local/bin/ntpdate ntp.aliyun.com");

	NetworkInit(&n);
	NetworkConnect(&n, opts.host, opts.port);
	MQTTClientInit(&c, &n, 1000, buf, 100, readbuf, 100);

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.clientID.cstring = opts.clientid;
	data.username.cstring = opts.username;
	data.password.cstring = opts.password;

	data.keepAliveInterval = 1000;
	data.cleansession = 1;
	printf("Connecting to %s %d\n", opts.host, opts.port);
	
	rc = MQTTConnect(&c, &data);
	printf("Connected %d\n", rc);
    
    /* printf("Subscribing to %s\n", topic); */
	rc = MQTTSubscribe(&c, topic, opts.qos, messageArrived);
	printf("Subscribed %d\n", rc);
    msg.dup = 0;
    msg.id = 0;
    msg.payload = buffer;
    msg.retained = 0;
    cputemp_init();
	while (!toStop)	{
		rc = MQTTYield(&c, 1000);
		if(rc != SUCCESS) {
		    NetworkInit(&n);
		    NetworkConnect(&n, opts.host, opts.port);
		    MQTTClientInit(&c, &n, 1000, buf, 100, readbuf, 100);
		    rc = MQTTConnect(&c, &data);
		    printf("[%s@%d] Reconnecting the server, rc = %d\r\n", __FUNCTION__, __LINE__, rc);
		} else {
            msg.payloadlen = fill_send_buffer(buffer);
            MQTTPublish(&c, "jan", &msg);
            if(rc != SUCCESS) {
                printf("[%s@%d] rc = %d\r\n", __FUNCTION__, __LINE__, rc);
            }
		}
	}
	
	printf("Stopping\n");

	MQTTDisconnect(&c);
	NetworkDisconnect(&n);

	cputemp_deinit();

	return 0;
}


