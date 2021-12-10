#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
	#define _WINSOCK_DEPRECATED_NO_WARNINGS 0
	#define _WIN32_LEAN_AND_MEAN
	#include <WinSock2.h>
	#include <windows.h>
	# pragma comment(lib,"ws2_32.lib")
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <string.h>

	#define SOCKET int
#define INVALID_SOCKET(SOCKET)(~0)
#define SOCKET_ERROR			(-1)

#endif //win32
#include <stdio.h>
#include "MessageHeader.hpp"
using namespace std;


class EasyTcpClient
{
		SOCKET _sock;
		bool _isConnect;
public:
	EasyTcpClient()
	{
		//��ʼ��
		_sock = INVALID_SOCKET;
		_isConnect = false;
	}

	virtual ~EasyTcpClient()
	{
		Close();
	}
	//��ʼ��socket
	void InitSocket()
	{
		//����in Sock 2.x����
#ifdef _WIN32
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		//1.����һ��socket
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket = %d>�رվ�����...\n", _sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			printf("error\n");
		}
		//else printf("�����ɹ�\n");

	}

	//���ӷ�����
	int Connect(char *ip,unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		//2.���ӷ����� connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port); //���Ӷ˿ں�
#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip); //���ӷ�������ַ 127Ϊ�������Ե�ַ
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret)
		{
			printf("error\n");
		}
		else
		{
			_isConnect = true;
		}
		//else printf("���ӷ������ɹ�\n");
		return ret;
	}
	
	//�ر�socket
	void Close()
	{
		//�ر� in Sock 2.x����
		//�ر��׽���closesocket
		if (_sock != INVALID_SOCKET) 
		{
#ifdef _WIN32
			closesocket(_sock);
			//���windows socket����
			WSACleanup();
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
		_isConnect = false;
	}

	//����������Ϣ
	bool OnRun()
	{
		if (isRun())
		{
			fd_set fdRead;
			FD_ZERO(&fdRead);
			FD_SET(_sock, &fdRead, NULL, NULL);
			timeval t = { 1,0 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret<0)
			{
				printf("<socket = %d>select�������1\n", _sock);
				return false;
			}
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);

				if (RecvData(_sock) == -1)
				{
					printf("<socket = %d>select�������2\n", _sock);
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET && _isConnect;
	}
	
	//��������С��Ԫ��С
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif
	//�û���������
	char _szRecv[RECV_BUFF_SIZE] = {};
	//�ڶ������� ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE*5] = {};
	//��Ϣ������β��λ��
	int _lastPos = 0;
	//�������� ����ճ�� ��ְ�
	int RecvData(SOCKET _cSock)
	{
		// 5.���տͻ�������
		int nlen = (int)recv(_cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			printf("<Socket = %d>��������Ͽ����ӣ��������\n", _cSock);
			return -1;
		}
		//����ȡ�������ݿ�������Ϣ������
		memcpy(_szMsgBuf + _lastPos, _szRecv, nlen);
		//��Ϣ������������β��λ�ú���
		_lastPos += nlen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataheader�ĳ���
		while (_lastPos >= sizeof(DataHeader))
		{
			//��ʱ���ǾͿ���֪����ǰ��Ϣ��ĳ���
			DataHeader* header = (DataHeader *)_szMsgBuf;

			if (_lastPos >= header->dataLength)
			{
				//ʣ��δ������Ϣ���������ݵĳ���
				const int nSize = _lastPos - header->dataLength;
				//����������Ϣ�����ܻ�ı�header��
				OnNetMsg(header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(_szMsgBuf, _szMsgBuf+header->dataLength, nSize);
				_lastPos = nSize;
			}
			else break;
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN_RESULT:
		{
			LoginResult* login = (LoginResult *)header;
			//printf("<Socket = %d>�յ���������Ϣ�� CMD_LOGIN_RESULT , ���ݳ��ȣ� %d\n", _sock, login->dataLength);
		}
		break;
		case CMD_LOGOUT_RESULT:
		{
			LogoutResult* logout = (LogoutResult *)header;
			//printf("<Socket = %d>�յ���������Ϣ�� CMD_LOGOUT_RESULT , ���ݳ��ȣ� %d\n", _sock, logout->dataLength);
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* userJoin = (NewUserJoin *)header;
			//printf("<Socket = %d>�յ���������Ϣ�� CMD_NEW_USER_JOIN , ���ݳ��ȣ� %d\n", _sock, userJoin->dataLength);
		}
		break;
		case CMD_ERROR:
		{
			//printf("<Socket = %d>�յ���������Ϣ�� CMD_ERROR , ���ݳ��ȣ� %d\n", _sock , header->dataLength);
		}
		break;
		default:
		{
			printf("<Socket = %d>�յ�δ������Ϣ�����ݳ��ȣ� %d\n", _sock, header->dataLength);
		}
		}
	}

	//��������
	int SendData(DataHeader* header)
	{
		int ret = SOCKET_ERROR;
		if (isRun() && header != NULL) 
		{
			ret = send(_sock, (const char *)header, header->dataLength, 0);
			if (ret == SOCKET_ERROR)
			{
				Close();
			}
		}
		return ret;
	}
private:


};


#endif