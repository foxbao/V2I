#include <assert.h>
#include "config.h"
#include "utilities.h"

config::config()
: m_eCodeGenType(codegen_lang_type_cpp)
{
}

config::~config()
{
}

bool config::parse_dir(const char *d, string& cdir)
{
	assert(NULL != d);

#ifndef WIN32
	if (!strcmp(d, "/"))
	{
		cdir.clear();
		return true;
	}
#endif

	if (*d == '\"') ++d;
	int d_len = strlen(d);

	if (d_len > 1 && d[d_len - 1] == '\"') --d_len;
#ifdef WIN32
	if (d_len > 1 && d[d_len - 1] == '\\') --d_len;
#else
	if (d_len > 1 && d[d_len - 1] == '/') --d_len;
#endif
	if (!d_len)
	{
		printf("Invalid directory specified.\n");
		return false;
	}

	char tmp[256];
	memcpy(tmp, d, d_len);
	tmp[d_len] = '\0';

	cdir = tmp;
	return true;
}

bool config::ParseInputFile(const char *f)
{
	if (!m_cInputFileFullName.empty())
	{
		printf("Argument: %s:\n"
			"source idl file already specified: \"%s\"\n",
			f, m_cInputFileFullName.c_str());
		return false;
	}

	assert(NULL != f);
	if (*f == '\"') ++f;
	int fn_len = strlen(f);

	if (fn_len > 1 && f[fn_len - 1] == '\"') --fn_len;
	if (!fn_len)
	{
		printf("Invalid input filename specified.\n");
		return false;
	}

	char tmp[256];
	memcpy(tmp, f, fn_len);
	tmp[fn_len] = '\0';

	m_cInputFileFullName = utilities::CanonicalizeFileName(tmp);
	if (m_cInputFileFullName.empty())
		return false;

	return true;
}

bool config::ParseArgs(int argc, char *argv[])
{
	if (argc <= 1)
		return false;

	bool bDestDirSet = false;
	config::idl_srch_dir_node* pNode = NULL;

	for (int i = 1; i < argc; ++i)
	{
		if (!strncmp(argv[i], "--output-dir=", 13))
		{
			if (!m_cDestDir.empty())
			{
				printf("Argument: %s:\n"
					"destination directory already set as \"%s\"\n",
					argv[i] + 13, m_cDestDir.c_str());
				return false;
			}
			if (!parse_dir(argv[i] + 13, m_cDestDir))
				return false;
			bDestDirSet = true;

#ifdef RPCC_DEBUG
			printf("destdir = %s\n", m_cDestDir.c_str());
#endif
		}
		else if (!strcmp(argv[i], "-o"))
		{
			if (!m_cDestDir.empty())
			{
				printf("Argument: %s:\n"
					"destination directory already set as \"%s\"\n",
					argv[i] + 13, m_cDestDir.c_str());
				return false;
			}
			if (!parse_dir(argv[++i], m_cDestDir))
				return false;
			bDestDirSet = true;

#ifdef RPCC_DEBUG
			printf("destdir = %s\n", m_cDestDir.c_str());
#endif
		}
		else if(!strncmp(argv[i], "--input-file=", 13))
		{
			if (!ParseInputFile(argv[i] + 13))
				return false;
			pNode = parse_add_srchdir_from_canonicalized_filename(m_cInputFileFullName);
			if (NULL == pNode) return false;

#ifdef RPCC_DEBUG
			printf("inputfile = %s\n", m_cInputFileFullName.c_str());
#endif
		}
		else if (!strncmp(argv[i], "-I", 2))
		{
			string includePath;
			if (!parse_dir(argv[i] + 2, includePath))
				return false;
			AppendSearchDir(includePath);

#ifdef RPCC_DEBUG
			printf("includepath = %s\n", includePath.c_str());
#endif
		}
		else if (!strcmp(argv[i], "-cpp") || !strcmp(argv[i], "--cpp"))
			m_eCodeGenType = codegen_lang_type_cpp;
		else if (!strcmp(argv[i], "-java") || !strcmp(argv[i], "--java"))
			m_eCodeGenType = codegen_lang_type_java;

		else if(!strncmp(argv[i], "--version", 9) )
		{
			printf("Current Version is: %s\n",VERSION);
			return false;
		}
		else if (!strncmp(argv[i], "--service-name=", 15)) {
			_service_name = argv[i] + 15;
		}
		else if (!strncmp(argv[i], "--package-name=", 15)) {
			_package_name = argv[i] + 15;
		}
		else
		{
			printf("%s\n", PRCC_HELP_COMMENT);

#ifdef RPCC_DEBUG
			printf("help\n");
#endif
			return false;
		}
	}

	if (!bDestDirSet) m_cDestDir = (pNode) ? pNode->m_Dir : ".";
	return true;
}

