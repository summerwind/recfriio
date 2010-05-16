#include "Friio.h"

// コンストラクタ
// -----------------------------------------------------------
Friio::Friio() {
	int result = 0;
	
	// ステータス初期化
	status = 0;
	
	// デバイスの検索
	result = _findDevice();
	if(!result) { 
		status = 1;
		return;
	}
	// インターフェースを検索
	result = _findInterface();
	if(!result) {
		status = 2;
		return;
	}
	// インターフェースをセットアップ
	result = _setupInterface();
	if(!result) {
		status = 3;
		return;
	}
	// b25をセットアップ
	result = _setupB25();
	if(!result) {
		status = 4;
		return;
	}
	
	// 初期化処理
	_initStageA();
	_initStageB();
	_initStream();
	
	// LEDを緑に変更
	setLED(LED_COLOR_GREEN);
}


// デストラクタ
// -----------------------------------------------------------
Friio::~Friio() {
    IOReturn error;
	
	// LEDを紫に変更
	setLED(LED_COLOR_PURPLE);
	
	// インターフェースを閉じる
	error = (*interface)->USBInterfaceClose(interface);
    if(error) {
		printf("Unable to close interface.\n");
		return;
    }
	// インターフェースを解放
	error = (*interface)->Release(interface);
	if(error) {
		printf("Unable to release interface.\n");
		return;
    }

	// デバイスを閉じる
	error = (*dev)->USBDeviceClose(dev);
    if(error) {
		printf("Unable to close device.\n");
		return;
	}
	// デバイスの解放
	error = (*dev)->Release(dev);
	if(error) {
		printf("Unable to release device.\n");
		return;
    }
	
	// b25の解放
	b25->release(b25);
	b25 = NULL;
	bcas->release(bcas);
	bcas = NULL;
	
	// マスターポートの解放
    mach_port_deallocate(mach_task_self(), masterPort);
}


// デバイス検索メソッド
// -----------------------------------------------------------
int Friio::_findDevice() {
	SInt32					idVendor = 0x7A69;
    SInt32					idProduct = 0x0001;
	CFNumberRef				numberRef;
	CFMutableDictionaryRef 	matchingDictionary = 0;
    io_iterator_t			iterator = 0;
	kern_return_t			error;
	
	// マスターポートを生成
	error = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if(error) {
		printf("Could not open master port.\n");
		return 0;
	}
	
	// 辞書を生成
	matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);
	// ベンダーIDを辞書に登録
	numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idVendor);
	CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBVendorID), numberRef);
    CFRelease(numberRef);
	numberRef = 0;
	// プロダクトIDを辞書に登録
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idProduct);
	CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBProductID), numberRef);
    CFRelease(numberRef);
    numberRef = 0;
	
	// Friioを探す
	error = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
	matchingDictionary = 0;
	if(error) {
		printf("Unable to find device.\n");
		return 0;
	}
	
	// USBデバイスのリファレンスを取得
	usbDeviceRef = IOIteratorNext(iterator);
	if(!usbDeviceRef) {
		printf("Unable to find device.\n");
		return 0;
	}
	
	// 後処理
	IOObjectRelease(iterator);
	iterator = 0;
	
	return 1;
}


// インターフェース検索メソッド
// -----------------------------------------------------------
int Friio::_findInterface() {
    IOCFPlugInInterface**		iodev;
    IOUSBFindInterfaceRequest	interfaceRequest;
    io_iterator_t				iterator;
	SInt32						score;
	IOReturn					error;
	
	// プラグインインターフェースの生成
	error = IOCreatePlugInInterfaceForService(
		usbDeviceRef,
		kIOUSBDeviceUserClientTypeID,
		kIOCFPlugInInterfaceID,
		&iodev,
		&score
	);
	if(error) {
		printf("Unable to create interface. %x\n", error);
		return 0;
    }
	// クエリインターフェース
	error = (*iodev)->QueryInterface(
		iodev, 
		CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245),
		(void**)&dev
	);
	// プラグインインターフェースを放棄
	IODestroyPlugInInterface(iodev);
	if(error || !dev) {
		printf("Unable to destroy interface.\n");
		return 0;
    }
	
	// デバイスのオープン
	error = (*dev)->USBDeviceOpen(dev);
	if(error) {
		printf("Unable to open device. %x\n", error);
		return 0;
    }
	
	// インターフェースイテレータの生成
	interfaceRequest.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
    interfaceRequest.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    interfaceRequest.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	interfaceRequest.bAlternateSetting  = kIOUSBFindInterfaceDontCare;
	error = (*dev)->CreateInterfaceIterator(dev, &interfaceRequest, &iterator);
	if(error) {
		printf("Unable to create interface iterator.\n");
		(*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return 0;
	}
	
	// インターフェースリファレンスの取得
	usbInterfaceRef = IOIteratorNext(iterator);
	if(!usbInterfaceRef) {
		printf("Unable to find interface.\n");
		return 0;
	}
	
	// 後処理
	IOObjectRelease(iterator);
    iterator = 0;
	
	return 1;
}


