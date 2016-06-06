/*
 * LibNfcManager.hpp
 *
 *  Created on: May 27, 2015
 *      Author: Maxime Teisseire
 */
#ifndef LIBNFCMANAGER_
#define LIBNFCMANAGER_

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "linux_nfc_api.h"

class LibNfcManager
{
//struct and attributs//
public :

private :
    nfcTagCallback_t mTagCb;
    nfcSnepClientCallback_t mSnepClientCb;

//methods
public :
    LibNfcManager();
    ~LibNfcManager();
    void setTagCbOnTagArrival(void (nfc_tag_info_t *pTagInfo));
    void setTagCbOnTagDeparture(void ());
    void setSnepClientCbOnDeviceArrival(void ());
    void setSnepClientCbOnDeviceDeparture(void ());
	bool writeNdefWiFiMessage(int mTagHandle,unsigned char* pSsid,unsigned char* pPwd, unsigned char* pAuth, unsigned char* pEnc);
    bool writeNdefBluetoothMessage(int mTagHandle,unsigned char* pMacAddress,unsigned char* pDeviceName);
    bool pushNdefWiFiMessage(unsigned char* pSsid,unsigned char* pPwd,unsigned char* pAuth, unsigned char* pEnc);
    bool pushNdefBluetoothMessage(unsigned char* pMacAddress,unsigned char* pDeviceName);
private :
    void init();
    void deinit();
    void createWifiMessage(unsigned char* pSsid,unsigned char* pPwd,unsigned char* pAuth,unsigned char* pEnc,unsigned char*pMsg,int *pMsgLength);
    void createBluetoothMessage(unsigned char * pMacAddress,unsigned char* pDeviceName,unsigned char *pMsg,int *pMsgLength);
    void reverseTab(unsigned char* pTab,unsigned char* pReverseTab);
};
#endif /* LIBNFCMANAGER_ */
