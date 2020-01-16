//------------------------------------------------------------------------------
//
//	Copyright (C) 2017 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __CNX_BASE_H__
#define __CNX_BASE_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pthread.h>


//------------------------------------------------------------------------------
//
//	CNX_Mutex
//
class CNX_Mutex
{
public:
	CNX_Mutex()
	{
		pthread_mutex_init( &m_hLock, NULL );
	}

	~CNX_Mutex()
	{
		pthread_mutex_destroy( &m_hLock );
	}

public:
	void Lock()
	{
		pthread_mutex_lock( &m_hLock );
	}

	void Unlock()
	{
		pthread_mutex_unlock( &m_hLock );
	}

private:
	pthread_mutex_t m_hLock;

private:
	CNX_Mutex (const CNX_Mutex &Ref);
	CNX_Mutex &operator=(const CNX_Mutex &Ref);
};


//------------------------------------------------------------------------------
//
//	CNX_AutoLock
//
class CNX_BaseAutoLock
{
public:
	CNX_BaseAutoLock( CNX_Mutex *pLock )
		: m_pLock( pLock )
	{
		m_pLock->Lock();
	}

	~CNX_BaseAutoLock()
	{
		m_pLock->Unlock();
	}

private:
	CNX_Mutex*	m_pLock;

private:
	CNX_BaseAutoLock (const CNX_BaseAutoLock &Ref);
	CNX_BaseAutoLock &operator=(const CNX_BaseAutoLock &Ref);
};


//------------------------------------------------------------------------------
//
//	CNX_Thread
//
class CNX_Thread
{
public:
	CNX_Thread()
		: m_bThreadRun( false )
		, m_hThread( 0x00 )
	{

	}

	virtual ~CNX_Thread()
	{
		Stop();
	}

public:
	virtual void ThreadProc() = 0;
	virtual int32_t Start()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		if( m_bThreadRun == false )
		{
			m_bThreadRun = true;
			if( 0 != pthread_create( &m_hThread, NULL, CNX_Thread::ThreadStub, this) )
				return -1;
		}
		return 0;
	}

	virtual int32_t Stop()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		if( m_bThreadRun == true )
		{
			m_bThreadRun = false;

			pthread_join( m_hThread, NULL );
			m_hThread = 0x00;
		}
		return 0;
	}

protected:
	int32_t		m_bThreadRun;

private:
	static void* ThreadStub( void *pObj )
	{
		if( NULL != pObj ) {
			((CNX_Thread*)pObj)->ThreadProc();
		}
		return (void*)0xDEADDEAD;
	}

private:
	CNX_Mutex	m_hLock;
	pthread_t	m_hThread;

private:
	CNX_Thread (const CNX_Thread &Ref);
	CNX_Thread &operator=(const CNX_Thread &Ref);
};


//------------------------------------------------------------------------------
//
//	CNX_Queue
//
class CNX_Queue
{
public:
	CNX_Queue( int32_t iMaxCount )
		: m_iHead( 0 )
		, m_iTail( 0 )
		, m_iCount( 0 )
		, m_iMaxCount( iMaxCount )
	{
		m_ppQueuePool = (void**)malloc( m_iMaxCount );
	}

	~CNX_Queue()
	{
		if( m_ppQueuePool )
			free( m_ppQueuePool );
	}

public:
	int32_t Push( void *pData )
	{
		CNX_BaseAutoLock lock( &m_hLock );
		if( m_iMaxCount <= m_iCount )
			return -1;

		m_ppQueuePool[m_iTail] = pData;
		m_iTail = (m_iTail + 1) % m_iMaxCount;
		m_iCount++;
		return 0;
	}

	int32_t Pop( void **ppData )
	{
		CNX_BaseAutoLock lock( &m_hLock );
		*ppData = NULL;

		if( 0 >= m_iCount )
			return -1;

		*ppData = m_ppQueuePool[m_iHead];
		m_iHead = (m_iHead + 1) % m_iMaxCount;
		m_iCount--;
		return 0;
	}

	void Reset()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		m_iHead  = 0;
		m_iTail  = 0;
		m_iCount = 0;
	}

	int32_t GetCount()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		return m_iCount;
	}

	void Print( int32_t iIndex )
	{
		CNX_BaseAutoLock lock( &m_hLock );

		if( m_iCount == 0 || iIndex >= m_iCount )
			return;

		printf( "[ %d ] iHead( %d ) / iTail( %d ) / pData( %p ) \n",
			iIndex, m_iHead, m_iTail, m_ppQueuePool[m_iHead+iIndex % m_iMaxCount] );
	}

	void PrintAll()
	{
		CNX_BaseAutoLock lock( &m_hLock );

		if( m_iCount == 0 )
			return;

		int32_t iStartPos = m_iHead;
		int32_t iEndPos   = (m_iHead > m_iTail) ? m_iCount + m_iTail : m_iTail;
		int32_t iCount    = 0;

		for( int32_t i = iStartPos; i < iEndPos; i++ )
		{
			printf( "[ %d ] iHead( %d ) / iTail( %d ) / pData( %p ) \n",
				iCount++, m_iHead, m_iTail, m_ppQueuePool[i % m_iMaxCount] );
		}
	}

