#ifndef _CellServer_HPP_
#define _CellServer_HPP_

#include "INetEvent.hpp"


class CellServer
{
private:
	SOCKET ssock;
	INetEvent* serverEvent;
	char recvBuf[RECV_BUF_SIZE] = {};
	char sendBuf[SEND_BUF_SIZE] = {};
	int lastSendPos;
	std::unordered_map<SOCKET, CLIENT*> clients;
	std::vector<CLIENT*> clientsBuf;
	std::mutex mtx;
	std::thread* mainThread = nullptr;
	fd_set fdRead;
	fd_set fdReadBak;
	SOCKET maxSocket;
	std::atomic<bool> fd_read_changed;
public:
	std::atomic<int> recvPackCount;

	CellServer(SOCKET serverSock, INetEvent* evt) :ssock(serverSock), serverEvent(evt)
	{
	}

	~CellServer()
	{
		delete mainThread;
	}

	void start()
	{
		mainThread = new std::thread(std::mem_fun(&CellServer::OnRun), this);
		mainThread->detach();
	}

	inline size_t getClientCount() { return clients.size() + clientsBuf.size(); }

	void addClientToBuf(CLIENT* c)
	{
		std::lock_guard<std::mutex> lg(mtx);
		clientsBuf.push_back(c);

	}

	//�жϷ������Ƿ�����������
	inline bool active() { return ssock != INVALID_SOCKET; }

	void checkHeart()
	{
		auto nowTime = NOWTIME_MILLI;
		for (auto it = clients.begin(); it != clients.end(); it++)
		{
			if (!it->second->checkHeart(nowTime))
			{
				serverEvent->OnLeave(it->second);
				fd_read_changed = true;
				std::lock_guard<std::mutex> lg(mtx);
				clients.erase(it->first);
			}
		}
	}

