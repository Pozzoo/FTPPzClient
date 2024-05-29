#include <iostream>
#include <WS2tcpip.h>
#include <algorithm>
#include <fstream>
#include <json.hpp>


#pragma comment (lib, "ws2_32.lib")

constexpr auto DELIMITER = "\r\n\r\n";

bool compareStrings(std::string first, std::string second) {
	transform(first.begin(), first.end(), first.begin(), ::tolower);
	transform(second.begin(), second.end(), second.begin(), ::tolower);

	return first == second;
}

nlohmann::json listCommand() {
	nlohmann::json package = {
		{"command", "list"},
	};

	return package;
}

void handleGetOperation(std::string content, nlohmann::json json) {
	uint64_t calculatedHash = std::hash<std::string>{}(content);

	std::string messageStr;
	nlohmann::json messageJson;
	std::string file = json.at("file");

	if (json.at("hash") != calculatedHash) {
		std::cout << "Error on file transfer: Hash does not Match!" << std::endl;

	} else {
		std::ofstream copy("download/" + file, std::ios::binary);
		copy << content;

		std::cout << "File downloaded successfully" << std::endl;
	}
}

nlohmann::json getCommand(std::string fileName) {
	nlohmann::json package = {
		{"command", "get"},
		{"file", fileName},
	};

	return package;
}

std::tuple<nlohmann::json, std::string> putCommand(std::string filePath) {
	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		std::cerr << "Error opening file" << std::endl;
		return { nullptr, " " };
	}

	std::string content(std::istreambuf_iterator<char>(file), {});

	nlohmann::json package = {
		{"command", "put"},
		{"file", filePath.substr(filePath.find_last_of("/\\") + 1)},
		{"hash", std::hash<std::string>{}(content + DELIMITER)}
	};

	return { package, content };
}

void responseHandler(SOCKET sock) {
	int stat;
	char buf[1024] = { 0 };
	std::string output = { 0 };

	while ((stat = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
		output.append(buf, stat);

		if (output.find(DELIMITER) != std::string::npos) {
			break;
		}
	}

	if (stat == 0) {
		std::cout << "Connection closed by server." << std::endl;
	} else if (stat < 0) {
		std::cerr << "Receive error: " << WSAGetLastError() << std::endl;
		closesocket(sock);
	}

	std::string content = output.substr(output.find_first_of("}") + 1);
	std::string jsonStr = output.substr(1, output.find_first_of("}"));
	nlohmann::json json = nlohmann::json::parse(jsonStr);

	if (json.at("operation") == "put") {
		std::cout << "operation " << json.at("status") << std::endl;
	} else if (json.at("operation") == "get") {
		handleGetOperation(content, json);
	} else {
		std::vector<std::string> items = json.at("items");

		for(std::string item : items) {
			std::cout << item.substr(10) << std::endl;
		}
	}
}

void commandHandler(SOCKET sock) {
	std::string command;
	std::string fileName;

	nlohmann::json package = nullptr;
	std::string content = "-";

	std::cout << "--------------------------------------------" << std::endl;
	std::cout << "Write a command (PUT/GET/LIST)" << std::endl;
	std::cin >> command;

	if (compareStrings(command, "get")) {
		std::cin >> fileName;
		package = getCommand(fileName);
	} else if (compareStrings(command, "put")) {
		std::cin >> fileName;
		std::tie(package, content) = putCommand(fileName);
	} else if (compareStrings(command, "list")) {
		package = listCommand();
	} else {
		std::cout << "Invalid Command" << std::endl;
		return;
	}

	if (package == nullptr) {
		return;
	}

	if (content == " ") {
		return;
	}

	std::string packageStr = package.dump();
	packageStr.append(content + DELIMITER);
	const char* buf = packageStr.c_str();

	send(sock, buf, packageStr.size(), 0);

	responseHandler(sock);
}

int main() {

	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsErr = WSAStartup(ver, &wsData);

	if (wsErr != 0) {
		std::cerr << "Can´t Initialize Winsock!" << std::endl;
		return 1;
	}

	SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSock == INVALID_SOCKET) {
		std::cerr << "Can't create a socket: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(5400);
	InetPton(AF_INET, TEXT("127.0.0.1"), &hint.sin_addr.s_addr);

	if (connect(clientSock, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
		std::cerr << "Can't connect socket: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	std::cout << "Connection succesful" << std::endl;

	while (true) {
		commandHandler(clientSock);
	}

	closesocket(clientSock);
	WSACleanup();
	return 0;
}
    