// インターフェースセットアップメソッド
// -----------------------------------------------------------
int Friio::_setupInterface() {
    IOCFPlugInInterface**	iodev;
    SInt32					score;
	IOReturn				error;

	// プラグインインターフェースの生成
	error = IOCreatePlugInInterfaceForService(
		usbInterfaceRef,
		kIOUSBInterfaceUserClientTypeID,
		kIOCFPlugInInterfaceID,
		&iodev,
		&score
	);
	// クエリインターフェース
	error = (*iodev)->QueryInterface(
		iodev,
		CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245),
		(void**)&interface
	);
	// プラグインインターフェースを放棄
	IODestroyPlugInInterface(iodev);
	if(error || !interface){
		printf("Unable to create a device interface.\n");
		return 0;
    }
	
	// インターフェースを開く
	error = (*interface)->USBInterfaceOpen(interface);
	if(error) {
		printf("Unable to open interface.\n");
		return 0;
    }
	
	return 1;
}


// b25セットアップメソッド
// -----------------------------------------------------------
int Friio::_setupB25() {
	int error;
	
	b25 = create_arib_std_b25();
	bcas = create_b_cas_card();
	
	// b25の初期化
	error = b25->set_multi2_round(b25, 4);
	error = b25->set_strip(b25, 0);
	error = b25->set_emm_proc(b25, 0);
	
	// B-CASの初期化
	error = bcas->init(bcas);
	
	// b25にB-CASを設定
	error = b25->set_b_cas_card(b25, bcas);
	
	if(error) {
		printf("Unable to setup b25. (B-CAS Card Error)\n");
		return 0;
	}
	
	return 1;
}


// リクエスト送信関数
// -----------------------------------------------------------
int Friio::_sendRequest(uint16_t wValue, uint16_t wIndex) {
	uint8_t		buffer[32];
	IOReturn	error;
	
	// リクエストを生成
	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
	req.bRequest = 0x01;
	req.wValue = wValue;
	req.wIndex = wIndex;
	req.wLength = 0;
	req.wLenDone = 0;
	req.pData = buffer;
	
	// Friioに送信
	error = (*interface)->ControlRequest(interface, 0, &req);
	if(error) {
		printf("Unable to send request.\n");
		return 0;
	}
	
	return 1;
}


// 初期化処理Aメソッド
// -----------------------------------------------------------
int Friio::_initStageA() {
	int			error_flag;
	uint8_t		buffer1[32], buffer2[32];
	IOReturn	error;

	// リクエスト送信
	error_flag = _sendRequest(0x0002, 0x0011);
	// リクエストの送信に失敗したら初期化失敗
	if(!error_flag) {
		printf("Initialization failed.\n");
		return 0;
	}
	_sendRequest(0x0000, 0x0011);
	
	IOUSBDevRequest req1;
	req1.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
	req1.bRequest = 0x02;
	req1.wValue = 0xA000;
	req1.wIndex = 0x00FE;
	req1.wLength = 1;
	req1.wLenDone = 0;
	req1.pData = buffer1;
	error = (*interface)->ControlRequest(interface, 0, &req1);

	IOUSBDevRequest req2;
	req2.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
	req2.bRequest = 0x02;
	req2.wValue = 0x3000;
	req2.wIndex = 0x0080;
	req2.wLength = 1;
	req2.wLenDone = 0;
	req2.pData = buffer2;
	error = (*interface)->ControlRequest(interface, 0, &req2);
	
	_sendRequest(0x0004, 0x0030);
	
	return 1;
}


