#include <string.h>

#include "std/list.h"
#include "utils/json.h"
#include "utils/mutex.h"
#include "utils/avltree.h"

#define USERINFO_USER_MAX_GROUP (32)
#define USERINFO_USER_MAX_PATH  (4)

using namespace std;
using namespace zas::utils;

namespace zas {
namespace system {

struct appinfo
{
	string package_name;
	uid_t uid;
	uint32_t gid_count;
	gid_t gids[USERINFO_USER_MAX_GROUP];
	string data_path;
	bool	sysapp;
};

class appinfo_mgr
{
public: 
	appinfo_mgr();
	~appinfo_mgr();

public:
	int init();

	/**
	  Get the appinfo and lock this app (cannot be uninstalled)
	  if necessary
	  @param pkg_name the name of the app package
	  @param info the info object, data will be copied to this object
	  @param lock see if we need to lock the appinfo, once it is locked
	  		the app could not be uninstalled until it unlocked
	  @return 0 for success
	 */
	int get_appinfo(const char* pkg_name, appinfo* info, bool lock = false);

private:
	class appinfo_node
	{
		friend class appinfo_mgr;
	public:
		appinfo_node(const char* name, jsonobject& jobj, bool bsysapp);
		~appinfo_node();

	private:
		avl_node_t _avlnode;
		listnode_t _ownerlist;
		union {
			uint32_t _flags;
			struct {
				uint32_t is_sysapp : 1;
				uint32_t loaded : 1;
				uint32_t locked : 1;
			} _f;
		};

		const char* _package_name;
		uid_t _uid;

		// gids: there is at most USERINFO_USER_MAX_GROUP's gids
		int _gids_count;
		gid_t _gids[USERINFO_USER_MAX_GROUP];

		// app data directory path
		const char* _data_path;
		ZAS_DISABLE_EVIL_CONSTRUCTOR(appinfo_node);
	};

	static int appinfo_node_compare(avl_node_t* a, avl_node_t* b);

private:
	void release_appinfo_tree(void);

	// load the appinfo from the jsonobject
	int load_appinfo(jsonobject& cfg, bool bsysapp);

	// load the appinfo node from the jsonobject
	int load_appinfo_node(jsonobject& jobj, bool bsysapp);

private:
	jsonobject* _sysapp_info;
	jsonobject* _userapp_info;
	
	listnode_t _appinfo_list;
	avl_node_t* _appinfo_tree;
	mutex _mut;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(appinfo_mgr);
};

}} // end of namespace zas::launch

