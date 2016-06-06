/******************************************************************************
 *
 *  Copyright (C) 2015 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License")
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <stdarg.h>
#include <dlfcn.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "../inc/Demo.hpp"

const unsigned char* DEFAULT_PASSWORD 			= (unsigned char*)"12345678";
const unsigned char* DEFAULT_SSID				= (unsigned char*)"NXP";
const unsigned char DEFAULT_AUTHENTICATION[] 	= {0x20};//WPA2PSK
const unsigned char DEFAULT_ENCRYPTION[] 		= {0x08};//AES

const unsigned char* DEFAULT_MAC_ADDRRESS_ASCII = (unsigned char*)"31:32:33:34:35:36";
const unsigned char* DEFAULT_DEVICE_NAME = (unsigned char*)"NXP_BT";
const unsigned char* DEFAULT_MAC_ADDRESS = (unsigned char*)"123456";

//application state
enum {
    EVENT_TAG_ARRIVAL = 1,
    EVENT_TAG_DEPARTURE,
    EVENT_SNEP_CLIENT_ARRIVAL,
    EVENT_SNEP_CLIENT_DEPARTURE,
    EVENT_EXIT_APPLICATION
};

enum STATE {
    NONE = 0,
    WIFI,
    BLUETOOTH
};

typedef struct nfc_callback_data
{
    /* Semaphore used to wait for callback */
    sem_t sem;
    /* Used to store the event sent by the callback */
    int event;
    /* Used to provide a local context to the callback */
    void* pContext;
} nfc_callback_data_t;

//catch the lib-nfc event
static nfc_callback_data_t cb_data;
static bool running = false;
static int mTagHandle;
static nfc_tag_info_t mTagInfo;

static void printHelp()
{
    fprintf(stderr, "%s",
                    "Options: \n"
                    "--help										Display this information\n"
                    "--wifi										Set the WiFi configuration with default parameters\n"
                    "--wifi -ssid <ssid> -pwd <password> -auth <authentication> -enc <encryption>	Set the WiFi configuration\n"
                    "--bt , --bluetooth								Set the Bluetooth configuration with default parameter\n"
                    "--bt , --bluetooth -addr <mac_address> -name <device_name> 			Set Bluetooth configuration\n\n"

			"<authentication> supported : Open, WPAPSK, WPA2PSK(default), WPA, WPA2, Shared \n"
			"<encryption>	 supported : None, WEP, TKIP, AES(default)\n");
                    return;
}

unsigned char* hexDecode(unsigned char *in, size_t len,unsigned char *out)
{
        unsigned int i, t, hn, ln;
        for (t = 0,i = 0; i < len; i+=2,++t) {
            if(in[i] == 0x3A)
                i+=1;
            hn = in[i] > '9' ? in[i] - 'A' + 10 : in[i] - '0';
            ln = in[i+1] > '9' ? in[i+1] - 'A' + 10 : in[i+1] - '0';
            out[t] = (hn << 4 ) | ln;

        }
        return out;
}

static void printMessage(const char*pMessage)
{
#ifdef DEBUG
    printf("%s",pMessage);
#endif
}

void SigInitHandler(int sig)
{
    (void)(sig);
    printMessage("\nExit is processing !\n");
    cb_data.event = EVENT_EXIT_APPLICATION;
    sem_post(&cb_data.sem);
}

bool nfcCbDataInit(nfc_callback_data* pCallbackData, void* pContext)
{
    /* Create semaphore */
    if(sem_init(&pCallbackData->sem, 0, -1) == -1)
        return false;

    /* Set default status value */
    pCallbackData->event = -1;

    /* Copy the context */
    pCallbackData->pContext = pContext;
    sem_post(&pCallbackData->sem);

    return true;
}

bool nfcCbDataDeinit(nfc_callback_data* pCallbackData)
{
    return !sem_destroy(&pCallbackData->sem);
}

void onTagArrival (nfc_tag_info_t *pTag)
{
    (void)pTag;
    cb_data.event = EVENT_TAG_ARRIVAL;
    mTagHandle = pTag->handle;
    memcpy(&mTagInfo, pTag, sizeof(nfc_tag_info_t));
    sem_post(&cb_data.sem);
}

void onTagDeparture (void)
{
    /*cb_data.event = EVENT_TAG_DEPATURE;
    sem_post(&cb_data.sem);*/
}

void onSnepClientDeviceArrival()
{
    cb_data.event = EVENT_SNEP_CLIENT_ARRIVAL;
    sem_post(&cb_data.sem);
}

void onSnepClientDeviceDeparture()
{
    /*cb_data.event = EVENT_SNEP_CLIENT_DEPATURE;
    sem_post(&cb_data.sem);*/
}

