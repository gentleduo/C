#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdbool.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>  
#include "tchar.h"

#define DEFAULT_BUFLEN 8192  // ʹ�� 8KB �Ļ�����

void error(const char* msg) {
	perror(msg);
	WSACleanup();
	exit(1);
}

void receiveFile(SOCKET sockfd, const char* fileName) {
	int n;
	char buffer[DEFAULT_BUFLEN];
	long long fileSize;
	long long nowSize;
	//printf("�ȴ�����%s�Ĵ�С\n", fileName);
	send(sockfd, fileName, strlen(fileName), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		printf("�����ļ���Сʧ��\n");
	}
	buffer[n] = '\0';
	//printf("�����ļ���С��%s\n", buffer);
	fileSize = strtoll(buffer, NULL, 10);
	send(sockfd, buffer, strlen(buffer), 0);


	// �����ļ�
	FILE* fp = fopen(fileName, "wb");
	if (fp == NULL) {
		error("�����ļ�ʧ��");
	}

	// �����ļ�����
	float lastProgress = 0.0f; // ��һ�εĽ��Ȱٷֱ�
	nowSize = 0;
	while (1) {
		memset(buffer, 0, DEFAULT_BUFLEN);
		n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
		if (n <= 0) {
			printf("�����ж�\n");
			break;
		}
		fwrite(buffer, sizeof(char), n, fp);
		nowSize += n;
		// ������Ȱٷֱ�
		float progress = (float)nowSize / fileSize * 100;
		// ֻ�н��ȸ���ʱ�����
		if (progress != lastProgress) {
			// ʹ�ûس���������ƶ����еĿ�ͷ
			printf("\r");

			// ��ӡ����
			printf("�ѽ��� %.2f%%", progress);

			// ˢ�������������ȷ��������Ϣ������ʾ
			fflush(stdout);

			lastProgress = progress;
		}
		if (nowSize >= fileSize)
		{
			printf("�������\n");
			break;
		}
	}
	snprintf(buffer, DEFAULT_BUFLEN, "�ļ�%s�������", fileName);
	send(sockfd, buffer, strlen(buffer), 0);
	fclose(fp);
}

bool endsWithBackslash(const char* str) {
	size_t length = strlen(str);
	if (length > 0 && str[length - 1] == '\\') {
		return true;
	}
	return false;
}

bool folderExists(const char* folderPath) {
	struct _stat info;
	if (_stat(folderPath, &info) != 0) {
		return false;
	}
	return (info.st_mode & _S_IFDIR) != 0;
}

void RemoveLastFolder(char* path) {
	size_t len = strlen(path);

	if (len == 0)
		return; // ��·��

	// �������һ����·���ָ�����λ��
	size_t i = len - 1;
	while (i > 0 && (path[i] == '\\' || path[i] == '/'))
		i--;

	// �������һ��·���ָ�����λ��
	while (i > 0 && path[i] != '\\' && path[i] != '/')
		i--;

	// ����ҵ�·���ָ����������滻Ϊ���ַ�
	if (i > 0)
		path[i + 1] = '\0';
}

int main() {
	WSADATA wsaData;
	SOCKET sockfd, newsockfd;
	struct sockaddr_in addr, cli_addr;

	// ��ʼ��Winsock��
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		error("WSAStartup failed");
	}

	// �����׽���
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET) {
		error("Failed to create socket");
	}

	// ���õ�ַ�ṹ
	addr.sin_family = AF_INET;
	addr.sin_port = 0; // ʹ�ö�̬����Ķ˿ں�
	addr.sin_addr.s_addr = INADDR_ANY;

	// ���׽���
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		error("Bind failed");
	}

	// ��ȡ��̬����Ķ˿ں�
	socklen_t addrLen = sizeof(addr);
	if (getsockname(sockfd, (struct sockaddr*)&addr, &addrLen) == SOCKET_ERROR) {
		error("Error getting socket address");
	}

	// ������������
	if (listen(sockfd, 5) == SOCKET_ERROR) {
		error("Listen failed");
	}

	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) != 0) {
		fprintf(stderr, "Error getting hostname\n");
		WSACleanup();
		return 1;
	}

	struct hostent* host;
	host = gethostbyname(hostname);
	if (host == NULL) {
		fprintf(stderr, "Error getting host by name\n");
		WSACleanup();
		return 1;
	}

	char ip[INET_ADDRSTRLEN];
	struct in_addr** addr_list = (struct in_addr**)host->h_addr_list;
	strcpy(ip, inet_ntoa(*addr_list[0]));

	// ��ʾ�豸�� IP ��ַ�Ͷ�̬����Ķ˿ں�
	printf("���ն�������\n");
	printf("IP ��ַ: %s\n", ip);
	printf("�˿ں�: %hu\n", ntohs(addr.sin_port));

	char parentPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, parentPath);
	strcat(parentPath, "\\");

	while (1)
	{
		char nowPath[MAX_PATH];
		strcpy(nowPath, parentPath);
		SetCurrentDirectoryA(nowPath);
		//printf("��ǰ����·����%s\n", nowPath);
		// �ȴ�����
		printf("�ȴ����Ͷ�����...\n");
		socklen_t clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if (newsockfd == INVALID_SOCKET) {
			error("���ӷ��Ͷ�ʧ��");
		}
		printf("���Ͷ����ӳɹ�\n");

		char finishStr[] = "�����ļ��гɹ�";
		char finishStr2[] = "������һ��Ŀ¼�ɹ�";
		// �����ļ����ļ���
		while (1) {
			char buffer[DEFAULT_BUFLEN];
			int n = recv(newsockfd, buffer, DEFAULT_BUFLEN, 0);
			if (n <= 0) {
				break;
			}
			buffer[n] = '\0';

			if (strcmp(buffer, "end") == 0) {
				break;
			}


			if (strcmp(buffer, "..") == 0) {
				// ������һ��Ŀ¼
				RemoveLastFolder(nowPath);
				SetCurrentDirectoryA(nowPath);
				//printf("��ǰ����·����%s\n", nowPath);

				send(newsockfd, finishStr2, strlen(finishStr2), 0);
				continue;
			}

			char combinePath[MAX_PATH];
			snprintf(combinePath, MAX_PATH, "%s%s", nowPath, buffer);

			if (endsWithBackslash(combinePath)) {
				//�����ļ��в������ļ���
				//�����ļ���
				strcpy(nowPath, combinePath);
				if (!folderExists(nowPath))
				{
					if (_mkdir(nowPath) == -1) {
						printf("�����ļ���%sʧ��\n", nowPath);

					}
					else {
						printf("�����ļ���%s�ɹ�\n", nowPath);
					}
				}

				// �����ļ���
				if (SetCurrentDirectoryA(nowPath) == 0) {
					//printf("��ǰ����·����%s\n", nowPath);
				}
				send(newsockfd, finishStr, strlen(finishStr), 0);
			}
			else
			{
				printf("��ʼ�����ļ�%s\n", buffer);
				receiveFile(newsockfd, combinePath);

			}
		}

		// ѯ���Ƿ���������ļ�
		char answer;
		printf("��������Ƿ���������ļ���(y/n): ");
		scanf(" %c", &answer);
		if (answer != 'y' && answer != 'Y') {
			break;
		}
	}

	// �ر�����
	closesocket(newsockfd);
	closesocket(sockfd);
	WSACleanup();

	return 0;
}