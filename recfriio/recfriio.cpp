#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "friio/friio.h"

// 関数宣言
// -----------------------------------------------------------
void usage();


// メイン関数
// -----------------------------------------------------------
int main(int argc, char* argv[]) {
	int channel, rec_sec, status;
	char opt, file[256];
	
	// チャンネルの初期化
	channel = 0;
	rec_sec = 0;

	// オプションの解析
	while((opt = getopt(argc, argv, "c:s:f:")) != -1) {
		switch(opt) {
			// チャンネル
			case 'c':
				channel = atoi(optarg);
				break;
			// 録画時間
			case 's':
				rec_sec = atoi(optarg);
				break;
			// 出力ファイル
			case 'f':
				strcpy(file, optarg);
				break;
			// それ以外
			case '?':
				usage();
				break;
		}
	}
	
	// チャンネルのチェック
	if(channel == 0) {
		usage();
	}

	// Friioインスタンスの生成
	Friio friio;
	
	// ステータスのチェック
	status = friio.getStatus();
	if(status) {
		exit(1);
	}
	
	// チャンネルを変更
	friio.setChannel(channel);
	printf("Channel: %d\n", channel);
	sleep(1);
	
	// シグナルのチェック
	for(int i=1; i<=3; i++) {
		// シグナルを取得
		float signal = friio.getSignalLevel();
		printf("Singal: %f\n", signal);
		
		// シグナルの判定
		if(signal >= 10) {
			break;
		} else if(i==3) {
			printf("Signal level is insufficient.\n");
			exit(1);
		}
	}
	
	// 録画を実行
	friio.rec(file, rec_sec);
	
	return 1;
}


// 使用方法表示関数
// -----------------------------------------------------------
void usage() {
	printf("Usage: recfriio -c 25 -s 60 -f output.ts\n");
	printf("Option:\n");
	printf("     -c : Channel number.\n");
	printf("     -s : Recording time (second).\n");
	printf("     -f : Output file path.\n");
	exit(0);
}


