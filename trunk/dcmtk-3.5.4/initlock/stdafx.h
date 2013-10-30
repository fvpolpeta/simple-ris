// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 某些 CString 构造函数将是显式的

#include <atlbase.h>
#include <atlstr.h>

// TODO: 在此处引用程序需要的其他头文件
#include <direct.h>
#include <strsafe.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <regex>
#include <algorithm>  
#include <numeric>
#include "lock.h"

extern "C"
{
	char *md5crypt(const char *passwd, const char *magic, const char *salt);
	int genrsa(int num, char *privateKey, char *publicKey, char *pass);
	int rsaSignVerify(char *infile, char *outfile, char *keyfile, int keyType, char *pass);
	int aes256cbc_enc(char *outf, unsigned char *pass, size_t pass_length);
	int aes256cbc_dec(char *inf, unsigned char *pass, size_t pass_length);
	int fillSeedSIV(void *siv, size_t sivSize, void *content, size_t contentLength, size_t start);
	void base64test();
}

#define COUT cout			//wcout
#define CERR cerr			//wcerr
#define String string		//wstring
#define REGEX regex			//wregex
#define SCANF_S sscanf_s	//swscanf_s
#define IFSTREAM ifstream   //wifstream
