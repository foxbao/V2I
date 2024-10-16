/** @file evloop.h
 * definition of evloop impl 
 *  */

#ifndef __CXX_ZAS_UTILS_DATA_POOL_IMPL_H__
#define __CXX_ZAS_UTILS_DATA_POOL_IMPL_H__

#include <string>
#include "std/list.h"
#include "utils/datapool.h"
#include "utils/evloop.h"
#include "utils/avltree.h"
#include "utils/mutex.h"
#include "utils/wait.h"
#include "dpmsg.h"

namespace zas {
namespace utils {


struct dp_evl_msg_retinfo
{
	int result;
	std::string name;
	std::string data;
};

/**
 * @brief element storage node
 * storage content:
 * name, flags, data, noifier
 */
class pond_element
{
public:
    pond_element(const char* name, bool needpersistent,
        const char* creator);
    ~pond_element();

    int setdata(void* buffer, size_t sz);
    int getdata(std::string &data);
    int notify(void* buffer, size_t sz);
    int add_listener(notifier notify, void* owner);
    int remove_listener(notifier notify, void* owner);
    int add_listener(notifier notify, void* owner, evlclient client);

    size_t getdatasize(void);
    const char* getdatabuf(void);
    bool get_need_persistent(void);
private:
    void release_all_notifier(void);
public:
    // this node is for avltree or list of pond
    avl_node_t avlnode;
    listnode_t  _listowner;
    std::string _element_name;
    std::string _creator_name;

private:
    struct element_listener
    {
        //notifier list node
        listnode_t  ownerlist;
        notifier    notify;
        void*       owner;
        evlclient   client;
    };
    
private: 
    bool            _need_persistent;
    //element storage data
    std::string     *_element_data;
    //element notifier list;
    listnode_t      _notify_list;

};

class pond
{
public:
    pond();
    ~pond();

public:
    /**
     * @brief Create a element node object
     * @param  name             element name
     * @param  need_persistent  be need persistent
     * @return pond_element*    NULL : create error
     */
    pond_element* create_element_node(const char* name, 
        bool need_persistenet, const char* creator = NULL);

    /**
     * @brief find exist element node unlocked
     * @param  name             element name
     * @return pond_element*    NULL : not find
     */
    pond_element* find_element_node(const char* name);
    int remove_pond_element(const char* name, 
        const char* remover = NULL);
private:

    pond_element* find_element_node_unlocked(const char* name);
    int remove_pond_element_unlocked(const char* name,
        const char* remover = NULL);
    //TODO: serilize storage when elemnet is persistent

public:
    avl_node_t      avlnode;
    listnode_t      _listowner;
    std::string     _pond_name;

private:
    static int pond_element_compare(avl_node_t* a, avl_node_t* b);
    void release_all_node(void);

private:
    // pond elemnet tree and list;
    avl_node_t*     _element_tree;
    listnode_t      _element_list;
    mutex           _mut;
};

//datapool impl
class pond_manager
{
public:
    pond_manager();
    ~pond_manager();

public:
    pond* createpond(const char* pond);
    pond* find_pond(const char* pond);
    int remove_pond(const char* name);

private:
    static int pondmanager_pond_compare(avl_node_t* a, avl_node_t* b);
	void release_all_nodes(void);
	pond* find_pond_unlocked(const char* name);

public:   
    //when evloop is server
    //_pond_tree is global-local pond, 
    //_pond_list is global-local list
    avl_node_t*     _pond_tree;
    listnode_t      _pond_list;

private:
	mutex           _mut;
};

class datapool_impl;

/**
 * @brief real fucntion of datapool_element_object
 */
class datapool_element_impl
{
public:
    datapool_element_impl(datapool_impl* datapool, 
        const char* name, 
        bool is_global, 
        bool need_persistent);
    ~datapool_element_impl();
    
    int addref(void);
    int release(void);

    int setdata(const char* str);
    int setdata(const std::string &str);
    int setdata(void* buffer, size_t sz);
    int getdata(std::string &data);
    int notify(const char* str);
    int notify(const std::string &str);
    int notify(void* buffer, size_t sz);   
    int add_listener(notifier notify, void* owner);
    int add_listener(datapool_listener* lnr);
    int remove_listener(notifier notify, void* owner);
    int remove_listener(datapool_listener* lnr);

private:
    bool is_global(){
        return (1 == _f.is_global);
    }
    bool need_persistent(){
        return (1 == _f.need_persistent);
    }

private:
    union {
        uint32_t _flags;
        struct {
            uint32_t is_global : 1;
            uint32_t need_persistent : 1;
        }_f;
    };

private:
    std::string     _element_name;
    datapool_impl*  _datapool;
    int             _refcnt;
};

class datapool_impl
{
public:
	datapool_impl();
	~datapool_impl();

public:
    datapool_element create_element(const char* name,
        bool is_global = false,
        bool need_persistent = false);