private:
	CNX_Mutex	m_hLock;
	void**		m_ppQueuePool;
	int32_t		m_iHead, m_iTail, m_iCount;
	int32_t		m_iMaxCount;

private:
	CNX_Queue (const CNX_Queue &Ref);
	CNX_Queue &operator=(const CNX_Queue &Ref);
};


//------------------------------------------------------------------------------
//
//	CNX_LinkedList
//
typedef struct NX_NODE	{
	void	*pData;
	NX_NODE	*pNext;
} NX_NODE;

class CNX_LinkedList
{
public:
	CNX_LinkedList()
		: m_pHead( NULL )
		, m_iCount( 0 )
	{
		m_pHead = (NX_NODE*)malloc( sizeof(NX_NODE) );
		m_pHead->pData = NULL;
		m_pHead->pNext = NULL;
	}

	~CNX_LinkedList()
	{
		if( m_pHead )
		{
			free( m_pHead );
			m_pHead = NULL;
		}
	}

public:
	enum { NODE_FRONT = 0, NODE_REAR = -1 };

	int32_t Add( void *pData, int32_t iIndex = NODE_REAR )
	{
		CNX_BaseAutoLock lock( &m_hLock );

		int32_t i = 0;
		int32_t iPos = (iIndex == NODE_REAR) ? m_iCount : iIndex;

		if( iPos < 0 || iPos > m_iCount )
			return -1;

		NX_NODE *pNode = m_pHead;
		while( (pNode != NULL) && (++i <= iPos) )
			pNode = pNode->pNext;

		NX_NODE *pNewNode = (NX_NODE*)malloc( sizeof(NX_NODE) );
		if( pNewNode == NULL )
			return -1;

		pNewNode->pData = pData;
		pNewNode->pNext = (pNode->pNext != NULL) ? pNode->pNext : NULL;
		pNode->pNext	= pNewNode;

		m_iCount++;
		return 0;
	}

	int32_t Remove( int32_t iIndex = NODE_FRONT )
	{
		CNX_BaseAutoLock lock( &m_hLock );

		int32_t i = 0;
		int32_t iPos = (iIndex == NODE_REAR) ? m_iCount-1 : iIndex;

		if( iPos < 0 || iPos >= m_iCount )
			return -1;

		NX_NODE *pNode = m_pHead;
		while( (pNode != NULL) && (++i <= iPos) )
			pNode = pNode->pNext;

		NX_NODE *pTmpNode = (pNode->pNext->pNext != NULL) ? pNode->pNext->pNext : NULL;
		free( pNode->pNext );
		pNode->pNext = pTmpNode;

		m_iCount--;
		return 0;
	}

	int32_t RemoveAll()
	{
		CNX_BaseAutoLock lock( &m_hLock );

		while( m_iCount > 0 )
		{
			int32_t i = 0;
			int32_t iPos = m_iCount - 1;
			NX_NODE *pNode = m_pHead;
			while( (pNode != NULL) && (++i <= iPos) )
				pNode = pNode->pNext;

			NX_NODE *pTmpNode = (pNode->pNext->pNext != NULL) ? pNode->pNext->pNext : NULL;
			free( pNode->pNext );
			pNode->pNext = pTmpNode;

			m_iCount--;
		}

		return 0;
	}

	int32_t	Search( void **ppData, int32_t iIndex )
	{
		CNX_BaseAutoLock lock( &m_hLock );

		int32_t i = 0;
		int32_t iPos = (iIndex == NODE_REAR) ? m_iCount-1 : iIndex;

		NX_NODE *pNode = (NX_NODE*)m_pHead;

		*ppData = NULL;
		if( iPos >= m_iCount )
			return -1;

		while( (pNode != NULL) && (++i <= iPos) )
			pNode = pNode->pNext;

		*ppData = pNode->pNext->pData;
		return 0;
	}

	int32_t Swap( int32_t iIndex1, int32_t iIndex2 )
	{
		CNX_BaseAutoLock lock( &m_hLock );

		int32_t i = 0, j = 0;
		int32_t iPos1 = (iIndex1 == NODE_REAR) ? m_iCount : iIndex1;
		int32_t iPos2 = (iIndex2 == NODE_REAR) ? m_iCount : iIndex2;

		if( (iPos1 == iPos2) ||
			(iPos1 < 0 || iPos1 >= m_iCount) ||
			(iPos2 < 0 || iPos2 >= m_iCount) )
		{
			return -1;
		}

		if( iPos1 > iPos2 ) {
			int32_t iTemp = iPos1;
			iPos1 = iPos2;
			iPos2 = iTemp;
		}

		NX_NODE *pCurNode1 = m_pHead;
		while( (pCurNode1 != NULL) && (++i <= iPos1) )
			pCurNode1 = pCurNode1->pNext;

		NX_NODE *pCurNode2 = m_pHead;
		while( (pCurNode2 != NULL) && (++j <= iPos2) )
			pCurNode2 = pCurNode2->pNext;

		pCurNode1 = pCurNode1->pNext;
		pCurNode2 = pCurNode2->pNext;

		void *pTmpData = pCurNode2->pData;
		pCurNode2->pData = pCurNode1->pData;
		pCurNode1->pData = pTmpData;

		return 0;
	}

