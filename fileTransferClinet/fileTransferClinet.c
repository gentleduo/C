#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#define DEFAULT_BUFLEN 8192  // ʹ�� 8KB �Ļ�����

void error(const char* msg) {
	perror(msg);
	WSACleanup();
	exit(1);
}

char* extractPathFromInput(const char* input) {
	char* path = NULL;
	size_t len = strlen(input);
	int start = 0;
	int end = len - 1;

	// ����Ƿ�ʹ�����Ű���·��
	if (len >= 2 && input[0] == '"' && input[end] == '"') {
		start = 1;
		end = len - 2;
	}
	else {
		// ���ҵ�һ���ո���з���λ��
		int i;
		for (i = 0; i < len; i++) {
			if (input[i] == ' ' || input[i] == '\n') {
				end = i - 1;
				break;
			}
		}
	}

	if (end >= start) {
		len = end - start + 1;
		path = malloc(len + 1);
		strncpy(path, input + start, len);
		path[len] = '\0';
	}

	return path;
}

long long getFileSize(const char* path) {
	struct _stat64 fileStat;

	if (_stat64(path, &fileStat) == 0) {
		return fileStat.st_size;
	}
	else {
		return -1;
	}
}

void sendFile(SOCKET sockfd, const char* filePath) {
	char buffer[DEFAULT_BUFLEN];
	int n;
	long long size = getFileSize(filePath);

	// ���ļ�
	FILE* fp = fopen(filePath, "rb");
	if (fp == NULL) {
		error("���ļ�ʧ��");
	}

	// �����ļ���
	char* pFileName = strrchr(filePath, '\\');
	if (pFileName != NULL) {
		pFileName++;  // ����Ŀ¼�ָ���
	}
	else {
		pFileName = filePath;  // ���û��Ŀ¼�ָ��������ļ�����������·��
	}
	send(sockfd, pFileName, strlen(pFileName), 0);
	printf("��ʼ�����ļ���%s\n", pFileName);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	buffer[n] = '\0';
	//printf("�����ļ�����ɣ��յ��ظ���%s\n", buffer);

	//�����ļ���С
	//printf("��ʼ�����ļ���С%lld\n", size);
	snprintf(buffer, DEFAULT_BUFLEN, "%lld", size);
	send(sockfd, buffer, strlen(buffer), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	//printf("�����ļ���С��ɣ��յ��ظ���%s\n", buffer);

	// �����ļ�����
	long long nowSize = 0;
	float lastProgress = 0.0f; // ��һ�εĽ��Ȱٷֱ�
	while (1) {
		memset(buffer, 0, DEFAULT_BUFLEN);
		size_t bytesRead = fread(buffer, sizeof(char), DEFAULT_BUFLEN, fp);
		if (bytesRead == 0) {
			break;
		}
		n = send(sockfd, buffer, bytesRead, 0);
		nowSize += n;
		// ������Ȱٷֱ�
		float progress = (float)nowSize / size * 100;
		// ֻ�н��ȸ���ʱ�����
		if (progress != lastProgress) {
			// ʹ�ûس���������ƶ����еĿ�ͷ
			printf("\r");

			// ��ӡ����
			printf("�ѷ��� %.2f%%", progress);

			// ˢ�������������ȷ��������Ϣ������ʾ
			fflush(stdout);

			lastProgress = progress;
		}
		if (n <= 0) {
			error("�����ļ�ʧ��");
		}
	}
	fclose(fp);
	printf("�ļ���ɷ���\n");
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	//printf("�յ��ظ���%s\n", buffer);
}

void extractFolderName(const char* path, char* folderName) {
	// ��ȡ·������
	size_t pathLength = strlen(path);

	// Ѱ�����һ��Ŀ¼�ָ�����λ��
	const char* lastSeparator = strrchr(path, '/');
	if (lastSeparator == NULL) {
		lastSeparator = strrchr(path, '\\');
	}

	if (lastSeparator != NULL) {
		// �������һ��Ŀ¼���Ƶĳ���
		size_t folderNameLength = pathLength - (lastSeparator - path) - 1;

		// �������һ��Ŀ¼���Ƶ����ַ���
		strncpy(folderName, lastSeparator + 1, folderNameLength);
		folderName[folderNameLength] = '\0';
	}
	else {
		// û��Ŀ¼�ָ�����ֱ�ӽ�ԭ·�����Ƶ����ַ���
		strcpy(folderName, path);
	}

	// ��Ŀ¼����ĩβ��ӷ�б��
	strcat(folderName, "\\");
}


void sendFolder(SOCKET sockfd, const char* folderPath) {
	char folderName[MAX_PATH];
	char buffer[DEFAULT_BUFLEN];
	int n;
	extractFolderName(folderPath, folderName);
	printf("������Ҫ�������ļ������ƣ�%s\n", folderName);
	send(sockfd, folderName, strlen(folderName), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		error("�������ָ��ʧ��");
	}
	buffer[n] = '\0';
	//printf("�յ��ظ���%s\n", buffer);

	struct _finddata_t fileData;
	intptr_t handle;
	int result;

	char searchPath[MAX_PATH];
	snprintf(searchPath, sizeof(searchPath), "%s\\*.*", folderPath);

	handle = _findfirst(searchPath, &fileData);
	if (handle == -1) {
		error("�����ļ�ʧ��");
	}

	do {
		if (strcmp(fileData.name, ".") == 0 || strcmp(fileData.name, "..") == 0) {
			continue;
		}

		char filePath[MAX_PATH];
		snprintf(filePath, sizeof(filePath), "%s\\%s", folderPath, fileData.name);

		if (fileData.attrib & _A_SUBDIR) {
			// ���ļ��У��ݹ鷢��
			sendFolder(sockfd, filePath);
		}
		else {
			// �ļ�������
			sendFile(sockfd, filePath);
		}
	} while (_findnext(handle, &fileData) == 0);
	//printf("���ͷ�����һ��ָ��\n");
	char retStr[] = "..";
	send(sockfd, retStr, strlen(retStr), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		error("������һ��ʧ��");
	}
	buffer[n] = '\0';
	//printf("�յ��ظ���%s\n", buffer);
	_findclose(handle);
}

int main() {
	WSADATA wsaData;
	SOCKET sockfd;
	struct sockaddr_in serv_addr;

	// ��ʼ��Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		error("WSAStartup failed");
	}

	while (1)
	{
		// �����׽���
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
			error("Failed to create socket");
		}

		// ���÷�������ַ�ṹ
		serv_addr.sin_family = AF_INET;

		char ip[16];
		printf("��������ն˵�IP��ַ: ");
		scanf("%s", ip);
		serv_addr.sin_addr.s_addr = inet_addr(ip);

		unsigned short port;
		printf("������˿ں�: ");
		scanf("%hu", &port);
		serv_addr.sin_port = htons(port);

		if (inet_pton(AF_INET, ip, &(serv_addr.sin_addr)) <= 0) {
			error("IP��ַ��Ч��֧��");
		}

		// ���ӵ����ն�
		if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
			error("���ӵ����ն�ʧ��");
		}

		// �����ļ����ļ���
		char path[MAX_PATH];
		printf("������Ҫ���͵��ļ����ļ���·��:\n");
		scanf("%s", path);

		struct _stat64 fileStat;
		int result = _stat64(path, &fileStat);
		if (result == -1) {
			error("��ȡ�ļ�/�ļ�����Ϣʧ��");
		}

		if (fileStat.st_mode & _S_IFDIR) {
			// �ļ��У������ļ���
			sendFolder(sockfd, path);
		}
		else {
			// �ļ��������ļ�
			sendFile(sockfd, path);
		}

		printf("�ļ����ͳɹ�\n");

		closesocket(sockfd);

		// ѯ���Ƿ���������ļ�
		char answer;
		printf("�Ƿ���������ļ���(y/n): ");
		scanf(" %c", &answer);
		if (answer != 'y' && answer != 'Y') {
			break;
		}
	}

	WSACleanup();

	return 0;
}