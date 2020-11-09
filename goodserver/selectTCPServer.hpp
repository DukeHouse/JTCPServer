#ifndef _SELECTTCPServer_HPP_
#define _SELECTTCPServer_HPP_

#include <iostream>
#include <tuple>
#include <vector>
#include <algorithm>

#include <string>
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

#define CMD_ERROR 0
#define CMD_SUCCESS 1

#define CLIENT_DISCONNECT -1


class CLIENT
{
public:
	CLIENT(SOCKET csock, sockaddr_in csin, int userid, std::string username) :sock(csock), sin(csin), userID(userid), userName(username) {}
	inline SOCKET getSock() { return sock; }
	inline sockaddr_in getSin() { return sin; }
	inline int getUserID() { return userID; }
	inline std::string getUserName() { return userName; }
	inline void setUserName(std::string username) { userName = username; }
private:
	SOCKET sock;
	sockaddr_in sin;
	int userID;
	std::string userName;
};


class TCPServer
{
private:
	SOCKET ssock;
	sockaddr_in ssin;
	fd_set fdRead;
	fd_set fdWrite;
	fd_set fdExp;
public:
	std::vector<CLIENT> clients;
	inline SOCKET getSocket() { return ssock; }
	inline sockaddr_in getSockaddr_in() { return ssin; }
	TCPServer(const TCPServer& other) = delete;
	const TCPServer& operator=(const TCPServer& other) = delete;
public:
	//��ʼ��win����
	explicit inline TCPServer()
	{
		ssock = INVALID_SOCKET;
		ssin = {};
	}

	~TCPServer()
	{
		terminal();
	}

	//�жϷ������Ƿ�����������
	inline bool active() { return ssock != INVALID_SOCKET; }

	//��ʼ��socket
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
			std::cout << "�ѳɹ���ʼ��Winsock����!" << std::endl;
		}
#endif 
		ssock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == ssock)
		{
			std::cout << "��ʼ��������ʧ��" << std::endl;
			return CMD_ERROR;
		}
		else
		{
			std::cout << "��������ʼ���ɹ�!" << std::endl;
		}
		return CMD_SUCCESS;
	}

	//�󶨲������˿�
	int bindServer(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == ssock)
		{
			std::cout << "�׽���δ��ʼ������Ч" << std::endl;
			return CMD_ERROR;
		}
		ssin.sin_family = AF_INET;
		ssin.sin_port = htons(port);
#ifdef _WIN32
		ssin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		ssin.sin_addr.s_addr = inet_addr(ip);
