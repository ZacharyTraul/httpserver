#include "view.h"

std::map<std::string, std::string> end_to_mime = {
	{".css", "text/css"}, {".js", "application/javascript"}, {".html", "text/html"},
	{".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
	{".gif", "image/gif"}, {".ico", "image/x-icon"}
};


//###########################################################################
//Loads the data and length of a file into the binary_file_data struct.
//Data must be delete[]'d at some point, which should occur in write_message()
//On error returns empty struct.
binary_file_data View::get_binary_file(std::string path){
	binary_file_data out;
	std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
	if(file.is_open()){
		out.length = file.tellg();
		out.data = new char [out.length];
		file.seekg(0, std::ios::beg);
		file.read(out.data, out.length);
		file.close();
		return out;
	}	
	return out;
}

//Returns the text file at path as an std::string. On error returns
//empty string.
std::string get_text_file(std::string path){
	std::string out;
	std::string line;
	std::ifstream file (path);
	if(file.is_open()){
		while(getline(file, line)){
			out += line + "\n";
		}
	}
	return out;
}

//Returns the mime type of a file based on its file ending.
//On error returns an empty string.
std::string View::mime_from_path(std::string path){
	std::string output;
	size_t period = path.find(".");
	if(period == std::string::npos) return output;
	std::string ending = path.substr(period);
	if(!end_to_mime[ending].empty()) return end_to_mime[ending];
	return output;
}

//Returns the properly formatted last-modifed header as a string.
//On error returns and empty string.
std::string View::last_modified_from_path(std::string path){
	struct stat fileInfo;
	stat(path.c_str(), &fileInfo);
	tm * timeinfo;
	char buffer[30];
	timeinfo = gmtime(&fileInfo.st_ctime);
	strftime(buffer, sizeof(buffer), "%a, %d %b %G %T GMT", timeinfo);
	return std::string(buffer); 
}

//Takes the uri and decides what view the uri goes with. 
View* View::Create(std::string u){
	return new Simple(u);	
	/*
	if     (u.find("/index")   == 0)	return new Main(u);
	else if(u.find("/blog")    == 0) 	return new Blog(u);
	else if(u.find("/about")   == 0) 	return new About(u);
	else if(u.find("/cycling") == 0) 	return new Cycling(u);
	else if(u.find("/other")   == 0) 	return new Other(u);
	else					return new NotFound(u);
	*/
}

//###########################################################################
//Simple View 
Simple::Simple(std::string u){
	uri = u;
}
//For now just returns the requested asset.
entity_data Simple::generate(){
	entity_data output;
	binary_file_data bin_data = get_binary_file("src" + uri);
	if(bin_data.length){
		output.asset = bin_data.data; 
		output.length = bin_data.length; 
		output.mime_type = View::mime_from_path("src" + uri);
		output.last_modified = View::last_modified_from_path("src" + uri);
		return output; 
	}
	output.notfound = true;
	return output;
}
