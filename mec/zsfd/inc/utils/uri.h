/** @file uri.h
 * Definition of the uri parser
 */

#include "utils/utils.h"
#if (defined(UTILS_ENABLE_FBLOCK_URI) && defined(UTILS_ENABLE_FBLOCK_CMDLINE) && defined(UTILS_ENABLE_FBLOCK_CERT))

#ifndef __CXX_ZAS_UTILS_URI_H__
#define __CXX_ZAS_UTILS_URI_H__

#include <string>
#include "utils/utils.h"

namespace zas {
namespace utils {

class query;

class UTILS_EXPORT uri
{
public:
	uri();
	~uri();

	uri(const uri& u);
	uri& operator=(const uri& u);
	uri(const char* uristr);
	uri(const std::string& uristr);

	/**
	 Get the scheme of the uri
	 @return the string of scheme or leave blank
	 */
	const std::string get_scheme(void) const;

	/**
	 Check if the uri is valid (means successfully parsed)
	 @return true for valid while false for invalid
	 */
	bool valid(void) const;

	/**
	 Check if the uri has a username and password
	 @return < 0 invalid uri
	 		== 0 valid uri but withouth username and password
			 > 0 valid uri with username and password
	 */
	int has_user_passwd(void) const;

	/**
	  Check if the uri has a port
	  @return < 0 invalid uri
	  		 == 0 valid uri but has no port
			  > 0 valid uri and has a port
	 */
	int has_port(void) const;

	/**
	  Check if the uri has a filename
	  @return < 0 invalid uri
	  		 == 0 valid uri but has no filename
			  > 0 valid uri and has a filename
	 */
	int has_filename(void) const;

	/**
	 Get the name and password of the uri
	 @param user the parsed username
	 @param passwd the parsed password
	 @return < 0 invalid uri
	 		== 0 valid uri without username and password
			 > 0 valid uri with username and password
	 */
	int get_user_passwd(std::string& username, std::string& passwd) const;

	/**
	  Get the full path of the url
	  @return the string of the full path
	 */
	const std::string get_fullpath(void) const;

	/** 
	 * get the filename of the url
	 * @return the char* of filename
	 **/
	const char* get_filename(void) const;

	/**
	 * get the host of the url
	 * @return the host of the url
	 */
	const std::string get_host(void) const;

	/**
	 * get the port of the url
	 * @return the port or -1 if there is no port
	 */
	int get_port(void) const;

	/**
	  Change to another uri
	  @param the uri string
	  @return true for success parse
	  	false fail to parse the uri
	 */
	bool change(const char* uristr);
	bool change(const std::string& uristr);
	
	/**
	  Change the fullpath to a new one
	  @param newpath the new path to be changed
	  @return 0 for success
	 */
	int set_fullpath(const char* newpath);

	/**
	  get the unique digest of the uri
	  the digest is 32 bytes long and is
	  unique globally
	  @param output the digest (32 bytes)
	  @return 0 for success
	 */
	int digest(uint8_t output[32]) const;

	/**
	  Restore the uri object to the string
	  @return the string of uri
	 */
	std::string tostring(void) const;

	/**
	  get the count of query
	  @return the count
	 */
	int query_count(void);

	/**
	  get the specific key name of the query by its index
	  @param index the index of the quiery
	  @return the string of key of the query, NULL for an invalid index
	  */
	std::string query_key(int index);

	/**
	  get the specific value of the query by its index
	  @param index the index of the quiery
	  @return the string of value of the query, NULL for an invalid index
	  */
	std::string query_value(int index);

	/**
	  get the value of query specified by its key
	  @param key the const char* / string version key
	  @return the value of the query
	  */
	std::string query_value(const char* key);
	std::string query_value(const std::string& key);

	/**
	  clear all query
	  @return 0 success
	  */
	int clear_query(void);

	/**
	  add the specific value of its key
	  @param key the const char* / string version key
	  @param value the value of key
	  @return 0 success
	  */
	int add_query(const char* key, const char* value);

	/**
	  change the specific value of its key
	  @param key the const char* / string version key
	  @param value the value of key
	  @return 0 success
	  */
	int change_query(const char* key, const char* value);

	static uri parse(const char* uristr);

private:
	void* _data;
};

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_URI_H__ */
#endif // (defined(UTILS_ENABLE_FBLOCK_URI) && defined(UTILS_ENABLE_FBLOCK_CMDLINE) && defined(UTILS_ENABLE_FBLOCK_CERT))
/* EOF */
