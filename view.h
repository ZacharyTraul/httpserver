#ifndef VIEW_H
#define VIEW_H
#include <iostream>
#include <fstream>   //Reading files.
#include <string>    //Working with strings.
#include <filesystem>//Working with files.
#include <sys/stat.h>//Getting modify times of files.
#include <ctime>     //Working with times
#include <map>	     //For maps.

extern std::map<std::string, std::string> end_to_mime;

struct binary_file_data {
	char * data;
	size_t length = 0;
};

struct entity_data {
	char * asset;
	size_t length;
	std::string mime_type;
	std::string last_modified;
	bool notfound = false;
};

class View {
	protected:
		std::string uri;
		static binary_file_data get_binary_file(std::string path);
		std::string get_text_file(std::string path);
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
#endif
