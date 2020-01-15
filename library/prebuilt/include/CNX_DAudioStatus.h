#ifndef CNX_DAUDIOSTATUS_H
#define CNX_DAUDIOSTATUS_H

// sqlite3
#include <sqlite3.h>
// alsa
#include <alsa/asoundlib.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <vector>
#include <string>
using namespace std;

#define DEFAULT_DAUDIO_STATUS_DATABASE			"daudio.status.db"
#define DEFAULT_DAUDIO_STATUS_DATABASE_TABLE	"DAUDIO_STATUS"

class CNX_DAudioStatus
{
public:
	CNX_DAudioStatus(string database = DEFAULT_DAUDIO_STATUS_DATABASE);

	~CNX_DAudioStatus();

	int32_t SetBTConnection(int32_t value);

	int32_t GetBTConnection();

	int32_t SetVolume(int32_t value);

	int32_t GetVolume();

private:
	int32_t OpenDataBase();

	int32_t CloseDataBase();

	int32_t IsOpenedDataBase();

	int32_t CreateTable();

	int32_t InsertTuple();

	int32_t RowCount();

	int32_t IsExistByID(int32_t id);

	int32_t SetSystemVolume(int32_t percentage);

	static void *ThreadStub(void *pObj);

	void ThreadProc();

private:
	sqlite3* m_pHandle;

	string m_DataBasePath;

	string m_SoundCard;

	string m_CardNumber;

	int m_iMaxVolume;

	pthread_t m_hInitThread;
};

#endif // CNX_DAUDIOSTATUS_H
