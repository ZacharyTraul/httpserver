#ifndef HTTPMESSAGE_H
#define HTTPMESSAGE_H
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unistd.h>

extern std::vector<std::string> methods;
extern std::vector<std::string> field_strings;

class HTTPResponseLine{
	public:
		std::string version, status_code, reason_phrase;
		size_t length;

		HTTPResponseLine();
		HTTPResponseLine(std::string v, std::string s, std::string r);
		void reassign(std::string v, std::string s, std::string r);
		std::string to_str();
};

class HTTPRequestLine{
	public:
		std::string method, uri, version;
		bool valid;

		HTTPRequestLine();
		HTTPRequestLine(std::string request);
		std::string to_str();
};

class HTTPHeader{
	public:
		std::map<std::string, std::string> fields;
		size_t length;

		HTTPHeader();
		HTTPHeader(std::string header);
		std::string to_str();
};

struct HTTPEntityBody{
	size_t length;
	char * body;
};


class HTTPMessage{
	public:
		HTTPResponseLine response_line;
		HTTPRequestLine request_line;
		HTTPHeader header;
		HTTPEntityBody entity_body;
		bool valid;
		size_t length;

		HTTPMessage();
		HTTPMessage(std::string version, std::string status_code, std::string reason_phrase);
		HTTPMessage(std::string message);
		void reassign(std::string status_code, std::string reason_phrase);
		void write_message(int connection, std::string method);
};
#endif
