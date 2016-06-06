/*
 * LibNfcManager.cpp
 *
 *  Created on: May 27, 2015
 *      Author: Maxime Teisseire
 */

#include <errno.h>
#include "../inc/LibNfcManager.hpp"

LibNfcManager::LibNfcManager(void)
{
    init();
}

LibNfcManager::~LibNfcManager(void)
{
    deinit();
}

void LibNfcManager::init()
{
    nfcManager_doInitialize ();
    nfcManager_registerTagCallback(&mTagCb);
    nfcSnep_registerClientCallback(&mSnepClientCb);
    nfcManager_enableDiscovery(DEFAULT_NFA_TECH_MASK,false, false, false);
}

void LibNfcManager::deinit()
{
    nfcManager_disableDiscovery();
    nfcSnep_deregisterClientCallback();
    nfcSnep_stopServer();
    nfcManager_deregisterTagCallback();
    nfcManager_doDeinitialize ();
}

void LibNfcManager::setTagCbOnTagArrival(void pCb (nfc_tag_info_t *pTagInfo))
{
    mTagCb.onTagArrival = pCb;
}

void LibNfcManager::setTagCbOnTagDeparture(void pCb())
{
    mTagCb.onTagDeparture = pCb;
}

void LibNfcManager::setSnepClientCbOnDeviceArrival(void pCb())
{
    mSnepClientCb.onDeviceArrival = pCb;
}

void LibNfcManager::setSnepClientCbOnDeviceDeparture(void pCb())
{
    mSnepClientCb.onDeviceDeparture = pCb;
}

void LibNfcManager::reverseTab(unsigned char* pTab,unsigned char* pReverseTab)
{
	int lIndex = 0;
    size_t tabLength = sizeof(unsigned char)*strlen((char*)pTab);
    for(lIndex = 0;lIndex < tabLength ; lIndex++)
    {
        pReverseTab[lIndex] = pTab[tabLength - 1 - lIndex];
    }
    return;

}

void LibNfcManager::createWifiMessage(unsigned char* pSsid,unsigned char* pPwd,unsigned char* pAuth,unsigned char* pEnc,unsigned char*pMsg,int *pMsgLength)
{
    int currentIndex =0;

    unsigned char header = 0xD2;
    const char *type = "application/vnd.wfa.wsc";
    size_t typeLength = sizeof(unsigned char)*strlen((char*)type);
    
    size_t pwdLength = (unsigned char) (sizeof(unsigned char)*strlen((char*)pPwd));
    unsigned char pwdSettings[] = {0x10,0x27,0x00,(unsigned char)pwdLength};
    
	size_t ssidLength = (unsigned char) (sizeof(unsigned char)*strlen((char*)pSsid));
    unsigned char ssidSettings[] = {0x10,0x45,0x00,(unsigned char)ssidLength};
    
	unsigned char authSettings[] = {0x10,0x03,0x00,0x02,0x00};

	unsigned char encSettings[] = {0x10,0x0F,0x00,0x02,0x00};

	size_t wifiInfoLength = (unsigned char) ( pwdLength + sizeof(pwdSettings) + ssidLength + sizeof(ssidSettings) + 0x01 + sizeof(authSettings) + 0x01 + sizeof(encSettings));
    
	unsigned char wifiInfoSettings[] = {0x10,0x0E,0x00,(unsigned char)wifiInfoLength};
    size_t payloadLength = (unsigned char) (wifiInfoLength + sizeof(wifiInfoSettings));
    *pMsgLength = payloadLength + typeLength + 3;


    //build message
    pMsg[currentIndex] = header;
    currentIndex += 1;
    pMsg[currentIndex] = typeLength;
    currentIndex += 1;
    pMsg[currentIndex] = payloadLength;
    currentIndex += 1;
    memcpy(&pMsg[currentIndex],type,typeLength);
    currentIndex += typeLength;

    memcpy(&pMsg[currentIndex],wifiInfoSettings,4);
    currentIndex += 4;
    memcpy(&pMsg[currentIndex],ssidSettings,4);
    currentIndex += 4;
    memcpy(&pMsg[currentIndex],pSsid,ssidLength);
    currentIndex += ssidLength;
    memcpy(&pMsg[currentIndex],pwdSettings,4);
    currentIndex += 4;
    memcpy(&pMsg[currentIndex],pPwd,pwdLength);
	currentIndex += pwdLength;
	
	memcpy(&pMsg[currentIndex],authSettings,5);
    currentIndex += 5;
	memcpy(&pMsg[currentIndex],pAuth,1);
    currentIndex += 1;

	memcpy(&pMsg[currentIndex],encSettings,5);
    currentIndex += 5;
	memcpy(&pMsg[currentIndex],pEnc,1);
    currentIndex += 1;


    return;
}

