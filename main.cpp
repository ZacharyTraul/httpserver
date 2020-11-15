#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <map>
#include <filesystem>
#include <ctime>
#include <sys/stat.h>
#include <thread>
#include <mutex>
/* TODO: 
 * Seperate stuff out into at least a few different files
 * Start making some webpages
 * 	Make "rendering" engine. i.e. I can just have a folder full of posts and
 * 	everything else in the blog "app" will fill in content based on that
 * A visitor counter would be pretty cool
 * Pages
 * 	Main landing page
 * 	Blog Page
 * 		Main Page: Title and short description of each post, tiled. Option to sort by category, date, or search by name (filter based)
 * 		Individual Posts: Title description and body (can include pictures, links, etc)
 * 		File structure:
 *		/blog
 *			maintemplate.html---------------------------------------|Vertical list of all posts
 *			individualtemplate.html---------------------------------|Just nicely displays one post
 *			style.css
 *			posts/
 *				post1.html					|Format: <h3>title</h3>
 *				post2.html -------------------------------------+        <p>Short description</p>
 *				post3.html					|	 <p>body with <a>links</a> and images: <img/> or whatever</p>
 *			media/
 *				post1pic.pic
 *				post2pic.pic
 *
 * 	About/Contact
 * 	Cycling Project
 * 	Utilities/Games/Whatever else
 * Get domain.
 */

