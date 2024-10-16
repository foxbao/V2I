#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_DATAPOOL

#ifndef __CXX_ZAS_UTILS_DATAPOOL_H__
#define __CXX_ZAS_UTILS_DATAPOOL_H__

#include <string>
#include "std/smtptr.h"
#include "utils/utils.h"

namespace zas {
namespace utils {

typedef void (*notifier)(void* owner, void* data, size_t sz);

zas_interface UTILS_EXPORT datapool_listener
{
    virtual void on_notify(void* data, size_t sz) = 0;
};

class UTILS_EXPORT datapool_element_object
{
public:
    datapool_element_object() = delete;
    ~datapool_element_object() = delete;
    
    int addref(void);
    int release(void);

    /**
     * @brief only set data for datapool_element
     * no notify when setdata.
     * @return int
     * @return int == 0          success
     * @return int != 0         error
     */

    /**
     * @brief set data for datapool_element
     * don't send notify when setdata.
     * @param  str              char* string data
     * @return int == 0          success
     * @return int != 0         error
     */
    int setdata(const char* str);

    /**
     * @brief set data for datapool_element
     * don't send notify when setdata.
     * @param  str              string data
     * @return int == 0          success
     * @return int != 0         error
     */
    int setdata(const std::string &str);

    /**
     * @brief set data for datapool_element
     * don't send notify when setdata.
     * @param  buffer           buffer data
     * @param  sz               buffer size
     * @return int == 0          success
     * @return int != 0         error
     */
    int setdata(void* buffer, size_t sz);
    
    /**
     * @brief get datapool_element data
     * @param  buffer           data buffer
     * @param  sz               buffer size
     * @return int == 0          success
     * @return int > 0          the `sz` of buffer is less than elementdata
     * @return int < 0          other error
     */
    int getdata(std::string &data);
    
    /**
     * @brief set data(str,buffer) to datapool_element data
     * send notify to noifier or datapool_listener
     * @param  str              char* string data
     * @return int == 0          success
     * @return int != 0         error
     */
    int notify(const char* str);

    /**
     * @brief set data(str,buffer) to datapool_element data
     * send notify to noifier or datapool_listener
     * @param  str              string data
     * @return int == 0          success
     * @return int != 0         error
     */
    int notify(const std::string &str);

    /**
     * @brief set data(str,buffer) to datapool_element data
     * send notify to noifier or datapool_listener
     * @param  buffer           buffer data
     * @param  sz               buffer size
     * @return int == 0          success
     * @return int != 0         error
     */
    int notify(void* buffer, size_t sz);
    
    /**
     * @brief add notifier to datapool_element
     * notifier will be call, when notify called
     * 'void* owner' will transfer to notifier
     * @param  notify           callback function
     * @param  owner            user data
     * @return int == 0          success
     * @return int != 0         error
     */
    int add_listener(notifier notify, void* owner);

    /**
     * @brief add listener to datapool_element
     * listener will be call, when notify called
     * @param  lnr              listener object point
     * @return int == 0          success
     * @return int != 0         error
     */
    int add_listener(datapool_listener* lnr);

    /**
     * @brief remove notifier from datapool_element
     * notifier & owner must be same as add_listener used
     * @param  notify           callback fucntion
     * @param  owner            user data
     * @return int == 0          success
     * @return int != 0         error
     */
    int remove_listener(notifier notify, void* owner);
    
    /**
     * @brief  remove listener from datapool_element
     * lnr must be same as add_listener used
     * @param  lnr              listener object
     * @return int == 0          success
     * @return int != 0         error
     */
    int remove_listener(datapool_listener* lnr);
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(datapool_element_object);
};

typedef zas::smtptr<datapool_element_object> datapool_element;

class UTILS_EXPORT datapool
{
public:
    datapool() = delete;
    ~datapool() = delete;
    
    /**
     * @brief get singleton datapool object
     * @return datapool* : NULL     error
     */
	static datapool* inst(void);

    /**
     * @brief Create a element object
     * datapool elements has 2 type,
     * is_global is true, element is global-element
     * is_global is false, element is local-element
     * 
     * global-element's name is unique
     * local-element's name is unique
     * names of local-element and global-elemenet are not unique.
     * @param  name             element name
     * @param  is_global        element is global
     * @param  need_persistent  element is need persistent
     * @return datapool_element get() == NULL       error
     * @return datapool_element get() != NULL       success
     */
    datapool_element create_element(const char* name,
        bool is_global = false,
        bool need_persistent = false);
        
    /**
     * @brief Get the element object
     * the element is created by yourself
     * or global-element
     * @param  name             element name
     * @param  is_global        element is global
     * @return datapool_element get() == NULL       error
     * @return datapool_element get() != NULL       success
     */
    datapool_element get_element(const char* name,
        bool is_global = false);
        
    /**
     * @brief remove element
     * the element must be created by yourself
     * @param  name             element name
     * @param  is_global        element is global
     * @return int == 0          success
     * @return int != 0         error
     */
    int remove_element(const char* name,
        bool is_global = false);

	ZAS_DISABLE_EVIL_CONSTRUCTOR(datapool);
};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_DATAPOOL_H__ */
#endif // UTILS_ENABLE_FBLOCK_DATAPOOL
/* EOF */
