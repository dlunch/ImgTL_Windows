#pragma once

#include <string>
#include <vector>
#include <cstdint>

class Uploader
{
private:
	std::string boundary_;
	std::string token_;
	std::string sendRequest(const std::string &uri, const std::vector<uint8_t> &requestBody = std::vector<uint8_t>());
public:
	Uploader() : boundary_("---------------Boundaryimgtl151235fa") {}
	std::string upload(const std::vector<uint8_t> &data);
	std::string getUserEmail();
	void setToken(const std::string &token)
	{
		token_ = token;
	}
};