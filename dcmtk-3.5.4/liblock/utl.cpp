#include <windows.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <regex>
#include "lock.h"
#include "liblock.h"

using namespace std;

extern "C" unsigned int getLockNumber(const char *filter, const char *regxPattern, int isDirectory, char *filenamebuf)
{
	WIN32_FIND_DATA ffd;
	unsigned int lockNumber = 0;

	HANDLE hFind = FindFirstFile(filter, &ffd);
	if (INVALID_HANDLE_VALUE == hFind) 
	{
		cerr << TEXT("FindFirstFile Error in ") << filter << endl;
		return -3;
	}
	
	regex pattern(TEXT(regxPattern));
	match_results<string::const_iterator> result;
	// List all the files in the directory with some info about them.
	do
	{
		if (isDirectory ? ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY : true)
		{
			string fileName(ffd.cFileName);
			if(regex_match(fileName, result, pattern))
			{
				char buffer[MAX_PATH];
				result[1].str().copy(buffer, result[1].length(), 0);
				buffer[result[1].length()] = '\0';
				sscanf_s(buffer, TEXT("%d"), &lockNumber);
				if(filenamebuf)
				{
					fileName.copy(filenamebuf, fileName.length(), 0);
					filenamebuf[fileName.length()] = '\0';
				}
				break;
			}
		}
	} while (FindNextFile(hFind, &ffd) != 0);
	FindClose(hFind);
	return lockNumber;
}

extern "C" void mkpasswd(const char *base64, unsigned int salt, char *lock_passwd)
{
	ostringstream saltBase64;
	saltBase64 << hex << setw(8) << setfill('0') << Lock32_Function(salt);
	string hash(md5crypt(base64, "1", saltBase64.str().c_str()));
	hash.copy(lock_passwd, 8, hash.length() - 8);
	lock_passwd[8] = '\0';
}

extern "C" int loadPublicKeyContent(const char* publicKey, SEED_SIV *siv, unsigned int lockNumber, char *gen_passwd)
{
	ifstream keystrm(publicKey);
	if(keystrm.fail()) return -2;
	ostringstream contentBase64;
	bool startTag = false, endTag = false;
	char buffer[82];
	while(!endTag)
	{
		keystrm.getline(buffer, sizeof(buffer));
		if(keystrm.fail()) break;
		if(buffer[0] == '-' && buffer[1] == '-')
		{
			if(startTag)
				endTag = true;
			else
				startTag = true;
		}
		else if(startTag && !endTag)
			contentBase64 << buffer << endl;
	}
	string base64(contentBase64.str());
	char *data = new char[base64.size() + 1];
	base64.copy(data, base64.size());
	data[base64.size()] = '\0';
	mkpasswd(data, lockNumber, gen_passwd);

	int read = fillSeedSIV(siv, sizeof(SEED_SIV), data, base64.size(), PUBKEY_SKIP + (lockNumber % PUBKEY_MOD));
	delete data;
	if(endTag && read == sizeof(SEED_SIV))
		return 0;
	else
		return -1;
}

extern "C" int invalidLock(const char *licenseRSAEnc, const char *rsaPublicKey, SEED_SIV *sivptr)
{
	unsigned char inBuf[KEY_SIZE / 8], midBuf[KEY_SIZE / 8], outBuf[KEY_SIZE / 8];
	ifstream licenseRSAStream(licenseRSAEnc, ios_base::binary);
	licenseRSAStream.read((char*)inBuf, KEY_SIZE / 8);
	if(licenseRSAStream.fail())
	{
		licenseRSAStream.close();
		return -10;
	}
	licenseRSAStream.close();

	int ret = rsaVerify(inBuf, KEY_SIZE / 8, midBuf, rsaPublicKey);
	if(ret <= 0) return -11;

	// skip magic number and salt
	ret = aes256cbc_dec(midBuf + AES_OFFSET, ret - AES_OFFSET, outBuf, sivptr->key, sivptr->iv);
	if(ret <= 0) return -12;

	DWORD digestSig[4], *originSig = reinterpret_cast<DWORD*>(&outBuf[DICTIONARY_SIZE * 8]);
	MD5_digest(outBuf, DICTIONARY_SIZE * 8, reinterpret_cast<unsigned char*>(digestSig));
	if(digestSig[0] != originSig[0] || digestSig[1] != originSig[1]
		|| digestSig[2] != originSig[2] || digestSig[3] != originSig[3])
		return -13;

	time_t t = time( NULL );
	struct tm tmp;
	localtime_s( &tmp, &t );
	int i = tmp.tm_yday % DICTIONARY_SIZE;
	DWORD *dict = reinterpret_cast<DWORD*>(outBuf);
	if(dict[i] == dict[DICTIONARY_SIZE + i] ^ Lock32_Function(dict[i]))
		return 0;
	else
		return -14;
}

extern "C" int currentCount(char *passwd)
{
	int ret;
	WORD data[4] = { 0, 0, 0, 0 };
	ret = ReadLock(15, reinterpret_cast<unsigned char*>(data), passwd);
	if(ret == 0)
		return data[3];
	else
		return -15;
}

extern "C" int decreaseCount(char *passwd)
{
	int ret;
	WORD data[4] = { 0, 0, 0, 0 };
	ret = ReadLock(15, reinterpret_cast<unsigned char*>(data), passwd);
	if(ret == 0)
	{
		if(data[3] > 0) --data[3];
		WriteLock(15, reinterpret_cast<unsigned char*>(data), passwd);
		return data[3];
	}
	else
		return -15;
}