// 初期化処理Bメソッド
// -----------------------------------------------------------
int Friio::_initStageB() {
	_sendRequest(0x3040, 0x01);
	_sendRequest(0x3038, 0x04);
	_sendRequest(0x3040, 0x05);
	_sendRequest(0x3040, 0x07);
	_sendRequest(0x304F, 0x0F);
	_sendRequest(0x3021, 0x11);
	_sendRequest(0x300B, 0x12);
	_sendRequest(0x302F, 0x13);
	_sendRequest(0x3031, 0x14);
	_sendRequest(0x3002, 0x16);
	_sendRequest(0x30C4, 0x21);
	_sendRequest(0x3020, 0x22);
	_sendRequest(0x3079, 0x2C);
	_sendRequest(0x3034, 0x2D);
	_sendRequest(0x3000, 0x2F);
	_sendRequest(0x3028, 0x30);
	_sendRequest(0x3031, 0x31);
	_sendRequest(0x30DF, 0x32);
	_sendRequest(0x3001, 0x38);
	_sendRequest(0x3078, 0x39);
	_sendRequest(0x3033, 0x3B);
	_sendRequest(0x3033, 0x3C);
	_sendRequest(0x3090, 0x48);
	_sendRequest(0x3068, 0x51);
	_sendRequest(0x3038, 0x5E);
	_sendRequest(0x3000, 0x71);
	_sendRequest(0x3008, 0x72);
	_sendRequest(0x3000, 0x77);
	_sendRequest(0x3021, 0xC0);
	_sendRequest(0x3010, 0xC1);
	_sendRequest(0x301A, 0xE4);
	_sendRequest(0x301F, 0xEA);
	_sendRequest(0x3000, 0x77);
	_sendRequest(0x3000, 0x71);
	_sendRequest(0x3000, 0x71);
	_sendRequest(0x300C, 0x76);
	
	return 1;
}


// ストリーム初期化メソッド
// -----------------------------------------------------------
int Friio::_initStream() {
	_sendRequest(0x08, 0x33);
	_sendRequest(0x40, 0x37);
	_sendRequest(0x1F, 0x3A);
	_sendRequest(0xFF, 0x3B);
	_sendRequest(0x1F, 0x3C);
	_sendRequest(0xFF, 0x3D);
	_sendRequest(0x00, 0x38);
	_sendRequest(0x00, 0x35);
	_sendRequest(0x00, 0x39);
	_sendRequest(0x00, 0x36);
	
	return 1;
}


// Friioステータス取得メソッド
// -----------------------------------------------------------
int	Friio::getStatus() {
	// ステータスをそのまま返す
	return status;
}


// チャンネル制御メソッド
// -----------------------------------------------------------
int Friio::setChannel(int channel) {
	IOReturn error;
	uint32_t chCode;
	uint8_t  sbuffer[2][5], rbuffer[2][1];
	
	// リクエスト用バッファの初期化
	memset(sbuffer, '\0', sizeof(sbuffer));
	memset(rbuffer, '\0', sizeof(rbuffer));

	// チャンネルデータ計算
	chCode = 0x0E7F + 0x002A * (uint32_t)(channel - 13);
	
	// 1回目リクエスト設定	
	sbuffer[0][0] = 0xC0;
	sbuffer[0][1] = (uint8_t)(chCode >> 8);
	sbuffer[0][2] = (uint8_t)(chCode & 0xFF);
	sbuffer[0][3] = 0xB2;
	sbuffer[0][4] = 0x08;
	
	// 2回目リクエスト設定
	sbuffer[1][0] = 0xC0;
	sbuffer[1][1] = (uint8_t)(chCode >> 8);
	sbuffer[1][2] = (uint8_t)(chCode & 0xFF);
	sbuffer[1][3] = 0x9A;
	sbuffer[1][4] = 0x50;
	
	// 3回目のリクエスト送信
	IOUSBDevRequest req1;
	req1.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
	req1.bRequest = 0x03;
	req1.wValue = 0x3000;
	req1.wIndex = 0x00FE;
	req1.wLength = sizeof(sbuffer[0]);
	req1.wLenDone = 0;
	req1.pData = sbuffer[0];
	error = (*interface)->ControlRequest(interface, 0, &req1);
	if(error) {
		printf("Unable to set the channel.\n");
		return 0;
	}
	
	// 4回目のリクエスト送信
	IOUSBDevRequest req2;
	req2.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
	req2.bRequest = 0x03;
	req2.wValue = 0x3000;
	req2.wIndex = 0x00FE;
	req2.wLength = sizeof(sbuffer[1]);
	req2.wLenDone = 0;
	req2.pData = sbuffer[1];
	error = (*interface)->ControlRequest(interface, 0, &req2);
	if(error) {
		printf("Unable to set the channel.\n");
		return 0;
	}	
	
	// 5回目のリクエスト送信
	IOUSBDevRequest req3;
	req3.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
	req3.bRequest = 0x02;
	req3.wValue = 0x3000;
	req3.wIndex = 0x00B0;
	req3.wLength = sizeof(rbuffer[0]);
	req3.wLenDone = 0;
	req3.pData = rbuffer[0];
	error = (*interface)->ControlRequest(interface, 0, &req3);
	if(error) {
		printf("Unable to set the channel.\n");
		return 0;
	}
	
	// 6回目のリクエスト送信
	IOUSBDevRequest req4;
	req4.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
	req4.bRequest = 0x02;
	req4.wValue = 0x3000;
	req4.wIndex = 0x0080;
	req4.wLength = sizeof(rbuffer[1]);
	req4.wLenDone = 0;
	req4.pData = rbuffer[1];
	error = (*interface)->ControlRequest(interface, 0, &req4);
	if(error) {
		printf("Unable to set the channel.\n");
		return 0;
	}
	
	return 1;
}


