#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include <fstream>
#include <ctime>
#include <time.h>
#include <thread>
#include <mutex>
#include "httpmessage.h"
#include "view.h"

/* TODO: 
 * Make caching a thing.
 * Home Page
 * Blog
 * Utilities/Games/Whatever else
 * Get domain.
 */

const int port = 55555;
std::map<std::string, std::string> etags;
std::map<std::string, std::string> modified;

std::mutex mtx;
struct timeval tv;
//Time in seconds.
const int timeout = 15;

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

	//Decide what to do.
	//We can't parse the request.
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
	//If the etags match
	else if(!message.header.fields["If-None-Match"].empty() && etags[message.request_line.uri] == message.header.fields["If-None-Match"]){
		response.response_line.reassign("HTTP/1.1", "304", "Not Modified");
		response.header.fields["Content-Length"] = "0";
		response.header.fields["ETag"] = etags[message.request_line.uri];
		response.header.fields["Last-Modified"] = modified[message.request_line.uri];
	}
	//Attempt to give them the file they asked for
	else {
		//Generate the file
		View * view;
		view = View::Create(message.request_line.uri);
		entity_data entity = view->generate();
		//404 if there was nothing to serve back.
		if(!entity.notfound){
			//Etag and Caching stuff
			etags[message.request_line.uri] = entity.etag;
			response.header.fields["ETag"] = entity.etag;
			response.header.fields["Cache-Control"] = entity.cache_control;
			modified[message.request_line.uri] = entity.last_modified;

			//Set the body and mime type
			response.entity_body = entity.asset;
			std::string mime = entity.mime_type;
			
			//Check if acceptable
			bool acceptable = false;
			if(!message.header.fields["Accept"].empty()){
				std::string accept = message.header.fields["Accept"];
				if(accept.find("*/*") != std::string::npos) acceptable = true; //Wildcard
				else if(accept.find(mime.substr(0, mime.find("/")) + "/*") != std::string::npos) acceptable = true; //Partial Wildcard
				else if(accept.find(mime) != std::string::npos) acceptable = true; //Matches exactly
			} else acceptable = true;

			//It is
			if(acceptable){
				if(!message.header.fields["Accept-Ranges"].empty())
					response.header.fields["Accept-Ranges"] = "none"; //Accept-Ranges not yet implemented.
				response.header.fields["Content-Type"] = mime; //Set Content-Type
				response.header.fields["Last-Modified"] = entity.last_modified; //Set Last-Modified
				response.header.fields["Content-Length"] = std::to_string(entity.asset.size()); //Set Content-Length
			} else {
				response.response_line.reassign("HTTP/1.1", "406", "Not Acceptable");

			}
		//It was not found.
		} else {
			response.response_line.reassign("HTTP/1.1", "404", "Not Found");
			response.header.fields["Content-Length"] = "0";
		}
		delete view;
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
			//Connection reset by peer. Probably a better way to handle this.
			if(errno = 104){
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