	bool OnRun()
	{
		while (active())
		{
			if (clientsBuf.size() > 0)
			{
				std::lock_guard<std::mutex> lock(mtx);
				for (auto c : clientsBuf)
				{
					clients.insert(std::make_pair(c->getSock(), c));
				}
				clientsBuf.clear();
				fd_read_changed = true;
			}
			if (clients.empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			checkHeart();
			FD_ZERO(&fdRead);

			if (fd_read_changed)
			{
				maxSocket = clients.begin()->first;
				for (auto c : clients)
				{
					FD_SET(c.first, &fdRead);
					maxSocket = std::max(maxSocket, c.first);
				}
				memcpy(&fdReadBak, &fdRead, sizeof(fd_set));
				fd_read_changed = false;
			}
			else
			{
				memcpy(&fdRead, &fdReadBak, sizeof(fd_set));
			}


			timeval t = { 0,1 };
			int res = select(maxSocket + 1, &fdRead, nullptr, nullptr, &t);
			if (res < 0)
			{
				std::cout << "selectģ��δ֪�����������" << std::endl;
				ssock = INVALID_SOCKET;
				return false;
			}

#ifdef _WIN32
			for (int i = 0; i < fdRead.fd_count; i++)
			{
				auto it = clients.find(fdRead.fd_array[i]);
				if (it != clients.end())
				{
					if (CLIENT_DISCONNECT == recvPack(it->second))
					{
						//std::cout << "�ͻ�" << it->second->getUserName() << "(csock=" << it->second->getUserName() << ")�ѶϿ�����" << std::endl;
						auto p = it->second;
						serverEvent->OnLeave(p);
						std::lock_guard<std::mutex> lg(mtx);
						clients.erase(it->first);
						fd_read_changed = true;
					}
				}
				else
				{
					std::cout << "��ֵ����" << std::endl;
				}

			}

#else

			for (auto c : clients)
			{
				if (FD_ISSET(c.first, &fdRead))
				{
					if (CLIENT_DISCONNECT == recvPack(c.second))
					{
						std::cout << "�ͻ�" << c.second->getUserName() << "(csock=" << c.second->getUserName() << ")�ѶϿ�����" << std::endl;
						auto p = c.second;
						serverEvent->OnLeave(p);
						std::lock_guard<std::mutex> lg(mtx);
						clients.erase(c.first);
						fd_read_changed = true;
						break;
					}
				}
			}

#endif // _WIN32




		}

	}

	//���ղ��������ݰ�
	int recvPack(CLIENT* c)
	{
		SOCKET csock = c->getSock();
		int len = recv(csock, recvBuf, RECV_BUF_SIZE, NULL);
		if (len <= 0)return CLIENT_DISCONNECT;

		memcpy(c->getmsgBuf() + c->getLastBufPos(), recvBuf, len);
		c->setLastBufPos(c->getLastBufPos() + len);
		while (c->getLastBufPos() >= sizeof(Header))
		{
			Pack* pack = reinterpret_cast<Pack*>(c->getmsgBuf());
			if (c->getLastBufPos() >= pack->LENGTH)
			{
				int nSize = c->getLastBufPos() - pack->LENGTH;
				handleMessage(c, pack);
				memcpy(c->getmsgBuf(), c->getmsgBuf() + pack->LENGTH, nSize);
				c->setLastBufPos(nSize);
			}
			else
			{
				break;
			}
		}
		return CMD_SUCCESS;
	}

	//���ͻ��˷���Ϣ
	int sendMessage(SOCKET csock, Pack* msg)
	{
		if (INVALID_SOCKET == ssock)
		{
			std::cout << "�������׽���δ��ʼ������Ч" << std::endl;
			return CMD_ERROR;
		}
		int sendLen = msg->LENGTH;
		if (lastSendPos + sendLen >= SEND_BUF_SIZE)
		{
			int res = send(csock, sendBuf, lastSendPos, 0);
			if (SOCKET_ERROR == res)
			{
				std::cout << "�������ݰ�ʧ��" << std::endl;
				return CMD_ERROR;
			}
			else
			{
				//std::cout << "�������ݰ�to(" << csock << " ) " << msg->CMD << " " << msg->LENGTH << std::endl;
			}
			lastSendPos = msg->LENGTH;
			memcpy(sendBuf, msg, msg->LENGTH);
		}
		else
		{
			memcpy(sendBuf + lastSendPos, msg, msg->LENGTH);
			lastSendPos += msg->LENGTH;
		}
		return CMD_SUCCESS;

	}

	int sendMessageReal(SOCKET csock, Pack* msg)
	{
		int res = SOCKET_ERROR;
		if (lastSendPos > 0 && SOCKET_ERROR != csock)
		{
			res = send(csock, sendBuf, lastSendPos, 0);
			lastSendPos = 0;
			//resetSendTime();
		}
		return res;
	}

	virtual void handleMessage(CLIENT* c, Pack* pk)
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
			for (it; it != clients.end(); it++)
			{
				if ((*it).second->getUserName() == pack->targetName)
				{
					target = (*it).first;
					break;
				}
			}

			strcpy(pack->targetName, c->getUserName().c_str());
			if (it != clients.end())
			{
				sendMessage(target, pack);
			}
			else
			{
				MessagePack pack1;
				strcpy(pack1.message, "˽�ŷ���ʧ�ܣ�Ŀ���û������ڻ�������");
				sendMessage(c->getSock(), &pack1);
			}
			break;
		}
		case CMD_MESSAGE:
		{
			MessagePack* pack = static_cast<MessagePack*>(pk);
			std::cout << "�ӿͻ���(" << c->getSock() << ")�յ�����Ϣ :CMD=" << pack->CMD << " LENGTH=" << pack->LENGTH << " DATA=" << pack->message << std::endl;
			strcpy(pack->message, "��Ϣ�ѳɹ�������������!");
			sendMessage(c->getSock(), pack);
			break;
		}
		case CMD_BROADCAST:
		{
			BroadcastPack* pack = static_cast<BroadcastPack*>(pk);
			std::cout << "�㲥��Ϣ" << std::endl;
			for (auto c1 : clients)
			{
				sendMessage(c1.first, pack);
			}
			break;
		}
		case CMD_NAME:
		{
			NamePack* pack = static_cast<NamePack*>(pk);
			std::string userName = "";
			std::string oldName = "";
			auto it = clients.begin();
			for (it; it != clients.end(); it++)
			{
				if (c->getSock() == (*it).first)
				{
					oldName = (*it).second->getUserName();
					(*it).second->setUserName(pack->name);
					userName = pack->name;
					break;
				}
			}
			if (it != clients.end())
			{

				std::cout << "�û�" << oldName << "(" << c->getSock() << ")����Ϊ" << userName << std::endl;
				userName = "�ѳɹ��������ƣ����ڵ��ǳ�Ϊ " + userName;
				MessagePack pack1(userName.c_str());
				sendMessage(c->getSock(), &pack1);
			}
			else
			{
				MessagePack pack1("������ʧ��");
				sendMessage(c->getSock(), &pack1);
			}
			break;
		}
		case CMD_TEST:
		{
			recvPackCount++;
			TestPack pack("dwadawd");
			sendMessage(c->getSock(), &pack);
			break;
		}
		case CMD_HEART:
		{
			c->resetHeart();
			break;
		}
		default:
		{
			std::cout << "�޷���������Ϣ:CMD=" << pk->CMD << " length=" << pk->LENGTH << std::endl;
			break;
		}
		}
	}

};



#endif


