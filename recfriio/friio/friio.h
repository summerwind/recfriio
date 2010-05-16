#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CFNumber.h>

#include "b25/arib_std_b25.h"
#include "b25/b_cas_card.h"
#define __STDC_FORMAT_MACROS

#include "StreamBuffer.h"

// LED用定数
static const int LED_COLOR_PURPLE = 0;
static const int LED_COLOR_GREEN  = 1;
static const int LED_COLOR_RED    = 2;

// スレッドデータ構造体
// -----------------------------------------------
typedef struct {
	IOUSBInterfaceInterface**	interface;
	FILE*						fd;
	ARIB_STD_B25*				b25;
	B_CAS_CARD*					bcas;
	StreamBuffer*				buffer;
	int							flag;
} ThreadData;


// Friioクラス
// -----------------------------------------------
class Friio {
private:
	mach_port_t					masterPort;
	io_service_t				usbDeviceRef;
	io_service_t				usbInterfaceRef;
	IOUSBDeviceInterface**		dev;
	IOUSBInterfaceInterface**	interface;
	ARIB_STD_B25				*b25;
	B_CAS_CARD					*bcas;
	char*						filePath;
	int							status; 

public:
	Friio();
	~Friio();
		
	int _findDevice();
	int _findInterface();
	int _setupInterface();
	int _setupB25();
	
	int _sendRequest(uint16_t wValue, uint16_t wIndex);
	int _initStageA();
	int _initStageB();
	int _initStream();
	
	int	  getStatus();
	int   setChannel(int channel);
	float getSignalLevel();
	void  setLED(int color);
		
	int rec(char* file, int sec);
};


// スレッド関数とコールバック関数
// -----------------------------------------------
void* _friioWorker(void* arg);
void* _playerWorker(void *arg);
void _readCallback(ThreadData* tData, IOReturn result, void* arg);