bool checkMacAddress(unsigned char * pMacAddress,size_t pSize)
{
    if(pSize != 17)
        return false;
    uint i = 0;
    for(i = 0; i < pSize ; i++)
    {
        //We should have a ":" in this case
        if(i % 3 == 2)
        {
            if(pMacAddress[i] != ':')
            {
                return false;
            }
        }
        else
        {
            if(pMacAddress[i] >= 0x61 && pMacAddress[i] <= 0x66)
            {
                pMacAddress[i] -= 0x20;
            }
            if((pMacAddress[i] < '0' || pMacAddress[i] > '9')  &&
               (pMacAddress[i] < 'A' || pMacAddress[i] > 'F'))
            {
                return false;
            }
        }
    }
    return true;
}
int main( int argc, const char* argv[] )
{
    int i;
    STATE lState = NONE;
    bool flagAddressMac = false;
    unsigned char* ssid = NULL;
    unsigned char* pwd = NULL;
	unsigned char* auth = NULL;
    unsigned char* enc = NULL;

    unsigned char* macAddressASCII = NULL;
    unsigned char* deviceName = NULL;
    unsigned char* macAddress = NULL;
    if(argc == 1) {
        printHelp();
        return 0;
    }
    i = 1;
    if(strcmp(argv[i], "--help") == 0)
    {
       printHelp();
       return 0;
    }
    else if(strcmp(argv[i], "--wifi") == 0)
    {
        lState = WIFI;
        ssid = (unsigned char*)malloc(sizeof(unsigned char)*strlen((char*)DEFAULT_SSID));
        memcpy(ssid,(unsigned char*)DEFAULT_SSID,strlen((char*)DEFAULT_SSID));
        pwd = (unsigned char*)malloc(sizeof(unsigned char)*strlen((char*)DEFAULT_PASSWORD));
        memcpy(pwd,(unsigned char*)DEFAULT_PASSWORD,strlen((char*)DEFAULT_PASSWORD));

		auth = (unsigned char*)malloc(sizeof(unsigned char)*strlen((char*)DEFAULT_AUTHENTICATION));
        memcpy(auth,(unsigned char*)DEFAULT_AUTHENTICATION,strlen((char*)DEFAULT_AUTHENTICATION));
        enc = (unsigned char*)malloc(sizeof(unsigned char)*strlen((char*)DEFAULT_ENCRYPTION));
        memcpy(enc,(unsigned char*)DEFAULT_ENCRYPTION,strlen((char*)DEFAULT_ENCRYPTION));

        for(i = 2;i < argc ; i++)
        {
            if (strcmp(argv[i], "-ssid") == 0)  /* Process optional arguments. */
            {
                i = i+1;
                memset(ssid,0x00,sizeof(unsigned char)*strlen((char*)DEFAULT_SSID));
                ssid = (unsigned char*)malloc(sizeof(unsigned char)*strlen(argv[i]));
                strcpy((char*)ssid,argv[i]);
            }
            else if (strcmp(argv[i], "-pwd") == 0)  /* Process optional arguments. */
            {
                i = i+1;
                memset(pwd,0x00,sizeof(unsigned char)*strlen((char*)DEFAULT_PASSWORD));
                pwd = (unsigned char*)malloc(sizeof(unsigned char)*strlen(argv[i]));
                strcpy((char*)pwd,argv[i]);
            }
			else if (strcmp(argv[i], "-auth") == 0)  /* Process optional arguments. */
			{
				i= i+1;
				     if (strcmp(argv[i], "Open") 	==0) auth[0]= 0x01;
				else if (strcmp(argv[i], "WPAPSK") 	==0) auth[0]= 0x02; 
				else if (strcmp(argv[i], "Shared") 	==0) auth[0]= 0x04;
				else if (strcmp(argv[i], "WPA") 	==0) auth[0]= 0x08;
				else if (strcmp(argv[i], "WPA2") 	==0) auth[0]= 0x10;
				else if (strcmp(argv[i], "WPA2PSK") ==0) auth[0]= 0x20;
			}
			else if (strcmp(argv[i], "-enc") == 0)  /* Process optional arguments. */
			{
				i= i+1;
					 if (strcmp(argv[i], "None") 	==0) enc[0]= 0x01;
				else if (strcmp(argv[i], "WEP") 	==0) enc[0]= 0x02;
				else if (strcmp(argv[i], "TKIP") 	==0) enc[0]= 0x04;
				else if (strcmp(argv[i], "AES") 	==0) enc[0]= 0x08;				
			}

        }
    }
    else if(strcmp(argv[i], "--bt") == 0 || strcmp(argv[i], "--bluetooth") == 0)
    {
        size_t macAddressLength = 0;
        lState = BLUETOOTH;
        deviceName = (unsigned char*)malloc(sizeof(unsigned char*)*strlen((char*)DEFAULT_DEVICE_NAME));
        memcpy(deviceName,(unsigned char*)DEFAULT_DEVICE_NAME,strlen((char*)DEFAULT_DEVICE_NAME));
        macAddressASCII = (unsigned char*)malloc(sizeof(unsigned char*)*strlen((char*)DEFAULT_MAC_ADDRRESS_ASCII));
        memcpy(macAddressASCII,(unsigned char*)DEFAULT_MAC_ADDRRESS_ASCII,strlen((char*)DEFAULT_MAC_ADDRRESS_ASCII));
        macAddress =  (unsigned char*)malloc(sizeof(unsigned char*)*strlen((char*)DEFAULT_MAC_ADDRESS));
        memcpy(macAddress,(unsigned char*)DEFAULT_MAC_ADDRESS,strlen((char*)DEFAULT_MAC_ADDRESS));
        for(i = 2;i < argc ; i++)
            {
                if (strcmp(argv[i], "-addr") == 0)  /* Process optional arguments. */
                {
                    i = i+1;
                    macAddressLength = sizeof(unsigned char)*strlen(argv[i]);
                    memset(macAddressASCII,0x00,sizeof(unsigned char)*strlen((char*)DEFAULT_MAC_ADDRRESS_ASCII));
                    macAddressASCII = (unsigned char*)malloc(sizeof(unsigned char)*strlen((char*)DEFAULT_MAC_ADDRRESS_ASCII));
                    memset(macAddress,0x00,sizeof(unsigned char)*strlen((char*)DEFAULT_MAC_ADDRESS));
                    macAddress = (unsigned char*)malloc(sizeof(unsigned char)*6);
                    strcpy((char*)macAddressASCII,argv[i]);
                    flagAddressMac = checkMacAddress(macAddressASCII,macAddressLength);
                    hexDecode(macAddressASCII,macAddressLength,macAddress);


                }
                else if (strcmp(argv[i], "-name") == 0)  /* Process optional arguments. */
                {
                    i = i+1;
                    memset(deviceName,0x00,sizeof(unsigned char)*strlen((char*)DEFAULT_DEVICE_NAME));
                    deviceName = (unsigned char*)malloc(sizeof(unsigned char)*strlen(argv[i]));
                    strcpy((char*)deviceName,argv[i]);
                }
            }
    }
    else
    {
        printHelp();
        return 0;
    }
    if(lState == WIFI)
    {
        printf("ssid=%s\npwd=%s\n",ssid,pwd);
    }
    else if(lState == BLUETOOTH)
    {
        if(flagAddressMac == false)
        {
            printf("You need to provide a mac address with this format XX:XX:XX:XX:XX:XX\n");
            printHelp();
            return 0;
        }
        else
        {
            printf("device name=%s\nmac address=%s\n",deviceName,macAddressASCII);
        }
    }

    signal(SIGINT, SigInitHandler);
    //signal(SIGINT | SIGTSTP, SigInitHandler);
    LibNfcManager *mManager;
    mManager = new LibNfcManager;
    mManager->setSnepClientCbOnDeviceArrival(&onSnepClientDeviceArrival);
    mManager->setSnepClientCbOnDeviceDeparture(&onSnepClientDeviceDeparture);
    mManager->setTagCbOnTagArrival(&onTagArrival);
    mManager->setTagCbOnTagDeparture(&onTagDeparture);
    nfcCbDataInit(&cb_data,NULL);
    running = true;

    while(running)
    {
        printMessage("\n...Waiting for a Tag or P2P device ...\n");
        sem_wait(&cb_data.sem);
        switch (cb_data.event)
        {
            case EVENT_TAG_ARRIVAL:
            {
                if(lState == WIFI)
                {
                    if(mManager->writeNdefWiFiMessage(mTagHandle,ssid,pwd,auth,enc))
                        printMessage("\tWrite Wifi NDefmessage succeeded !\n");
                    else
                        printMessage("\tWrite Wifi NDefmessage failed !\n");
                }
                else if(lState == BLUETOOTH)
                {
                    if(mManager->writeNdefBluetoothMessage(mTagHandle,macAddress,deviceName))
                        printMessage("\tWrite Bluetooth NDefMessage succeeded !\n");
                    else
                        printMessage("\tWrite Bluetooth NDefMessage failed !\n");
                }
            }
            break;
            case EVENT_SNEP_CLIENT_ARRIVAL:
            {
                if(lState == WIFI)
                {
                    if(mManager->pushNdefWiFiMessage(ssid,pwd,auth,enc))
                         printMessage("\tPush Wifi NDefmessage succeeded !\n");
                    else
                         printMessage("\tPush Wifi NDefmessage failed !\n");
                }
                else if(lState == BLUETOOTH)
                {
                    //unsigned char addressMacTest[6] = {0x01,unsigned char)0x02,unsigned char)0x03,unsigned char)0x04,unsigned char)0x05,unsigned char)0x06};
                    if(mManager->pushNdefBluetoothMessage(macAddress,deviceName))
                        printMessage("\tPush Bluetooth NDefmessage succeeded !\n");
                    else
                        printMessage("\tPush Bluetooth NDefmessage failed !\n");
                }
            }
            break;
            case EVENT_EXIT_APPLICATION:
            {
                running = false;
                nfcCbDataDeinit(&cb_data);
                delete mManager;
            }
            break;
            default:
            {
                //Never Happen
            }
            break;
        }
    }
}
