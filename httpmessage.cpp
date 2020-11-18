#include "httpmessage.h"

//A few global variables
std::vector<std::string> methods{"OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT"};
std::vector<std::string> field_strings{
"Accept", "Accept-Charset", "Accept-Encoding", "Accept-Language", 
"Accept-Ranges", "Age", "Allow", "Authorization", "Cache-Control", 
"Connection", "Content-Encoding", "Content-Language", "Content-Length", 
"Content-Location", "Content-MD5", "Content-Range", "Content-Type", 
"Date", "ETag", "Expect", "Expires", "From", "Host", "If-Match", 
"If-Modified-Since", "If-None-Match", "If-Unmodified-Since",
"Last-Modified", "Location", "Max-Forwards", "Pragma", "Proxy-Authenticate",
"Proxy-Authorization", "Range", "Referer", "Retry-After", "Server", "TE", 
"Trailer", "Transfer-Encoding", "Upgrade", "User-Agent", "Vary", "Via", 
"Warning", "WWW-Authenticate"}; 

//HTTPResponseLine Constructors
//Default
HTTPResponseLine::HTTPResponseLine(){}
void HTTPResponseLine::reassign(std::string v, std::string s, std::string r){
	version = v;
	status_code = s;
	reason_phrase = s;
}
//Returns a valid HTTP/1.0 response line as a string
std::string HTTPResponseLine::to_str(){
	std::string out = version + " " + status_code + " " + reason_phrase + "\r\n";
	length = out.length();
	return out;
}

//HTTPRequestLine Constructors
//Default
HTTPRequestLine::HTTPRequestLine(){}
HTTPRequestLine::HTTPRequestLine(std::string request){
	valid = false;
	//Get Method		
	for(int i = 0; i < methods.size(); i++){
		if(request.find(methods[i]) == 0){
			method = methods[i];
			valid = true;
			break;
		}
	}	
	//Get URI
	if(request.find(method + " /") == 0 && valid){
		size_t beg = (method + " ").length();
		if(beg < request.length()){
			size_t len = request.substr(beg).find(" ");
			if(len != std::string::npos){
				uri = request.substr(beg, len);	
			} else valid = false;
		} else valid = false;
	}
	//Make sure there's no funny business going on in the uri
	if((uri.find("..") != std::string::npos || uri.find("/./") != std::string::npos) && valid){
		valid = false;
	}
	//Get Version
	if(request.find(method + " " + uri + " HTTP/") == 0 && valid){
		size_t beg = (method + " " + uri + " ").length();
		if(beg < request.length()){
			size_t len = request.substr(beg).find("\r\n");
			if(len != std::string::npos){ 
				version = request.substr(beg, len); 
			} else valid = false;
		} else valid = false;
	}
}
//Returns a valid HTTP/1.0 request line as a string
std::string HTTPRequestLine::to_str(){
	return method + " " + uri + " " + version + "\r\n";
}

//HTTPHeader Constructors
//Default: used when crafting a response.
HTTPHeader::HTTPHeader(){
	//Fill in the date header
	time_t now = time(NULL);
	struct tm * timeinfo;
	char buffer[30];
	timeinfo = gmtime(&now);
	strftime(buffer, sizeof(buffer), "%a, %d %b %G %T GMT", timeinfo);
	fields["Date"] = std::string(buffer); 
	fields["Expires"] = fields["Date"];
	//Server Version
	fields["Server"] = "Zach Traul's HTTP Server v1.0";
	//Caching stuff
	fields["Cache-Control"] = "no-cache";
	fields["Content-Lanugage"] = "en";
}
//Used when parsing a request
HTTPHeader::HTTPHeader(std::string header){
	//Parse and store the header if we are given one to parse
	if (header.length() == 0) return;
	for(int i = 0; i < field_strings.size(); i++){
		//Check if the field exists
		size_t f = header.find(field_strings[i]);
		if(f != std::string::npos) {
			//Find the offset of the beginning of it, if it exists
			size_t beg = header.substr(f).find(": ") + 2;
			//Find the length of it, if it exists
			size_t len = header.substr(f + beg).find("\r\n");
			//Add it to the fields map.
			if (beg != std::string::npos && len != std::string::npos){ 
				fields[field_strings[i]] = header.substr(f + beg, len);
			}
		}
	}	
}	
//Returns a valid HTTP/1.0 header as a string
std::string HTTPHeader::to_str(){
	std::string out;
	for(auto const& field: fields){
		out += field.first + ": " + field.second + "\r\n";
	}
	out += "\r\n";
	length = out.length();
	return out;
}

//HTTPMessage Constructors
//When we are crafting a response
HTTPMessage::HTTPMessage(std::string version, std::string status_code, std::string reason_phrase){
	//Update the response line
	response_line.reassign(version, status_code, reason_phrase);
}	
//When we are parsing a request
HTTPMessage::HTTPMessage(std::string message){
	//Break it into request/response and header
	size_t CRLF = message.find("\r\n");
	//Prevent overflow
	if(CRLF != std::string::npos && CRLF + 2 > CRLF){
		request_line = HTTPRequestLine(message.substr(0, CRLF + 2).c_str());
		header = HTTPHeader(message.substr(CRLF + 2).c_str());	
		valid = request_line.valid;
		//If there was not a host header field it was not valid.
		if(header.fields.find("Host") == header.fields.end()) valid = false;
	} else valid = false;
}	
//Write the message to the file descriptor given by connection
void HTTPMessage::write_message(int connection, std::string method){
	//So we can log() it
	length = response_line.to_str().length() + header.to_str().length();
	//Write
	write(connection, response_line.to_str().c_str(), response_line.to_str().length());
	write(connection, header.to_str().c_str(), header.to_str().length());
	//Only transfer a message body if there is data to be sent and the method is GET 
	if(entity_body.length > 0 && method == "GET"){
		write(connection, entity_body.body, entity_body.length);
		delete[] entity_body.body;
		//Again, for log()
		length += entity_body.length;
	}
}