// シグナルレベル取得メソッド
// -----------------------------------------------------------
float Friio::getSignalLevel() {
	IOReturn	err;
	uint8_t		buffer[37];
	uint32_t	dwHex;
	float		signal_level = 0;
	
	// リクエストバッファ
	memset(buffer, '\0', sizeof(buffer));

	// リクエスト送信
	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
	req.bRequest = 0x02;
	req.wValue = 0x3000;
	req.wIndex = 0x0089;
	req.wLength = sizeof(buffer);
	req.wLenDone = 0;
	req.pData = buffer;
	err = (*interface)->ControlRequest(interface, 0, &req);
	if(err) {
		printf("Unable to get signal level.\n");
		return 0;
	}

	// シグナルレベルを算出
	dwHex = ((uint32_t)(buffer[2]) << 16) | ((uint32_t)(buffer[3]) << 8) | (uint32_t)(buffer[4]);
	if(!dwHex) {
		signal_level = 0.0f;
	} else {
		signal_level = (float)(68.4344993f - log10((double)dwHex) * 10.0 + pow(log10((double)dwHex) - 5.045f, (double)4.0f) / 4.09f);
	}
	
	return signal_level;
}


// 録画実行メソッド
// -----------------------------------------------------------
int Friio::rec(char *file, int sec) {
	ThreadData		tData;
	pthread_t		pt;
	int				channel, viewMode = 0;
	
	// 視聴モードの判定
	if(sec == 0) {
		viewMode = 1;
	} else {
		// LEDを赤に変更
		setLED(LED_COLOR_RED);
	}
	
	// スレッドデータの初期化
	tData.interface = interface;
	tData.flag = 1;
	tData.buffer = new StreamBuffer;

	// b25とB-CASインスタンスをコピー
	tData.b25 = b25;
	tData.bcas = bcas;

	// 出力先のファイルデスクリプタを取得
	tData.fd = fopen(file, "w");
	if(tData.fd == NULL) {
		printf("Unable to open the output file.\n");
		return 0;
	}
			
	// スレッドを起動
	pthread_create(&pt, NULL, &_friioWorker, &tData);
	
	if(viewMode) {		
		// 視聴モード
		while(1) {
			// チャンネル入力
			printf("Channel: ");
			scanf("%d", &channel);
			
			// チャンネルを変更
			setChannel(channel);
			sleep(1);
			
			// シグナルを取得して表示
			getSignalLevel();
			float signal = getSignalLevel();
			printf("Singal: %f\n", signal);
		}
	} else {
		// 録画モード
		printf("Recording");
		for(int i=1; i<=sec; i++) {
			printf(".");
			fflush(stdout);
			sleep(1);
		}
		printf("done.\n");
		tData.flag = 0;
	}
	
	// スレッドを停止
	pthread_join(pt, NULL);
	
	// バッファとファイルデスクリプタのあとかたづけ
	fflush(tData.fd);
	fclose(tData.fd);
	delete(tData.buffer);
	
	return 1;
}


