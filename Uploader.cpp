#include "Uploader.h"

#include <WinSock2.h>
#include <ws2tcpip.h>

#include <string>
#include <vector>
#include <cstdint>

#include "SSL/SSL.h"
#include "Util.h"
#include "JSON.h"

inline void SendString(SSL_SOCKET *s, std::string string)
{
	s->s_send(const_cast<char *>(string.c_str()), string.size());
}

std::string Uploader::sendRequest(const std::string &uri, const std::vector<uint8_t> &requestBody)
{
	//Upload
	std::string hostname = "api.img.tl";

	addrinfo *info;
	getaddrinfo(hostname.c_str(), "https", NULL, &info);

	SOCKET s_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	connect(s_, info->ai_addr, info->ai_addrlen);
	SSL_SOCKET *s = new SSL_SOCKET(s_, 0, 0);
	s->ClientInit();

	if(requestBody.size())
		SendString(s, "POST " + uri + " HTTP/1.1\r\n");
	else
		SendString(s, "GET " + uri + " HTTP/1.1\r\n");
	SendString(s, "Host:" + hostname + "\r\n");
	SendString(s, std::string("Connection: close\r\n"));
	if(requestBody.size())
		SendString(s, "Content-Type: multipart/form-data; boundary=" + boundary_ + "\r\n");
		SendString(s, "Content-Length: " + ToString(requestBody.size()) + "\r\n");
	SendString(s, "X-IMGTL-TOKEN: " + token_ + "\r\n\r\n");
	if(requestBody.size())
		s->s_send(reinterpret_cast<const char *>(&requestBody[0]), requestBody.size());
	std::string data;
	while(true)
	{
		char buf[256];
		int n = s->s_recv(buf, 255);
		if(n <= 0)
			break;
		buf[n] = 0;
		data.append(buf);
	}
	if(data.size() == 0)
		return "";
	data = data.substr(data.find("\r\n\r\n") + 4);

	delete s;
	closesocket(s_);
	return data;
}

std::string Uploader::upload(const std::vector<uint8_t> &imageData)
{
	SYSTEMTIME st;
	FILETIME ft;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	uint64_t epoch = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL
	epoch = epoch / WINDOWS_TICK - SEC_TO_UNIX_EPOCH;

	std::string filename = "scrshot-" + ToString(epoch) + ".png";
	std::string uri = "/upload";

	//Prepare body
	std::vector<uint8_t> body;
	body.reserve(imageData.size() + 1000);

	AppendVector(body, "--" + boundary_ + "\r\n");
	AppendVector(body, "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n");
	AppendVector(body, std::string("Content-Type: application/octet-stream\r\n\r\n"));

	size_t oldSize = body.size();
	body.insert(body.end(), imageData.begin(), imageData.end());

	AppendVector(body, "\r\n--" + boundary_ + "--\r\n\r\n");

	auto value = JSONParser::parseJSONString(sendRequest(uri, body));

/*	{
		"data": {
			"url": {
				"direct": "https://img.tl/uMXJ.png", 
					"original": "https://s1.img.tl/0/1/010a7b4550e8c2151696e91a2612d344.png", 
					"page": "https://img.tl/uMXJ"
			}
		}, 
			"status": "success"
	}*/

	if(std::string(value["status"]) == "success")
		return value["data"]["url"]["page"];
	return "";
}

std::string Uploader::getUserEmail()
{
	auto value = JSONParser::parseJSONString(sendRequest("/user/info"));
	/*
	{
	"email": "dlunch@gmail.com", 
	"name": "dlunch", 
	"profile_image_url": "https://secure.gravatar.com/avatar/4770c01740993c820d5f77f9a73adfe6?d=https%3A%2F%2Fimg.tl%2Fimg%2Fuser_icon.png", 
	"uploads_count": 40
	}
	*/
	if(value.hasKey("error"))
		return "";
	return value["data"]["user"]["email"];
}