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
std::vector<char> View::get_binary_file(std::string path){
	std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
	if(file.is_open() && std::filesystem::is_regular_file(path)){
		char * temp;
		size_t length = file.tellg();
		temp = new char [length];
		file.seekg(0, std::ios::beg);
		file.read(temp, length);
		file.close();
		std::vector<char> out(temp, temp + length);
		delete[] temp;
		return out;
	}	
	std::vector<char> out;
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
	if(u == "/") return new Main(u);
	else if(u == "/about") return new About(u);
	else if(u == "/blog") return new Blog(u);
	else return new Simple(u);	

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
//Just returns the requested asset.
entity_data Simple::generate(){
	entity_data output;
	std::vector<char> bin_data = View::get_binary_file("src" + uri);
	if(bin_data.size()){
		output.asset = bin_data; 
		output.mime_type = View::mime_from_path("src" + uri);
		output.last_modified = View::last_modified_from_path("src" + uri);
		output.cache_control = "max-age=84600, public";
		std::string asset_str(output.asset.begin(), output.asset.end());
		output.etag = std::to_string(std::hash<std::string>{}(asset_str));	
		return output; 
	}
	output.notfound = true;
	output.mime_type = "";
	output.last_modified = "";

	return output;
}
//############################################################################
//Main Page
Main::Main(std::string u){
	uri = u;
}

entity_data Main::generate(){
	entity_data output;
	template_args vars;
	vars.import_vars["content"] = "src/home/home-main.html";
	HTMLTemplate temp;
	output.asset = temp.process_template("src/templates/main-template.html", vars);
	output.mime_type = "text/html";
	output.last_modified = View::last_modified_from_path("src/home/home-main.html");
	return output;
}

//############################################################################
//About Page
About::About(std::string u){
	uri = u;
}

entity_data About::generate(){
	entity_data output;
	template_args vars;
	vars.import_vars["content"] = "src/about/about-content.html";
	HTMLTemplate temp;
	output.asset = temp.process_template("src/templates/main-template.html", vars);
	output.mime_type = "text/html";
	output.last_modified = View::last_modified_from_path("src/about/about-content.html");
	return output;
}

//##############################################################################
//Blog Page
Blog::Blog(std::string u){
	uri = u;
}

entity_data Blog::generate(){
	entity_data output;
	template_args vars;
	vars.import_vars["content"] = "src/blog/blog-content.html";
	//Get all blog posts.
	for(const auto & entry : std::filesystem::directory_iterator("src/blog/posts")){
		std::string path = entry.path();
		path = path.substr(3);
		vars.loop_vars["path"].push_back(path);
	}
	HTMLTemplate temp;
	output.asset = temp.process_template("src/templates/main-template.html", vars);
	output.mime_type = "text/html";
	output.last_modified = View::last_modified_from_path("src/blog/blog-content.html");
	return output;
}
