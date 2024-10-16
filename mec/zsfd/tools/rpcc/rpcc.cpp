#include <stdio.h>
#include <string>
#include <dirent.h> 
#include <sys/stat.h>  

#include "config.h"
#include "token.h"
#include "parser.h"
#include "utilities.h"
#include "error.h"

int generate_proto_file(config &cfg)
{
	DIR *d = NULL;
	struct dirent *dp = NULL;
	struct stat st;
	// proto path
	string path = cfg.GetDescDir().c_str();
	path = utilities::CanonicalizeFileName(path);
	
	if(cfg.GetCodeGenType() == codegen_lang_type_java) {
		path += "/src/main";
		
		string name;
		string cmd;
		string fullfile;
		string proto_path = path + "/proto";
		if(!(d = opendir(proto_path.c_str()))) {
			printf("opendir[%s] error: %m\n", proto_path.c_str());
			return -1;
		}
		while((dp = readdir(d)) != NULL) {
			if((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
				continue;
			// check file stat
			fullfile = proto_path + "/";
			fullfile += dp->d_name;
			stat(fullfile.c_str(), &st);
			// for file
			if(!S_ISDIR(st.st_mode)) {
				//generate protofile
				pid_t status;
				string prefixcmd = "protoc ";

				prefixcmd += path + "/proto/" + dp->d_name + " --proto_path=";
				prefixcmd += proto_path;
				prefixcmd += " --java_out=" + path + "/java";
				status = system(prefixcmd.c_str());
				if (-1 == status)
					printf("generate pro file error\n");
				status = system("sync");
			}
		}

	} else if (cfg.GetCodeGenType() == codegen_lang_type_cpp) {
		path += "/proto";

		string fullfile;
		string prefixcmd = "protoc -I=";
		prefixcmd += path + " --cpp_out=";
		prefixcmd += path + " ";
		// clean file
		string cmd;
		int result = 0;
		cmd = "rm -r ";
		cmd += path + "/*.cpp ";
		// cmd += path + "/*.cc ";
		cmd += path + "/*.h ";
		result = system(cmd.c_str());
		//output protoc version
		cmd = "protoc --version";
		result = system(cmd.c_str());
		result = system("sync");

		string name;
		if(!(d = opendir(path.c_str()))) {
			printf("opendir[%s] error: %m\n", path.c_str());
			return -1;
		}
		while((dp = readdir(d)) != NULL) {
			if((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
				continue;
			// check file stat
			fullfile = path + "/";
			fullfile += dp->d_name;
			stat(fullfile.c_str(), &st);
			// for file
			if(!S_ISDIR(st.st_mode)) {
				//generate protofile
				pid_t status;
				cmd = prefixcmd + dp->d_name;
				status = system(cmd.c_str());
				if (-1 == status)
					printf("generate pro file error\n");
				status = system("sync");

				// change file name
				int len = strlen(dp->d_name) -1;
				for (; len > 0; len--)
					if (dp->d_name[len] == '.')
						break;
				if (len <= 0) {
					printf("proto file name error\n");
					continue;
				}
				name.clear();
				name.append(dp->d_name, len);
				string scrfile = path + "/";
				scrfile += name + ".pro.pb.cc";
				string desfile = path + "/";
				desfile += name + ".pro.pb.cpp";
				utilities::rename(scrfile.c_str(), desfile.c_str());
			}
		}
		closedir(d);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc <= 1)
	{
		// show help
		printf("%s\n",PRCC_HELP_COMMENT);
		return 4;
	}
	
	config cfg;
	if (!cfg.ParseArgs(argc, argv))
		return 1;

	string cInputFile = cfg.GetInputFile();
	if (cInputFile.empty())
	{
		printf("Please specify the input idl file.\n");
		return 2;
	}
	if (!utilities::IsFileExist(cInputFile.c_str()))
	{
		printf("The input file \"%s\" not exists.\n", cInputFile.c_str());
		return 3;
	}

	srcfile_stack stack;
	stack.push(cInputFile);

	token_parser parser(stack);
	grammar_parser gmr_parser(parser, cfg);
	gmr_parser.Parse();

	generate_proto_file(cfg);

	//for debug
	int result;
	if(TErrorInfo::IsThereErrors())
		result = -1;
	else
		result = 0;
	return result;
}