	int32_t GetCount()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		return m_iCount;
	}

	void Print( int32_t iIndex )
	{
		CNX_BaseAutoLock lock( &m_hLock );
		if( m_iCount == 0 || iIndex >= m_iCount )
			return;

		NX_NODE *pNode = m_pHead;
		for( int32_t i = 0; i < iIndex + 1; i++ )
		{
			pNode = pNode->pNext;
		}

		printf( "[ %d ] curNode( %p ) / nextNode( %p ) / pData( %p ) \n", iIndex, pNode, pNode->pNext, pNode->pData );
	}

	void PrintAll()
	{
		CNX_BaseAutoLock lock( &m_hLock );
		// if( m_iCount == 0 )
		// 	return;

		NX_NODE *pNode = m_pHead;
		printf( "------------------------------------------------------------------------------\n" );
		printf( "[ HEAD ] this( %p ) : curNode( %p ) / nextNode( %p )\n", this, pNode, pNode->pNext );
		printf( "------------------------------------------------------------------------------\n" );

		for( int32_t i = 0; i < m_iCount; i++ )
		{
			pNode = pNode->pNext;
			printf( "[ %d ] curNode( %p ) / nextNode( %p ) / pData( %p ) \n", i, pNode, pNode->pNext, pNode->pData );
		}
	}

private:
	CNX_Mutex	m_hLock;
	NX_NODE*	m_pHead;

	int32_t		m_iCount;

private:
	CNX_LinkedList (const CNX_LinkedList &Ref);
	CNX_LinkedList &operator=(const CNX_LinkedList &Ref);
};


//------------------------------------------------------------------------------
//
//	CNX_Semaphore
//
class CNX_Semaphore
{
public:
	CNX_Semaphore()
		: m_iInit( 0 )
		, m_iValue( 0 )
		, m_bReset( false )
	{
		pthread_cond_init ( &m_hCond,  NULL );
		pthread_mutex_init( &m_hMutex, NULL );
	}

	~CNX_Semaphore()
	{
		Reset();
		pthread_cond_destroy( &m_hCond );
		pthread_mutex_destroy( &m_hMutex );
	}

public:
	void Init()
	{
		pthread_mutex_lock( &m_hMutex );
		m_iValue = m_iInit;
		m_bReset = false;
		pthread_mutex_unlock( &m_hMutex );
	}

	void Reset()
	{
		pthread_mutex_lock( &m_hMutex );
		m_bReset = true;
		for( int32_t i = 0; i < m_iValue; i++ )
			pthread_cond_signal( &m_hCond );
		m_iValue = m_iInit;
		pthread_mutex_unlock( &m_hMutex );
	}

	int32_t Post()
	{
		int32_t iRet = 0;
		pthread_mutex_lock( &m_hMutex );
		m_iValue++;
		pthread_cond_signal ( &m_hCond );
		if( m_bReset || m_iValue <= 0 ) {
			iRet = -1;
		}
		pthread_mutex_unlock( &m_hMutex );
		return iRet;
	}

	int32_t Pend()
	{
		int32_t iRet = 0;
		pthread_mutex_lock( &m_hMutex );
		if( m_iValue == 0 && !m_bReset ) {
			iRet = pthread_cond_wait( &m_hCond, &m_hMutex );
			if( m_bReset )	iRet = -1;
			else 			m_iValue--;
		}
		else if( m_iValue < 0 || m_bReset ) {
			iRet = -1;
		}
		else {
			m_iValue--;
		}
		pthread_mutex_unlock( &m_hMutex );
		return iRet;
	}

	int32_t GetCount()
	{
		int32_t iValue = 0;
		pthread_mutex_lock( &m_hMutex );
		iValue = m_iValue;
		pthread_mutex_unlock( &m_hMutex );
		return iValue;
	}

	void Print()
	{
		int32_t iInit = 0, iValue = 0, bReset = 0;

		pthread_mutex_lock( &m_hMutex );
		iInit  = m_iInit;
		iValue = m_iValue;
		bReset = m_bReset;
		pthread_mutex_unlock( &m_hMutex );

		printf("init( %d ), value( %d ), reset( %d )\n", iInit, iValue, bReset);
	}

private:
	pthread_cond_t m_hCond;
	pthread_mutex_t m_hMutex;

	int32_t			m_iInit;
	int32_t			m_iValue;
	int32_t			m_bReset;

private:
	CNX_Semaphore (const CNX_Semaphore &Ref);
	CNX_Semaphore &operator=(const CNX_Semaphore &Ref);
};

#endif	// __CNX_BASE_H__
