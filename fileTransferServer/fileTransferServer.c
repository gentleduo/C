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

#define DEFAULT_BUFLEN 8192  // 使用 8KB 的缓冲区

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
	//printf("等待接收%s的大小\n", fileName);
	send(sockfd, fileName, strlen(fileName), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		printf("接收文件大小失败\n");
	}
	buffer[n] = '\0';
	//printf("接收文件大小：%s\n", buffer);
	fileSize = strtoll(buffer, NULL, 10);
	send(sockfd, buffer, strlen(buffer), 0);


	// 创建文件
	FILE* fp = fopen(fileName, "wb");
	if (fp == NULL) {
		error("创建文件失败");
	}

	// 接收文件数据
	float lastProgress = 0.0f; // 上一次的进度百分比
	nowSize = 0;
	while (1) {
		memset(buffer, 0, DEFAULT_BUFLEN);
		n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
		if (n <= 0) {
			printf("接收中断\n");
			break;
		}
		fwrite(buffer, sizeof(char), n, fp);
		nowSize += n;
		// 计算进度百分比
		float progress = (float)nowSize / fileSize * 100;
		// 只有进度更新时才输出
		if (progress != lastProgress) {
			// 使用回车符将光标移动到行的开头
			printf("\r");

			// 打印进度
			printf("已接收 %.2f%%", progress);

			// 刷新输出缓冲区，确保进度信息立即显示
			fflush(stdout);

			lastProgress = progress;
		}
		if (nowSize >= fileSize)
		{
			printf("接收完成\n");
			break;
		}
	}
	snprintf(buffer, DEFAULT_BUFLEN, "文件%s接收完成", fileName);
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
		return; // 空路径

	// 搜索最后一个非路径分隔符的位置
	size_t i = len - 1;
	while (i > 0 && (path[i] == '\\' || path[i] == '/'))
		i--;

	// 搜索最后一个路径分隔符的位置
	while (i > 0 && path[i] != '\\' && path[i] != '/')
		i--;

	// 如果找到路径分隔符，则将其替换为空字符
	if (i > 0)
		path[i + 1] = '\0';
}

int main() {
	WSADATA wsaData;
	SOCKET sockfd, newsockfd;
	struct sockaddr_in addr, cli_addr;

	// 初始化Winsock库
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		error("WSAStartup failed");
	}

	// 创建套接字
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET) {
		error("Failed to create socket");
	}

	// 设置地址结构
	addr.sin_family = AF_INET;
	addr.sin_port = 0; // 使用动态分配的端口号
	addr.sin_addr.s_addr = INADDR_ANY;

	// 绑定套接字
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		error("Bind failed");
	}

	// 获取动态分配的端口号
	socklen_t addrLen = sizeof(addr);
	if (getsockname(sockfd, (struct sockaddr*)&addr, &addrLen) == SOCKET_ERROR) {
		error("Error getting socket address");
	}

	// 监听连接请求
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

	// 显示设备的 IP 地址和动态分配的端口号
	printf("接收端已启动\n");
	printf("IP 地址: %s\n", ip);
	printf("端口号: %hu\n", ntohs(addr.sin_port));

	char parentPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, parentPath);
	strcat(parentPath, "\\");

	while (1)
	{
		char nowPath[MAX_PATH];
		strcpy(nowPath, parentPath);
		SetCurrentDirectoryA(nowPath);
		//printf("当前所在路径：%s\n", nowPath);
		// 等待连接
		printf("等待发送端连接...\n");
		socklen_t clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if (newsockfd == INVALID_SOCKET) {
			error("连接发送端失败");
		}
		printf("发送端连接成功\n");

		char finishStr[] = "创建文件夹成功";
		char finishStr2[] = "返回上一级目录成功";
		// 接收文件或文件夹
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
				// 返回上一级目录
				RemoveLastFolder(nowPath);
				SetCurrentDirectoryA(nowPath);
				//printf("当前所在路径：%s\n", nowPath);

				send(newsockfd, finishStr2, strlen(finishStr2), 0);
				continue;
			}

			char combinePath[MAX_PATH];
			snprintf(combinePath, MAX_PATH, "%s%s", nowPath, buffer);

			if (endsWithBackslash(combinePath)) {
				//创建文件夹并进入文件夹
				//创建文件夹
				strcpy(nowPath, combinePath);
				if (!folderExists(nowPath))
				{
					if (_mkdir(nowPath) == -1) {
						printf("创建文件夹%s失败\n", nowPath);

					}
					else {
						printf("创建文件夹%s成功\n", nowPath);
					}
				}

				// 进入文件夹
				if (SetCurrentDirectoryA(nowPath) == 0) {
					//printf("当前所在路径：%s\n", nowPath);
				}
				send(newsockfd, finishStr, strlen(finishStr), 0);
			}
			else
			{
				printf("开始接收文件%s\n", buffer);
				receiveFile(newsockfd, combinePath);

			}
		}

		// 询问是否继续接收文件
		char answer;
		printf("接收完成是否继续接收文件？(y/n): ");
		scanf(" %c", &answer);
		if (answer != 'y' && answer != 'Y') {
			break;
		}
	}

	// 关闭连接
	closesocket(newsockfd);
	closesocket(sockfd);
	WSACleanup();

	return 0;
}