// ストリームデータを取得するWoker
// -----------------------------------------------------------
void* _friioWorker(void *arg) {
	ThreadData*			tData = (ThreadData *)arg;
    CFRunLoopSourceRef	cfSource;
	IOReturn			error;

	// 非同期イベントソースの生成
	error = (*tData->interface)->CreateInterfaceAsyncEventSource(tData->interface, &cfSource);
	if(error) {
		printf("Unable to create event source.\n");
		return NULL;
    }
	// イベントソースを登録
	CFRunLoopAddSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
	
	// おまじない
	(*tData->interface)->SetAlternateInterface(tData->interface, 1);
	(*tData->interface)->SetAlternateInterface(tData->interface, 0);

	// パイプ初期化
	error = (*tData->interface)->ClearPipeStall(tData->interface, 1);
	if(error) {
		printf("Could not clear the pipe.\n");
	}

	// 非同期Bulk転送リクエストを2回発行する
	for(int i=0; i<8; i++) {
		error = (*tData->interface)->ReadPipeAsync(
			tData->interface, 1,
			tData->buffer->getCurrentBuffer(),
			BUFFER_SIZE, 
			(IOAsyncCallback1)_readCallback,
			tData
		);
		// エラー処理
		if(error) {
			printf("Read pipe error(%x).\n", error);
			// 転送失敗時の後始末
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
			return NULL;
		}
	}
	
	// イベントループを実行
	CFRunLoopRun();
	// イベントループの後処理
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
	
	return NULL;
}


// パイプ読み出しコールバック
// -----------------------------------------------------------
void _readCallback(ThreadData *tData, IOReturn result, void *arg) {
	IOReturn error;
	ARIB_STD_B25_BUFFER sBuffer, dBuffer;

	// コールバック結果のチェック
	if(result != kIOReturnSuccess) {
		printf("Callback failed(%x).\n", result);
		CFRunLoopStop(CFRunLoopGetCurrent());
		return;
	}

	// 非同期Bulk転送を再実行
	error = (*tData->interface)->ReadPipeAsync(
		tData->interface, 1,
		tData->buffer->getCurrentBuffer(),
		BUFFER_SIZE,
		(IOAsyncCallback1)_readCallback,
		tData
	);
	// エラー処理
	if(error) {
		printf("Read pipe error(%x).\n");
		CFRunLoopStop(CFRunLoopGetCurrent());
		return;
	}

	// 復号用データの生成
	sBuffer.data = tData->buffer->getDataBuffer();
	sBuffer.size = BUFFER_SIZE;	

	// b25でMULTI2を復号
	tData->b25->put(tData->b25, &sBuffer);
	tData->b25->get(tData->b25, &dBuffer);
/*	
	for(int i=0; i<dBuffer.size; i++) {
		printf("%x", dBuffer.data[i]);
	}
	printf("\n------------------\n");
	printf("Size : %d\n", dBuffer.size);
*/
	// ファイルに書き込み
	if(dBuffer.size > 0) {
		fwrite(dBuffer.data, 1, dBuffer.size, tData->fd);
	}
	
	// 終了処理
	if(!tData->flag) {
		CFRunLoopStop(CFRunLoopGetCurrent());
	}
}


// LED制御メソッド
// -----------------------------------------------------------
void Friio::setLED(int color) {
	_sendRequest(0x000F, 0x06);
	switch(color) {
		// 紫
		case 0:
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0009, 0x00);
			_sendRequest(0x000D, 0x00);
			break;
		// 緑
		case 1:
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0001, 0x00);
			_sendRequest(0x0005, 0x00);
			break;
		// 赤
		case 2:
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x000B, 0x00);
			_sendRequest(0x000F, 0x00);
			_sendRequest(0x0003, 0x00);
			_sendRequest(0x0007, 0x00);
			_sendRequest(0x0001, 0x00);
			_sendRequest(0x0005, 0x00);
			break;
		// 白
		case 3:
			// 情報なし
			break;
	}
	_sendRequest(0x0000, 0x06);
}