#endif 
		int res = bind(ssock, (sockaddr*)&ssin, sizeof(ssin));
		if (SOCKET_ERROR == res)
		{
			std::cout << "�󶨶˿�ʧ��" << std::endl;
			return CMD_ERROR;
		}
		else
		{
			std::cout << "�˿ڰ󶨳ɹ�!" << std::endl;
		}

		if (SOCKET_ERROR == listen(ssock, 5))
		{
			std::cout << "�����˿�ʧ��" << std::endl;
			return -1;
		}
		else
		{
			std::cout << "�����˿ڳɹ�!" << std::endl;
		}

		return CMD_SUCCESS;
	}


	bool OnRun()
	{
		if (!active())return false;

		FD_ZERO(&fdRead);
		FD_ZERO(&fdWrite);
		FD_ZERO(&fdExp);
		FD_SET(ssock, &fdRead);
		FD_SET(ssock, &fdWrite);
		FD_SET(ssock, &fdExp);
		SOCKET maxSocket = ssock;
		for (auto& c : clients)
		{
			FD_SET(c.getSock(), &fdRead);
			maxSocket = std::max(maxSocket, c.getSock());
		}
		timeval t = { 1,0 };
		int res = select(maxSocket + 1, &fdRead, &fdWrite, &fdExp, &t);
		if (res < 0)
		{
			std::cout << "selectģ��δ֪�����������" << std::endl;
			terminal();
			return false;
		}
		if (FD_ISSET(ssock, &fdRead))//�¿ͻ�����
		{
			clients.push_back(acceptClient());
			FD_CLR(ssock, &fdRead);
			return true;
		}
		
		for (auto& i : clients)
		{	
			if (FD_ISSET(i.getSock(), &fdRead))
			{
				recvPack(i);
			}
			
		}
		return true;
	}

	//���ͻ��˷���Ϣ
	template<typename PackType>
	int sendMessage(SOCKET csock, PackType* msg)
	{
		if (INVALID_SOCKET == ssock)
		{
			std::cout << "�������׽���δ��ʼ������Ч" << std::endl;
			return CMD_ERROR;
		}
		int res = send(csock, (const char*)msg, sizeof(PackType), 0);
		if (SOCKET_ERROR == res)
		{
			std::cout << "�������ݰ�ʧ��" << std::endl;
			return CMD_ERROR;
		}
		else
		{

			//std::cout << "�������ݰ�to(" << csock << " ) " << msg->CMD << " " << msg->LENGTH << std::endl;
		}
		return CMD_SUCCESS;
	}

	//�ر�socket
	void terminal()
	{
		if (INVALID_SOCKET == ssock)return;
#ifdef _WIN32
		closesocket(ssock);
		WSACleanup();
#else //Linux
		close(ssock);
#endif 
		ssock = INVALID_SOCKET;
	}

	virtual void handleMessage(CLIENT& c, Pack* pk)
	{
		switch (pk->CMD)
		{
		case CMD_PRIVATEMESSAGE:
		{
			PrivateMessagePack* pack = static_cast<PrivateMessagePack*>(pk);
			std::cout << "ת��˽�� " << std::endl;
			std::string sourceName = "user";
			SOCKET target = 0;
			auto it = clients.begin();
			for (it; it < clients.end(); it++)
			{
				if ((*it).getUserName() == pack->targetName)
				{
					target = (*it).getSock();
					break;
				}
			}
			
			strcpy(pack->targetName, c.getUserName().c_str());
			if (it != clients.end())
			{
				sendMessage(target,pack);
			}
			else
			{
				MessagePack pack1;
				strcpy(pack1.message, "˽�ŷ���ʧ�ܣ�Ŀ���û������ڻ�������");
				sendMessage(c.getSock(), &pack1);
			}
			break;
		}
		case CMD_MESSAGE:
		{
			MessagePack* pack = static_cast<MessagePack*>(pk);
			std::cout << "�ӿͻ����յ�����Ϣ :CMD=" << pack->CMD << " LENGTH=" << pack->LENGTH << " DATA=" << pack->message << std::endl;
			strcpy(pack->message, "��Ϣ�ѳɹ�������������!");
			sendMessage(c.getSock(), pack);
			break;
		}
		case CMD_BROADCAST:
		{
			BroadcastPack* pack = static_cast<BroadcastPack*>(pk);
			std::cout << "�㲥��Ϣ" << std::endl;
			for (auto c1 : clients)
			{
				sendMessage(c1.getSock(), pack);
			}
			break;
		}
		case CMD_NAME:
		{
			NamePack* pack = static_cast<NamePack*>(pk);
			std::string userName = "";
			std::string oldName = "";
			auto it = clients.begin();
			for (it; it < clients.end(); it++)
			{
				if (c.getSock() == (*it).getSock())
				{
					oldName = (*it).getUserName();
					(*it).setUserName(pack->name);
					userName = pack->name;
					break;
				}
			}
			if (it!=clients.end())
			{
				MessagePack pack1;
				std::cout << "�û�" << oldName << "(" << c.getSock() << ")����Ϊ" << userName << std::endl;
				userName = "�ѳɹ��������ƣ����ڵ��ǳ�Ϊ " + userName;
				strcpy(pack1.message, userName.c_str());
				sendMessage(c.getSock(), &pack1);
			}
			else
			{
				MessagePack pack1;
				userName = "������ʧ��";
				strcpy(pack1.message, userName.c_str());
				sendMessage(c.getSock(), &pack1);
			}
			break;
		}
		default:
		{
			std::cout << "�޷���������Ϣ:CMD=" << pk->CMD << std::endl;
			break;
		}
		}
	}

private:

	//�������ӵĿͻ���
	CLIENT acceptClient()
	{
		SOCKET csock = INVALID_SOCKET;
		sockaddr_in csin = {};
		int sz = sizeof(csin);

#ifdef _WIN32
		csock = accept(ssock, (sockaddr*)&csin, &sz);
#else
		csock = accept(ssock, (sockaddr*)&csin, reinterpret_cast<socklen_t *>(&sz));
#endif  

		if (INVALID_SOCKET == csock)
		{
			std::cout << "��Ч�Ŀͻ����׽���" << std::endl;
		}
		else
		{
			std::cout << "�¿ͻ�������:" << csock << " IP:" << inet_ntoa(csin.sin_addr) << std::endl;
		}
		std::string username = "user" + std::to_string(csock);
		CLIENT c(csock, csin, csock, username);
		return c;

	}

	//���ղ��������ݰ�
	int recvPack(CLIENT& c)
	{
		char buf[4096] = { '\0' };
		SOCKET csock = c.getSock();
		int len = recv(csock, buf, sizeof(Header), NULL);
		Header* header =(Header*)buf;
		if (len <= 0)
		{
			std::cout << "�ͻ�" << c.getUserName() << "(csock=" << csock << ")�ѶϿ�����" << std::endl;
			for (auto it = clients.begin(); it < clients.end(); it++)
			{
				if ((*it).getSock() == c.getSock())
				{
					clients.erase(it);
					break;
				}
			}
			return CLIENT_DISCONNECT;
		}

		len = recv(csock, buf + sizeof(Header), header->LENGTH - sizeof(Header), NULL);
		handleMessage(c, (Pack*)buf);

		return CMD_SUCCESS;
	}
};




#endif 
