#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

// バッファ用定数
//static const int BUFFER_LENGTH = 4096;
static const int BUFFER_LENGTH = 16;
//static const int BUFFER_SIZE = 512;
static const int BUFFER_SIZE = 327680;


// StreamBufferクラス
// -----------------------------------------------
class StreamBuffer {
private:
	int		current_num;
	int		data_num;
	uint8_t buffer[BUFFER_LENGTH][BUFFER_SIZE];
	
public:
	StreamBuffer();
		
	uint8_t* getCurrentBuffer();
	uint8_t* getDataBuffer();
	void clearBuffer();
};