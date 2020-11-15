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
#include "httpmessage.h"
/* TODO: 
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
		response.response_line.reassign("HTTP/1.1", "400", "Bad Request");
		response.header.fields["Content-Length"] = "0";
	}
	///Method not supported	
	else if (message.request_line.method != "GET" && message.request_line.method != "HEAD"){
		response.response_line.reassign("HTTP/1.1", "405", "Method Not Allowed");
		response.header.fields["Allow"] = "GET, HEAD";
		response.header.fields["Content-Length"] = "0";
	}
	
	//Version not supported
	else if (message.request_line.version != "HTTP/1.1"){
		response.response_line.reassign("HTTP/1.1", "505", "HTTP Version not supported");
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
				response.response_line.reassign("HTTP/1.1", "406", "Not Acceptable");
				acceptable = false;
			}
			//But maybe it had wildcards
			size_t slash = mime_type.find("/");
			if(slash == std::string::npos || slash == 0) std::cout << "ERROR SLASH" << std::endl;
			if(accept.find(mime_type.substr(0, slash + 1) + "*") != std::string::npos){
				response.response_line.reassign("HTTP/1.1", "200", "OK");
				acceptable = true;
			}
			//Maybe it really had wildcards
			else if(accept.find("*/*") != std::string::npos){
				response.response_line.reassign("HTTP/1.1", "200", "OK");
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
			if(message.request_line.method == "HEAD") response.entity_body.calc_length("src" + message.request_line.uri);
			else response.entity_body.from_file("src" + message.request_line.uri);
			response.header.fields["Content-Length"] = std::to_string(response.entity_body.length);
		}
	} 
	//We couldn't find the file 
	else {
		response.response_line.reassign("HTTP/1.1", "404", "Not Found");
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