const int port = 55555;
const std::vector<std::string> methods{"OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT"};
const std::vector<std::string> field_strings{
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
std::map<std::string, std::string> endings = {{".css", "text/css"}, {".js", "application/javascript"}, {".html", "text/html"},
						   {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
						   {".gif", "image/gif"}, {".ico", "image/x-icon"}};
std::mutex mtx;
struct timeval tv;
//Time in seconds.
const int timeout = 15;

std::string get_file_type(std::string path){
	size_t period = path.find(".");
	if(period == std::string::npos){
	       	std::cout << "No Period" << std::endl;
		return "NOTSUPPORTED";
	}
	std::string ending = path.substr(period);
	if(endings.find(ending) != endings.end()) return endings[ending];
	return "NOTSUPPORTED";
}

class HTTPEntityBody{
	public:
		size_t length = 0;
		char * body;

		void from_file(std::string path){
			std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
			if(file.is_open()){
				length = file.tellg();
				body = new char [length];
				file.seekg(0, std::ios::beg);
				file.read(body, length);
				file.close();
			} 
		}

		void get_length(std::string path){
			std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
			if(file.is_open()){
				length = file.tellg();
				file.close();
			}
		}
};

class HTTPResponseLine{
	public:
		std::string version;
		std::string status_code;
		std::string reason_phrase;

		size_t length;
		
		HTTPResponseLine(){return;}
		HTTPResponseLine(std::string v,std::string s,std::string r){
			version = v;
			status_code = s;
			reason_phrase = r;
		}

		std::string to_str(){
			std::string out = version + " " + status_code + " " + reason_phrase + "\r\n";
			length = out.length();	
			return out; 
		}
};

class HTTPRequestLine{
	public:
		std::string method;
		std::string uri;
		std::string version;

		bool valid = false;
	
		HTTPRequestLine(){return;};	
		HTTPRequestLine(std::string request){
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
						//if(uri == "/") uri += "index/index.html";
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

		std::string to_str(){
			return method + " " + uri + " " + version + "\r\n";
		}
};

class HTTPHeader{
	public:
		std::map<std::string, std::string> fields;

		size_t length;

		HTTPHeader(){
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
		HTTPHeader(std::string header){
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
		
		std::string to_str(){
			std::string out;
			for(auto const& field: fields){
				out += field.first + ": " + field.second + "\r\n";
			}
			out += "\r\n";
			length = out.length();
			return out;
		}
};

class HTTPMessage{
	public:
		HTTPRequestLine request_line;
		HTTPResponseLine response_line; 
		HTTPHeader header;
		HTTPEntityBody entity_body;

		bool valid;
		size_t length;
		
		HTTPMessage(){return;}

		//When we want to make a response
		HTTPMessage(std::string version, std::string status_code, std::string reason_phrase){
			//Update the response line
			response_line.version = version;
		       	response_line.status_code = status_code;
			response_line.reason_phrase = reason_phrase;	
		}	
		
		//When we want to parse the message from a request
		HTTPMessage(std::string message){
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

		//Change the status code easily
		void reassign(std::string status_code, std::string reason_phrase){
			response_line.status_code = status_code;
			response_line.reason_phrase = reason_phrase;
		}

		void write_message(int connection, std::string method){
			//So we can log() it
			length = response_line.to_str().length() + header.to_str().length();

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
};

void log(HTTPMessage * request, HTTPMessage * response, int connection){
	mtx.lock();
	
	//Get ip address we are sending to
	sockaddr_in sin;
	int len = sizeof(struct sockaddr_in);
	getpeername(connection, (sockaddr*)&sin, &len);
	
	//Get the time
	time_t now = time(NULL);
	struct tm * timeinfo;
	char time_buffer[29];
	timeinfo = localtime(&now);
	strftime(time_buffer, sizeof(time_buffer), "[%d/%b/%Y:%H:%M:%S %z]", timeinfo);
	
	std::ofstream logfile;
	logfile.open("log.txt", std::ios_base::app);
	//Ugliest line ever
	std::string line;
	if(request != nullptr) line = std::string(inet_ntoa(sin.sin_addr)) + " - - " + time_buffer + " \""+ request->request_line.method + " " + request->request_line.uri + " " + request->request_line.version + "\" " + response->response_line.status_code + " " + std::to_string(response->length) + "\n";
	//This is just the case where we send a timeout response.
	else line = std::string(inet_ntoa(sin.sin_addr)) + " - - " + time_buffer + " \""+ "TIMEDOUT"  + "\" " + response->response_line.status_code + " " + std::to_string(response->length) + "\n";

	logfile << line;
	logfile.close();
	mtx.unlock();
}

//Decides what to do with the request and responds to it. 
//Returns true if Connection: keep-alive
//Returns false otherwise
bool dispatch(std::string data, int connection){
	//If we have timed out the connection
	if(data == "TIMEDOUT"){
		HTTPMessage timeout("HTTP/1.1", "408", "Request Timeout");
		timeout.header.fields["Content-Length"] = "0";
		timeout.header.fields["Connection"] = "close";
		timeout.write_message(connection, "HEAD");
		log(nullptr, &timeout, connection);
		return false;
	} else if(data == "CLOSED"){
		return false;
	}
	
	//If everything is proceeding as usual
	HTTPMessage message(data);
	HTTPMessage response("HTTP/1.1", "200", "OK");
	response.header.fields["Connection"] = message.header.fields["Connection"];

	if(message.request_line.uri == "/") message.request_line.uri = "/index/index.html";

	//Decide what to do.
	//We can't parse the request
	if(!message.valid){
		response.reassign("400", "Bad Request");
		response.header.fields["Content-Length"] = "0";
	}
	///Method not supported	
	else if (message.request_line.method != "GET" && message.request_line.method != "HEAD"){
		response.reassign("405", "Method Not Allowed");
		response.header.fields["Allow"] = "GET, HEAD";
		response.header.fields["Content-Length"] = "0";
	}
	
	//Version not supported
	else if (message.request_line.version != "HTTP/1.1"){
		response.reassign("505", "HTTP Version not supported");
		response.header.fields["Content-Length"] = "0";
	}
	//Attempt to give them the file they asked for
	else if (std::filesystem::exists("src" + message.request_line.uri) && std::filesystem::is_regular_file("src" + message.request_line.uri)){
		//Check if what we intend to serve back is what was requested
		bool acceptable = true;
		//If there is an accept header
		//This could probably be rewritten to be more efficient
		if(message.header.fields.find("Accept") != message.header.fields.end()){
			std::string mime_type = get_file_type(message.request_line.uri);
			std::string accept = message.header.fields["Accept"];
			//Most restrictive test to least restrictive
			if(accept.find(mime_type) == std::string::npos){
				//We couldn't find the type we intended to give back
				response.reassign("406", "Not Acceptable");
				acceptable = false;
			}
			//But maybe it had wildcards
			size_t slash = mime_type.find("/");
			if(slash == std::string::npos || slash == 0) std::cout << "ERROR SLASH" << std::endl;
			if(accept.find(mime_type.substr(0, slash + 1) + "*") != std::string::npos){
				response.reassign("200", "OK");
				acceptable = true;
			}
			//Maybe it really had wildcards
			else if(accept.find("*/*") != std::string::npos){
				response.reassign("200", "OK");
				acceptable = true;
			}
		}
		
		//We are all good to proceed
		if(acceptable){
			//We don't do accept-ranges yet.
			if(message.header.fields.find("Accept-Ranges") != message.header.fields.end()){
			       	response.header.fields["Accept-Ranges"] = "none";
			}
			//Set the content-type
			response.header.fields["Content-Type"] = get_file_type(message.request_line.uri);
			//Set the Last-Modified header
			struct stat fileInfo;
		        stat(("src" + message.request_line.uri).c_str(), &fileInfo);
       		 	time_t mod;
       			struct tm * timeinfo;
		        char buffer[30];
			//std::cout << fileInfo.st_ctime;
       			timeinfo = gmtime(&fileInfo.st_ctime);
			strftime(buffer, sizeof(buffer), "%a, %d %b %G %T GMT", timeinfo);
			response.header.fields["Last-Modified"] = std::string(buffer);
			
			//If the request was HEAD, we don't actually want to load the file into memory, only get its size
			if(message.request_line.method == "HEAD") response.entity_body.get_length("src" + message.request_line.uri);
			else response.entity_body.from_file("src" + message.request_line.uri);
			response.header.fields["Content-Length"] = std::to_string(response.entity_body.length);
		}
	} 
	//We couldn't find the file 
	else {
		response.reassign("404", "Not Found");
		response.header.fields["Content-Length"] = "0";
	}
	//Respond to it
	if(message.request_line.method == "GET") response.write_message(connection, "GET");
	else response.write_message(connection, "HEAD");
	//Log it
	log(&message, &response, connection);
	//If request asks to keep talking then don't close the connection. 
	if(message.header.fields["Connection"] == "keep-alive" || message.header.fields["Connection"] == "Keep-Alive"){
		return true;
	}
	return false;
}

std::string read_request(int connection){
	//Set up for select
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(connection, &rfds);
	int timedout = select(connection + 1, &rfds, NULL, NULL, &tv);
	if(timedout == -1){
		std::cout << "Error with select: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}
	else if(timedout == 0) return "TIMEDOUT";
	else {
		//Read from the connection
		const size_t bufsize = 8000;
		char buffer[bufsize];
		if(read(connection, buffer, bufsize) < 0){
			//This is probaby a bad way to handle the error. 
			//Connection reset by peer
			if(errno = 104){
				std::cout << "ERROR CLOSED" << std::endl;
				return "CLOSED";
			}
			std::cout << "Error reading from connection: " << errno << std::endl;
			exit(EXIT_FAILURE);
		}
		std::string data(buffer);
		return data;
	}
}

void handle_request(int connection){
	//While they keep sending us data, we keep talking to them.
	while(true){
		if(!dispatch(read_request(connection), connection)) break;
	}
	close(connection);
}

void shell(bool * run){
	std::string command;
	while(true){
		std::cout << "Enter Command: " << std::endl;
		std::cin >> command; 
		if(command == "STOP"){
			*run = false;
			return;
		}
	}
}

int main(){
	//Create the socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		std::cout << "Failed to create socket. Errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}
	
	//Bind the port
	sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(port);
	if ( bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
		std::cout << "Failed to bind to port. Errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}

	//Start listening on the port
	if (listen(sockfd, 10) < 0){
		std::cout << "Failed to listen. Errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}

	bool run = true;
	std::thread(shell, &run).detach();
	while(run){
		//Get a connection from the queue
		auto addrlen = sizeof(sockaddr);
		int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);
		if (connection < 0) {
			std::cout << "Failed to grab connection. Errno: " << errno << std::endl;
			exit(EXIT_FAILURE);
		}
		//Run it on a thread
		(std::thread (handle_request, connection)).detach();
	}
	close(sockfd);
}