    datapool_element get_element(const char* name,
        bool is_global = false);
        
    int remove_element(const char* name,
        bool is_global = false);

public:
    /**
     * @brief setdata by name,global,persistent
     * it will be called by datapool_element_impl
     */
    int setdata(const char* name, bool is_global,
        bool need_persistent, void* buffer, size_t sz);
    int getdata(const char* name, bool is_global,
        bool need_persistent, std::string &data);
    int notify(const char* name, bool is_global,
        bool need_persistent, void* buffer, size_t sz);
    int add_listener(const char* name, bool is_global,
        bool need_persistent, notifier notify, void* owner);
    int remove_listener(const char* name, bool is_global,
        bool need_persistent, notifier notify, void* owner);

private:
    class package_listener: public evloop_pkglistener
    {
        public:
            package_listener();
            int set_datapool_impl(datapool_impl* impl);
            bool on_package(evlclient sender,
                const package_header& pkghdr,
                const triggered_pkgevent_queue& queue);
        private: 
            datapool_impl* _dp_impl;
    } _pkg_lnr;

    struct element_base_info{
        element_base_info(const char* nm,
            bool isglobal, bool need_persistent);
        std::string     name;
        bool            is_global;
        bool            need_persistent;
    };
    
private:
    bool issvr(void) { 
        return (_f.is_srv == 1);
    }

    bool is_init(void) {
        return (_f.is_init == 1);
    }
    bool init_datapool(void);
    int request_global(void);

private:

    // local function
    pond* find_create_pond(const char* pondname);
    pond_element* find_srv_element(const char* name, bool is_global);
    datapool_element_impl* create_pond_element(pond *pondnode,
        element_base_info *info, const char* creator = NULL);
    datapool_element_impl* get_pond_element(pond *pondnode, 
        element_base_info *info);

    // remote request fucntion
    datapool_element_impl* 
        create_remote_element_unlock(element_base_info *info);
    datapool_element_impl* 
        get_remote_element_unlocked(element_base_info *info);
    int remove_remote_element_unlocked(element_base_info *info);
    int check_server_element_exist(element_base_info *info);

    // evl remote request 
    int remote_evl_request(int operation, int action, 
        bool reply, element_base_info *baseinfo, dp_evl_msg_retinfo* retinfo,
        notifier ele_notify = NULL, void* owner = NULL,
        void* buf = NULL, size_t bufsz = 0);

    // package handle
    bool on_evl_package(evlclient client, const package_header& pkghdr,
        const triggered_pkgevent_queue& queue);
    bool handle_evl_client_request_pkg(evlclient sender,
        const package_header& pkghdr,
        const triggered_pkgevent_queue& queue);
    bool handle_evl_server_reply_pkg(const package_header& pkghdr,
        const triggered_pkgevent_queue& queue);
    bool handle_evl_server_noitfy_pkg(const package_header& pkghdr,
        const triggered_pkgevent_queue& queue);
    bool handle_evl_client_dp_request(evlclient sender, uint32_t seqid,
        datapool_client_request* reqinfo);
    int create_evl_pond_element(const char* sendername, 
        element_base_info *info);
    int get_evl_pond_element(const char* sendername,
        element_base_info *info);
    int remove_evl_pond_element(const char* sendername,
        element_base_info *info);
    bool handle_evl_client_element_request(evlclient sender, uint32_t seqid,
        datapool_client_request* reqinfo);

private:
    union {
        uint32_t _flags;
        struct {
            uint32_t is_init : 1;
            uint32_t is_srv : 1;
        } _f;
    };

private:
    //_globalpond storage global element when evloop is svr
    // otherwise _globalpond is NULL
    pond*            _globalpond;
    //_pond_manager storage local-persistent element when evloop is svr
    // pond is created by clientname in pondmanager
    // otherwise _pond_manager is NULL
    pond_manager*   _pond_manager;
    //_localpond storage local element
    pond            _localpond;
	mutex _mut;

	ZAS_DISABLE_EVIL_CONSTRUCTOR(datapool_impl);
};

}} // end of namespace zas::utils
#endif /*  __CXX_ZAS_UTILS_EV_LOOP_IMPL_H__ */
/* EOF */
