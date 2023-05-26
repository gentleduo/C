#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#define DEFAULT_BUFLEN 8192  // 使用 8KB 的缓冲区

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

	// 检查是否使用引号包含路径
	if (len >= 2 && input[0] == '"' && input[end] == '"') {
		start = 1;
		end = len - 2;
	}
	else {
		// 查找第一个空格或换行符的位置
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

	// 打开文件
	FILE* fp = fopen(filePath, "rb");
	if (fp == NULL) {
		error("打开文件失败");
	}

	// 发送文件名
	char* pFileName = strrchr(filePath, '\\');
	if (pFileName != NULL) {
		pFileName++;  // 跳过目录分隔符
	}
	else {
		pFileName = filePath;  // 如果没有目录分隔符，则文件名就是整个路径
	}
	send(sockfd, pFileName, strlen(pFileName), 0);
	printf("开始发送文件名%s\n", pFileName);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	buffer[n] = '\0';
	//printf("发送文件名完成，收到回复：%s\n", buffer);

	//发送文件大小
	//printf("开始发送文件大小%lld\n", size);
	snprintf(buffer, DEFAULT_BUFLEN, "%lld", size);
	send(sockfd, buffer, strlen(buffer), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	//printf("发送文件大小完成，收到回复：%s\n", buffer);

	// 发送文件数据
	long long nowSize = 0;
	float lastProgress = 0.0f; // 上一次的进度百分比
	while (1) {
		memset(buffer, 0, DEFAULT_BUFLEN);
		size_t bytesRead = fread(buffer, sizeof(char), DEFAULT_BUFLEN, fp);
		if (bytesRead == 0) {
			break;
		}
		n = send(sockfd, buffer, bytesRead, 0);
		nowSize += n;
		// 计算进度百分比
		float progress = (float)nowSize / size * 100;
		// 只有进度更新时才输出
		if (progress != lastProgress) {
			// 使用回车符将光标移动到行的开头
			printf("\r");

			// 打印进度
			printf("已发送 %.2f%%", progress);

			// 刷新输出缓冲区，确保进度信息立即显示
			fflush(stdout);

			lastProgress = progress;
		}
		if (n <= 0) {
			error("发送文件失败");
		}
	}
	fclose(fp);
	printf("文件完成发送\n");
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	//printf("收到回复：%s\n", buffer);
}

void extractFolderName(const char* path, char* folderName) {
	// 获取路径长度
	size_t pathLength = strlen(path);

	// 寻找最后一个目录分隔符的位置
	const char* lastSeparator = strrchr(path, '/');
	if (lastSeparator == NULL) {
		lastSeparator = strrchr(path, '\\');
	}

	if (lastSeparator != NULL) {
		// 计算最后一个目录名称的长度
		size_t folderNameLength = pathLength - (lastSeparator - path) - 1;

		// 复制最后一个目录名称到新字符串
		strncpy(folderName, lastSeparator + 1, folderNameLength);
		folderName[folderNameLength] = '\0';
	}
	else {
		// 没有目录分隔符，直接将原路径复制到新字符串
		strcpy(folderName, path);
	}

	// 在目录名称末尾添加反斜杠
	strcat(folderName, "\\");
}


void sendFolder(SOCKET sockfd, const char* folderPath) {
	char folderName[MAX_PATH];
	char buffer[DEFAULT_BUFLEN];
	int n;
	extractFolderName(folderPath, folderName);
	printf("发送需要创建的文件夹名称：%s\n", folderName);
	send(sockfd, folderName, strlen(folderName), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		error("接收完成指令失败");
	}
	buffer[n] = '\0';
	//printf("收到回复：%s\n", buffer);

	struct _finddata_t fileData;
	intptr_t handle;
	int result;

	char searchPath[MAX_PATH];
	snprintf(searchPath, sizeof(searchPath), "%s\\*.*", folderPath);

	handle = _findfirst(searchPath, &fileData);
	if (handle == -1) {
		error("查找文件失败");
	}

	do {
		if (strcmp(fileData.name, ".") == 0 || strcmp(fileData.name, "..") == 0) {
			continue;
		}

		char filePath[MAX_PATH];
		snprintf(filePath, sizeof(filePath), "%s\\%s", folderPath, fileData.name);

		if (fileData.attrib & _A_SUBDIR) {
			// 子文件夹，递归发送
			sendFolder(sockfd, filePath);
		}
		else {
			// 文件，发送
			sendFile(sockfd, filePath);
		}
	} while (_findnext(handle, &fileData) == 0);
	//printf("发送返回上一级指令\n");
	char retStr[] = "..";
	send(sockfd, retStr, strlen(retStr), 0);
	n = recv(sockfd, buffer, DEFAULT_BUFLEN, 0);
	if (n <= 0) {
		error("返回上一级失败");
	}
	buffer[n] = '\0';
	//printf("收到回复：%s\n", buffer);
	_findclose(handle);
}

int main() {
	WSADATA wsaData;
	SOCKET sockfd;
	struct sockaddr_in serv_addr;

	// 初始化Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		error("WSAStartup failed");
	}

	while (1)
	{
		// 创建套接字
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
			error("Failed to create socket");
		}

		// 设置服务器地址结构
		serv_addr.sin_family = AF_INET;

		char ip[16];
		printf("请输入接收端的IP地址: ");
		scanf("%s", ip);
		serv_addr.sin_addr.s_addr = inet_addr(ip);

		unsigned short port;
		printf("请输入端口号: ");
		scanf("%hu", &port);
		serv_addr.sin_port = htons(port);

		if (inet_pton(AF_INET, ip, &(serv_addr.sin_addr)) <= 0) {
			error("IP地址无效或不支持");
		}

		// 连接到接收端
		if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
			error("连接到接收端失败");
		}

		// 发送文件或文件夹
		char path[MAX_PATH];
		printf("请输入要发送的文件或文件夹路径:\n");
		scanf("%s", path);

		struct _stat64 fileStat;
		int result = _stat64(path, &fileStat);
		if (result == -1) {
			error("获取文件/文件夹信息失败");
		}

		if (fileStat.st_mode & _S_IFDIR) {
			// 文件夹，发送文件夹
			sendFolder(sockfd, path);
		}
		else {
			// 文件，发送文件
			sendFile(sockfd, path);
		}

		printf("文件发送成功\n");

		closesocket(sockfd);

		// 询问是否继续发送文件
		char answer;
		printf("是否继续发送文件？(y/n): ");
		scanf(" %c", &answer);
		if (answer != 'y' && answer != 'Y') {
			break;
		}
	}

	WSACleanup();

	return 0;
}