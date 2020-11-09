#ifndef _SELECTTCPClient_HPP_
#define _SELECTTCPClient_HPP_

#include <iostream>

#ifdef _WIN32
	#include <WinSock2.h>
	#include <Windows.h>
	#include "../Pack.hpp"
#else//Linux
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <string.h>
	#include "Pack.hpp"
	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif 

#define CLIENT_ERROR -1
#define CLIENT_SUCCESS 1

#define CLIENT_DISCONNECT -1

#define RECV_BUF_SIZE 40960
#define MSG_BUF_SIZE 409600

class TCPClient
{
private:
	SOCKET csock;
	sockaddr_in ssin;
	char recvBuf[RECV_BUF_SIZE] = {};
	char msgBuf[MSG_BUF_SIZE] = {};
	int lastBufPos = 0;
public:
	void setSsin(const char* ip, unsigned short port)
	{
		ssin = {};
		ssin.sin_family = AF_INET;
		ssin.sin_port = htons(port);
#ifdef _WIN32
		ssin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		ssin.sin_addr.s_addr = inet_addr(ip);
#endif 
	}
public:
	TCPClient()
	{
		csock = INVALID_SOCKET;
		ssin = {};
	}
	TCPClient(const char* ip, unsigned short port)
	{
		csock = INVALID_SOCKET;
		ssin = {};
		ssin.sin_family = AF_INET;
		ssin.sin_port = htons(port);
#ifdef _WIN32
		ssin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		ssin.sin_addr.s_addr = inet_addr(ip);
#endif 
	}
	~TCPClient()
	{
		terminal();
	}

	int initSocket()
	{
#ifdef _WIN32
		WORD version = MAKEWORD(2, 2);
		WSADATA data;
		if (SOCKET_ERROR == WSAStartup(version, &data))
		{
			std::cout << "��ʼ��Winsock����ʧ��" << std::endl;
		}
		else
		{
			std::cout << "�ɹ���ʼ��Winsock������" << std::endl;
		}
#endif 
		if (INVALID_SOCKET != csock)
		{
			std::cout << "�رվ�����" << std::endl;
			terminal();
		}
		csock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == csock)
		{
			std::cout << "��ʼ���ͻ���ʧ��" << std::endl;
			return CLIENT_ERROR;
		}
		else
		{
			std::cout << "�ͻ��˳�ʼ���ɹ���" << std::endl;
		}
		return CLIENT_SUCCESS;
	}

	int connectServer()
	{
		if (INVALID_SOCKET == csock)
		{
			std::cout << "�׽���δ��ʼ������Ч" << std::endl;
			return CLIENT_ERROR;
		}
		int res = connect(csock, (sockaddr*)&ssin, sizeof(ssin));
		if (SOCKET_ERROR == res)
		{
			std::cout << "�޷����ӵ�������" << std::endl;
			return CLIENT_ERROR;
		}
		else
		{
			std::cout << "�ɹ����ӵ���������" << std::endl;
		}
		return CLIENT_SUCCESS;
	}

	void terminal()
	{
		if (INVALID_SOCKET == csock)return;
#ifdef _WIN32
		closesocket(csock);
#else //Linux
		close(csock);
#endif 
		csock = INVALID_SOCKET;
#ifdef _WIN32
		WSACleanup();
#endif 
	}

	inline bool active() { return csock != INVALID_SOCKET; }

	bool onRun()
	{
		if (!active())return false;
		fd_set fdRead;
		FD_ZERO(&fdRead);
		FD_SET(csock, &fdRead);
		timeval t = { 1,0 };
		int res = select(csock + 1, &fdRead, NULL, NULL, &t);
		if (res < 0)
		{
			std::cout << "selectģ��δ֪�����������" << std::endl;
			terminal();
			return false;
		}
		if (FD_ISSET(csock, &fdRead))
		{
			FD_CLR(csock, &fdRead);
			if (CLIENT_DISCONNECT == recvPack())
			{
				return false;
			}
		}
		return true;
	}
	template<typename PackType>
	int sendMessage(PackType* msg)
	{
		if (INVALID_SOCKET == csock)
		{
			std::cout << "�������׽���δ��ʼ������Ч" << std::endl;
			return CLIENT_ERROR;
		}
		int res = send(csock, (char*)msg, sizeof(PackType), 0);
		if (SOCKET_ERROR == res)
		{
			std::cout << "�������ݰ�ʧ��" << std::endl;
			return CLIENT_ERROR;
		}
		else
		{
			//std::cout << "���ͳɹ�" << std::endl;
		}
		return CLIENT_SUCCESS;
	}

	virtual void handleMessage(Pack* pk)
	{
		switch (pk->CMD)
		{
		case CMD_PRIVATEMESSAGE:
		{
			PrivateMessagePack* pack = static_cast<PrivateMessagePack*>(pk);
			std::cout << "�յ����� " << pack->targetName << " ������˽�ţ�" << pack->message << std::endl;
			break;
		}
		case CMD_MESSAGE:
		{
			MessagePack* pack = static_cast<MessagePack*>(pk);
			std::cout << "�յ���������������Ϣ:" << pack->message << std::endl;
			break;
		}
		case CMD_BROADCAST:
		{
			BroadcastPack* pack = static_cast<BroadcastPack*>(pk);

			std::cout << "�յ��㲥��Ϣ:" << pack->message << std::endl;
			break;
		}
		case CMD_NAME:
		{
			NamePack* pack = static_cast<NamePack*>(pk);
			std::cout << "�������ѽ���������Ϊ:" << pack->name << std::endl;
			break;
		}
		case CMD_TEST:
		{
			TestPack* pack = static_cast<TestPack*>(pk);
			if (pack->LENGTH != 1024)
			{
				std::cout << "�����Test��Ϣ:CMD=" << pk->CMD << " length=" << pk->LENGTH << " message = " << pack->message << std::endl;
			}
			break;
		}
		default:
		{
			std::cout << "�޷���������Ϣ:CMD=" << pk->CMD << " length=" << pk->LENGTH << std::endl;
			break;
		}
		}
	}

	private:
	//���ղ��������ݰ�
	int recvPack()
	{
		int len = recv(csock, recvBuf, RECV_BUF_SIZE, 0);

		if (len <= 0)
		{
			std::cout << "��������Ͽ�����" << std::endl;
			csock = INVALID_SOCKET;
			return CLIENT_DISCONNECT;
		}

		memcpy(msgBuf + lastBufPos, recvBuf, len);
		lastBufPos += len;
		while (lastBufPos >= sizeof(Header))
		{
			Pack* pack = reinterpret_cast<Pack*>(msgBuf);
			if (lastBufPos >= pack->LENGTH)
			{
				int nSize = lastBufPos - pack->LENGTH;
				handleMessage(pack);
				memcpy(msgBuf, msgBuf + pack->LENGTH, lastBufPos - pack->LENGTH);
				lastBufPos = nSize;
			}
			else
			{
				break;
			}
		}
		/*
		int len = recv(csock, buf, sizeof(Header), NULL);
		Header* header = reinterpret_cast<Header*>(buf);


		len = recv(csock, buf + sizeof(Header), header->LENGTH - sizeof(Header), NULL);
		handleMessage(reinterpret_cast<Pack*>(buf));*/

		return CLIENT_SUCCESS;
	}


};




#endif // !_TCPClient_HPP_

