#ifndef VIEW_H
#define VIEW_H
#include <iostream>
#include <fstream>   //Reading files.
#include <string>    //Working with strings.
#include <vector>    //For files that aren't strings.
#include <filesystem>//Working with files.
#include <sys/stat.h>//Getting modify times of files.
#include <ctime>     //Working with times
#include <map>	     //For maps.
#include <functional>
#include "htmltemplate.h" //For templates

extern std::map<std::string, std::string> end_to_mime;

struct entity_data {
	std::vector<char> asset;
	std::string mime_type;
	std::string last_modified;
	std::string cache_control = "no-cache";
	std::string etag;
	bool notfound = false;
};

class View {
	protected:
		std::string uri;
		static std::vector<char> get_binary_file(std::string path);
		static std::string get_text_file(std::string path);
		static std::string mime_from_path(std::string path);
		static std::string last_modified_from_path(std::string path);
	public:
		virtual entity_data generate() = 0;
		static View* Create(std::string u);
};

class Simple: public View {
	public:
		entity_data generate();
		Simple(std::string u);
};

class Main: public View {
	public:
		entity_data generate();
		Main(std::string u);
};

class About: public View {
	public:
		entity_data generate();
		About(std::string u);
};

class Blog: public View {
	public:
		entity_data generate();
		Blog(std::string u);
};
#endif