config::idl_srch_dir_node* config::AppendSearchDir(string dir)
{
	list_item *pItem = m_SearchDirList.getnext();
	for (; pItem != &m_SearchDirList; pItem = pItem->getnext())
	{
		config::idl_srch_dir_node *pNode = LIST_ENTRY(config::idl_srch_dir_node,\
			m_OwnerList, pItem);
		if (pNode->m_Dir == dir)
			return pNode;
	}

	config::idl_srch_dir_node *pNewNode = new config::idl_srch_dir_node;
	assert(NULL != pNewNode);

	pNewNode->m_Dir = dir;
	pNewNode->m_OwnerList.AddTo(m_SearchDirList);
	return pNewNode;
}

config::idl_srch_dir_node* config::parse_add_srchdir_from_canonicalized_filename(string cName)
{
#ifdef WIN32
	const int sep = '\\';
#else
	const int sep = '/';
#endif

	const char *end = NULL;
	const char *start = cName.c_str();
	const char *s = start;

	for (; *start; ++start)
		if (*start == sep) end = start;

	if (NULL == end) return NULL;
	
	string path;
	if (end - 1 >= s)
	{
		char buf[256];
		memcpy(buf, s, end - s);
		buf[end - s] = '\0';
		path = buf;
	}
	return AppendSearchDir(path);
}

bool config::FindFileInConfiguratedDir(string& file)
{
	if (utilities::IsFileExist(file.c_str()))
		return true;

	string sep;

#ifdef WIN32
	if (file.c_str()[0] != '\\')
		sep = "\\";
#else
	if (file.c_str()[0] != '/')
		sep = "/";
#endif

	string destfile;
	list_item *pItem = m_SearchDirList.getnext();
	for (; pItem != &m_SearchDirList; pItem = pItem->getnext())
	{
		config::idl_srch_dir_node *pNode = LIST_ENTRY(config::idl_srch_dir_node,\
			m_OwnerList, pItem);
		destfile = pNode->m_Dir + sep + file;
		if (utilities::IsFileExist(destfile.c_str()))
		{
			file = destfile;
			return true;
		}
	}
	return false;
}

static global_config s_GloablConfig;

global_config* global_config::Inst()
{
	return &s_GloablConfig;
}

global_config::global_config()
{
	memset(m_cOutputPath, 0, PATH_LENGTH);
	memset(m_cInputFile, 0, PATH_LENGTH);
#ifndef WIN32
	strcpy(m_cOutputPath,"./");
#else 
	strcpy(m_cOutputPath,".\\");
#endif
}

global_config::~global_config()
{
}

bool global_config::SetOutputpath(char* pPath){
	if (NULL == pPath)
		return false;
	strcpy(m_cOutputPath, pPath);
#ifndef WIN32
	if( m_cOutputPath[strlen(m_cOutputPath) - 1] !='/')
		strcat(m_cOutputPath,"/");
#else
	if( m_cOutputPath[strlen(m_cOutputPath) - 1] !='\\')
		strcat(m_cOutputPath,"\\");
#endif
	return true;
};

bool global_config::SetInputfile(char* pFile){
	if (NULL == pFile)
		return false;
	strcpy(m_cInputFile, pFile);
	return true;
};


