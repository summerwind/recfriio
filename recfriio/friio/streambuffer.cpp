#include "StreamBuffer.h"

// コンストラクタ
// -----------------------------------------------------------
StreamBuffer::StreamBuffer() {
	// カウンタ変数の初期化
	current_num = 0;
	data_num = 0;
	
	// バッファの初期化
	for(int i=0; i<BUFFER_LENGTH; i++) {
		memset(buffer[i], '\0', sizeof(buffer[i]));
	}
}


// getCurrentBufferメソッド
// -----------------------------------------------------------
uint8_t* StreamBuffer::getCurrentBuffer() {
	//printf("Current Buffer : %d\n", current_num);

	// カウンタを1増やす
	current_num++;
	// カウンタが長さを超えたら初期化
	if(current_num >= BUFFER_LENGTH) {
		current_num = 0;
	}
	
	// バッファの初期化
	memset(buffer[current_num], '\0', sizeof(buffer[current_num]));
	return buffer[current_num];
}


// getDataBufferメソッド
// -----------------------------------------------------------
uint8_t* StreamBuffer::getDataBuffer() {
	//printf("Data Buffer : %d\n", data_num);

	// カウンタを1増やす
	data_num++;
	// カウンタが長さを超えたら初期化
	if(data_num >= BUFFER_LENGTH) {
		data_num = 0;
	}
	
	return buffer[data_num];
}


// getDataBufferメソッド
// -----------------------------------------------------------
void StreamBuffer::clearBuffer() {
	//printf("Data Buffer : %d\n", data_num);

	// バッファの初期化
	for(int i=0; i<BUFFER_LENGTH; i++) {
		memset(buffer[i], '\0', sizeof(buffer[i]));
	}
}



