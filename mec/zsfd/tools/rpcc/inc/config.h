#ifndef __CXX_RPCC_CONFIG_H__
#define __CXX_RPCC_CONFIG_H__

#include "rpcc.h"
#include "list.h"
#include <string>
#include <stdio.h>
#include <cstring>

class global_config
{
public:
	global_config();
	virtual ~global_config();
	
	const char* GetOutputpath()
	{
		return m_cOutputPath;
	};
	
	const char* GetInputfile()
	{
		return m_cInputFile;
	};

	static global_config* Inst();
	
	bool SetOutputpath(char* pPath);
	
	bool SetInputfile(char* pFile);
	
private:
	char m_cOutputPath[PATH_LENGTH];//fatal: if change size, please check related memset funcs
	char m_cInputFile[PATH_LENGTH];//fatal: if change size, please check related memset funcs
};


using namespace std;

enum codegen_lang_type
{
	codegen_lang_type_unknown = 0,
	codegen_lang_type_cpp,
	codegen_lang_type_java,
	codegen_lang_type_js,
};

class config
{
public:
	config();
	~config();

public:

	class idl_srch_dir_node
	{
	public:
		list_item m_OwnerList;
		string m_Dir;
	};

	bool ParseArgs(int argc, char *argv[]);
	idl_srch_dir_node* parse_add_srchdir_from_canonicalized_filename(string cName);
	bool FindFileInConfiguratedDir(string& file);

public:
	string& GetInputFile(void) {
		return m_cInputFileFullName;
	}

	string& GetDescDir(void) {
		return m_cDestDir;
	}

	codegen_lang_type GetCodeGenType(void) {
		return m_eCodeGenType;
	}

	string& get_service_name(void) {
		return _service_name;
	}

	string& get_package_name(void) {
		return _package_name;
	}

private:
	bool ParseInputFile(const char *f);
	bool parse_dir(const char *d, string& cdir);
	idl_srch_dir_node* AppendSearchDir(string dir);

private:
	list_item m_SearchDirList;
	string m_cInputFileFullName;
	string m_cDestDir;
	string _package_name;
	string _service_name;
	codegen_lang_type m_eCodeGenType;
};

//config class

#endif