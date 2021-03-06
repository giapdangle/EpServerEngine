/*! 
SyncUdpSocket for the EpServerEngine

The MIT License (MIT)

Copyright (c) 2012-2013 Woong Gyu La <juhgiyo@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "epSyncUdpSocket.h"
#include "epSyncUdpServer.h"

#if defined(_DEBUG) && defined(EP_ENABLE_CRTDBG)
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif // defined(_DEBUG) && defined(EP_ENABLE_CRTDBG)

using namespace epse;
SyncUdpSocket::SyncUdpSocket(ServerCallbackInterface *callBackObj,unsigned int waitTimeMilliSec,epl::LockPolicy lockPolicyType): BaseUdpSocket(callBackObj,waitTimeMilliSec,lockPolicyType)
{
	m_packetReceivedEvent=EventEx(false,false);
	m_isConnected=true;
}

SyncUdpSocket::~SyncUdpSocket()
{
	KillConnection();
}

bool SyncUdpSocket::IsConnectionAlive() const
{
	return m_isConnected;
}

void SyncUdpSocket::KillConnection()
{
	epl::LockObj lock(m_baseSocketLock);
	if(!IsConnectionAlive())
	{
		return;
	}
	m_isConnected=false;

	m_listLock->Lock();
	Packet *removeElem=NULL;
	while(!m_packetList.empty())
	{
		removeElem=m_packetList.front();
		if(removeElem)
			removeElem->ReleaseObj();
		m_packetList.pop();
	}
	m_listLock->Unlock();

	removeSelfFromContainer();
	m_callBackObj->OnDisconnect(this);
}


void SyncUdpSocket::killConnection()
{
	if(IsConnectionAlive())
	{
		m_isConnected=false;

		m_listLock->Lock();
		Packet *removeElem=NULL;
		while(!m_packetList.empty())
		{
			removeElem=m_packetList.front();
			if(removeElem)
				removeElem->ReleaseObj();
			m_packetList.pop();
		}
		m_listLock->Unlock();


		removeSelfFromContainer();
		m_callBackObj->OnDisconnect(this);

	}
}

void SyncUdpSocket::addPacket(Packet *packet)
{
	if(packet)
		packet->RetainObj();
	epl::LockObj lock(m_listLock);
	m_packetList.push(packet);
	m_packetReceivedEvent.SetEvent();
}

int SyncUdpSocket::Send(const Packet &packet, unsigned int waitTimeInMilliSec,SendStatus *sendStatus)
{
	epl::LockObj lock(m_baseSocketLock);
	return BaseUdpSocket::Send(packet,waitTimeInMilliSec,sendStatus);
}

Packet *SyncUdpSocket::Receive(unsigned int waitTimeInMilliSec,ReceiveStatus *retStatus)
{
	epl::LockObj lock(m_baseSocketLock);
	if(!IsConnectionAlive())
	{
		if(retStatus)
			*retStatus=RECEIVE_STATUS_FAIL_NOT_CONNECTED;
		return NULL;
	}

	// receive routine
	unsigned int packetSize=0;

	m_listLock->Lock();
	if(m_packetList.size()==0)
	{
		m_listLock->Unlock();
		if(!m_packetReceivedEvent.WaitForEvent(waitTimeInMilliSec))
		{
			if(retStatus)
				*retStatus=RECEIVE_STATUS_FAIL_TIME_OUT;
			return NULL;
		}

	}
	Packet *packet= m_packetList.front();
	packetSize=packet->GetPacketByteSize();
	if(packetSize==0)
	{
		m_listLock->Unlock();
		killConnection();
		if(retStatus)
			*retStatus=RECEIVE_STATUS_FAIL_CONNECTION_CLOSING;
		return NULL;
	}
	m_packetList.pop();
	m_listLock->Unlock();
	if(retStatus)
		*retStatus=RECEIVE_STATUS_SUCCESS;
	return packet;
}

void SyncUdpSocket::execute()
{
	m_callBackObj->OnNewConnection(this);
}

