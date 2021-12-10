#include "EasyTcpClient.hpp"
#include <thread>

using namespace std;

bool flag = true;
void cmdThread()
{
	while (true) 
	{
		char cmdbuf[256] = {};
		scanf("%s", cmdbuf);
		if (strcmp(cmdbuf, "exit") == 0)
		{
			flag = false;
			//client->Close();
			printf("�˳��߳�\n");
			return;
		}
		/*else if (strcmp(cmdbuf, "login") == 0)
		{
			Login login;
			strcpy(login.userName, "lyd");
			strcpy(login.PassWord, "lyd");
			client->SendData(&login);
		}
		else if (strcmp(cmdbuf, "logout") == 0)
		{
			Logout logout;
			strcpy(logout.userName, "lyd");
			client->SendData(&logout);
		}*/

		else printf("��֧�ִ�����\n");
	}
}

//�ͻ�������
const int cCount = 1000;
//�����߳�����
const int tCount = 4;
//�ͻ�������
EasyTcpClient* client[cCount];

void sendThread(int id)
{
	printf("thread<%d>,join\n", id);
	//4���߳� ID 1-4
	int c = cCount / tCount;
	int _begin = (id - 1)*c;
	int _end = id*c;

	for (int i = _begin;i < _end;i++)
	{
		if (!flag)
			return;
		client[i] = new EasyTcpClient();
	}
	for (int i = _begin;i < _end;i++)
	{
		client[i]->Connect("127.0.0.1", 4567);
		//printf("thread<%d>,Counnect = <%d>\n", id, i);
		
	}
	printf("thread<%d>,Counnect = <begin = %d,end = %d>\n", id, _begin, _end);
	chrono::milliseconds t(3000);
	this_thread::sleep_for(t);
	/*	EasyTcpClient client2;
	client2.Connect("127.0.0.1", 4568);
	thread t2(cmdThread, &client2);
	t2.detach();*/
	/*Login login[10];
	for (int i = 0;i < 10;i++) 
	{
		strcpy(login[i].userName, "lly");
		strcpy(login[i].PassWord, "lly");
	}*/
	Login login;
	strcpy(login.userName, "lly");
	strcpy(login.PassWord, "lly");
	while (flag)
	{
		for (int i = _begin;i < _end;i++)
		{
			client[i]->SendData(&login);
			//client[i]->OnRun();
		}
		//


	}
	for (int i = _begin;i < _end;i++)
	{
		client[i]->Close();
		delete client[i];
	}

	printf("thread<%d>,exit\n", id);
}

int main()
{
	// ���������߳�
	thread t(cmdThread);
	t.detach();
//	client.InitSocket();
	
	for (int i = 0;i < tCount;i++)
	{
		//���������߳�
		thread t1(sendThread, i + 1);
		t1.detach();
	}

	while (flag)
	{
		Sleep(100);
	}

	//7.�ر��׽��� closesocket
	printf("���˳�\n");
	return 0;
}