void LibNfcManager::createBluetoothMessage(unsigned char* pMacAddress,unsigned char* pDeviceName,unsigned char* pMsg,int* pMsgLength)
{
    int currentIndex =0;

    unsigned char header = 0xD2;
    const char *type = "application/vnd.bluetooth.ep.oob";
    size_t typeLength = sizeof(unsigned char)*strlen((char*)type);
    size_t macAddressLength = sizeof(unsigned char)*strlen((char*)pMacAddress);

    //According to the bluetooth spec address is reversed.
    unsigned char* reverseMacAddress = (unsigned char*)malloc(macAddressLength * sizeof(unsigned char));
    reverseTab(pMacAddress,reverseMacAddress);
    size_t deviceNameLength =  sizeof(unsigned char)*strlen((char*)pDeviceName);

    unsigned char bluetoothEIRDeviceName[] = {(unsigned char)deviceNameLength + 1 ,0x09};
    size_t payloadLength = (unsigned char) (deviceNameLength + sizeof(bluetoothEIRDeviceName) + macAddressLength + 2);
    *pMsgLength = payloadLength + typeLength + 3;

    //build message
    pMsg[currentIndex] = header;
    currentIndex += 1;
    pMsg[currentIndex] = typeLength;
    currentIndex += 1;
    pMsg[currentIndex] = payloadLength;
    currentIndex += 1;
    memcpy(&pMsg[currentIndex],type,typeLength);
    currentIndex += typeLength;
    pMsg[currentIndex] = payloadLength;
    currentIndex += 1;
    pMsg[currentIndex] = 0x00;
    currentIndex += 1;
    memcpy(&pMsg[currentIndex],reverseMacAddress,macAddressLength);
    currentIndex += macAddressLength;
    memcpy(&pMsg[currentIndex],bluetoothEIRDeviceName,2);
    currentIndex += 2;
    memcpy(&pMsg[currentIndex],pDeviceName,deviceNameLength);

    return;
}

bool LibNfcManager::writeNdefWiFiMessage(int mTagHandle,unsigned char* pSsid,unsigned char* pPwd, unsigned char* pAuth, unsigned char* pEnc)
{
    unsigned char* lWifiMessage = (unsigned char*)malloc(sizeof(unsigned char)*255);
    int lWifiMessageLength,ret = 0;
    createWifiMessage(pSsid, pPwd, pAuth, pEnc, lWifiMessage, &lWifiMessageLength);
    ret = nfcTag_writeNdef(mTagHandle, lWifiMessage, lWifiMessageLength);
    free (lWifiMessage);
    if(ret == 0)
        return true;
    else
        return false;
}

bool LibNfcManager::writeNdefBluetoothMessage(int mTagHandle,unsigned char* pMacAddress,unsigned char* pDeviceName)
{
    unsigned char* lBluetoothMessage = (unsigned char*)malloc(sizeof(unsigned char)*255);
    int lBluetoothMessageLength,ret = 0;
    createBluetoothMessage(pMacAddress,pDeviceName,lBluetoothMessage,&lBluetoothMessageLength);
    ret = nfcTag_writeNdef(mTagHandle, lBluetoothMessage, lBluetoothMessageLength);
    free (lBluetoothMessage);
    if(ret == 0)
        return true;
    else
        return false;
}

bool LibNfcManager::pushNdefBluetoothMessage(unsigned char* pMacAddress,unsigned char* pDeviceName)
{
int ret = 0;
    unsigned char* lBluetoothMessage = (unsigned char*)malloc(sizeof(unsigned char)*255);
    int lBluetoothMessageLength = 0;
    createBluetoothMessage(pMacAddress,pDeviceName,lBluetoothMessage,&lBluetoothMessageLength);
    unsigned char *lHandoverMessage = (unsigned char *)malloc(1024);
    int hs_len = ndef_createHandoverSelect(HANDOVER_CPS_ACTIVE, (char*)"00",
    		lBluetoothMessage, lBluetoothMessageLength,lHandoverMessage, 1024);
    if (hs_len > 0)
    {
        ret = nfcSnep_putMessage(lHandoverMessage, hs_len);
        free(lBluetoothMessage);
        free(lHandoverMessage);
        if(ret == 0)
            return true;
        else
            return false;
    }
    else
        return false;
}

bool LibNfcManager::pushNdefWiFiMessage(unsigned char* pSsid,unsigned char* pPwd,unsigned char* pAuth, unsigned char* pEnc)
{
    int ret = 0;
    unsigned char* lWifiMessage = (unsigned char*)malloc(sizeof(unsigned char)*255);
    int lWifiMessageLength = 0;
    createWifiMessage(pSsid,pPwd,pAuth,pEnc,lWifiMessage,&lWifiMessageLength);
    unsigned char *lHandoverMessage = (unsigned char *)malloc(1024);
    int hs_len = ndef_createHandoverSelect(HANDOVER_CPS_ACTIVE, (char*)"00",
    lWifiMessage, lWifiMessageLength,lHandoverMessage, 1024);
    if (hs_len > 0)
    {
        ret = nfcSnep_putMessage(lHandoverMessage, hs_len);
        free(lWifiMessage);
        free(lHandoverMessage);
        if(ret == 0)
            return true;
        else
           return false;
    }
    else
        return false;
}
