/** @file json.h
 * definition of json operations
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_ABSFILE

#ifndef __CXX_ZAS_UTILS_JSON_H__
#define __CXX_ZAS_UTILS_JSON_H__

#include <string>
#include "utils/uri.h"

namespace zas {
namespace utils {

class mutex;
class jsonobject;

enum json_type
{
	json_unknown = 0,
	json_null,
	json_number,
	json_string,
	json_bool,
	json_array,
	json_object,
};

enum json_new_object_action
{
	json_actn_replace = 1,
	json_actn_use_exists,
	json_actn_fail_exists,
};

class UTILS_EXPORT jsonobject
{
public:
	jsonobject() = delete;
	~jsonobject() = delete;


	/**
	  Relaese the json object
	 */
	void release(void);

	/**
	  Serialize the json object to a string
	  @param str the string to hold the serialized object
	  @param format true: make a good format easy to read
	  		false: compact store, saving size but hard to read
	 */
	void serialize(std::string& str, bool format = true);

	/**
	  check if the jsonobject is null
	  @return true for success
	 */
	bool is_null(void) const;

	/**
	  check if the jsonobject is a number
	  @return true means the object is number
	 */
	bool is_number(void) const;

	/**
	  check if the jsonobject is a string
	  @return true means the object is string
	 */
	bool is_string(void) const;

	/**
	  check if the jsonobject is a boolean
	  @return true means the object is boolean
	 */
	bool is_bool(void) const;

	/**
	  check if the jsonobject is an array
	  @return true means the object is array
	 */
	bool is_array(void) const;

	/**
	  check if the jsonobject is an object
	  @return true means the object is object
	 */
	bool is_object(void) const;

	/**
	  get the type of the json object
	  @return the object type
	 */
	int get_type(void) const;

	/**
	  get the number of the object if it's type is number
	  @return the number if object's type is number
	  		0 if the object's type is not number
	 */
	int64_t to_number(void) const;

	/**
	  get the number of the object if it's type is number
	  @return the number if object's type is number
	  		0 if the object's type is not number
	 */
	double to_double(void);

	/**
	  get the string of the object if it's type is string
	  @return the string if object's type is string
	  		NULL if the object's type is not string
	 */
	const char* to_string(void) const;

	/**
	  get the bool value of the object if it's type is bool
	  @return the bool value if object's type is bool
	  		false if the object's type is not number
	 */
	bool to_bool(void) const;

	/**
	  get the count of the object if it is an array
	  @return the count of array (if it is an array)
	  		-1 if the object's type is not array
	 */
	int count(void) const;

	/**
	  get the array object by the array index
	  @param index the index that will retrive
	  @return the json object if it is an array
	  		null_jsonobject if it is not an array
	 */
	jsonobject& get(int index);
	const jsonobject& get(int index) const;

	jsonobject& operator[](int index);
	const jsonobject& operator[](int index) const;

	/**
	  get the object by its name
	  @param name the object name that will retrive
	  @return the json object if it is an object has has
	  		a child with the specified name
	  		null_jsonobject if it is not an object or
			  an error occurred
	 */
	jsonobject& get(const char* name);
	const jsonobject& get(const char* name) const;

	jsonobject& operator[](const char* name);
	const jsonobject& operator[](const char* name) const;

	/**
	  duplicate the json object tree
	  @param recurse deplicate all sub-objects
	  @return the json object new created
	 */
	jsonobject& dup(bool recurse = true);

	/**
	  create a new boolean json object
	  @param name the name of the object
	  @param value the value (true / false) of the object
	  @param repl indicate if we need to replace the exist object
			with the same object name
			true: replace the object with the same name
			false: fail to create object if object with same name exists
	  @return the json object new created
	 */
	jsonobject& new_bool(const char* name, bool value, bool repl = false);

	/**
	  create a new string json object
	  @param name the name of the object
	  @param str the value (string) of the object
	  @param repl indicate if we need to replace the exist object
			with the same object name
			true: replace the object with the same name
			false: fail to create object if object with same name exists
	  @return the json object new created
	 */
	jsonobject& new_string(const char* name, const char* str, bool repl = false);

	/**
	  create a new number json object
	  @param name the name of the object
	  @param number the value (number) of the object
	  @param repl indicate if we need to replace the exist object
			with the same object name
			true: replace the object with the same name
			false: fail to create object if object with same name exists
	  @return the json object new created
	 */
	jsonobject& new_number(const char* name, double number, bool repl = false);

	/**
	  create a new json object
	  @param name the name of the object
	  @param number the value (number) of the object
	  @param actn define the action when an object with same name exists
	  		json_actn_replace: replace the exist object
			json_actn_use_exists: return (and use) the exist object
			json_actn_fail_exists: fail return (return null object)
	  @return the json object new created
	 */
	jsonobject& new_object(const char* name, int actn = json_actn_fail_exists);

	/**
	  create a new json array
	  @param name the name of the array object
	  @param number the value (number) of the object
	  @param actn define the action when an object with same name exists
	  		json_actn_replace: replace the exist object
			json_actn_use_exists: return (and use) the exist object
			json_actn_fail_exists: fail return (return null object)
	  @return the json object new created
	 */
	jsonobject& new_array(const char* name, int actn = json_actn_fail_exists);

	/**
	  remove (detach) the object from the json array
	  @param idx the index of the object in the array
	  @param del indicate if we need to release the object
	  @return the json object detached
	  		if parameter del = true, a null jsonobject will returned
	 */
	jsonobject& detach(int idx, bool del = false);

	/**
	  remove (detach) the object from the json tree
	  @param name the name of the object in the json tree
	  @param del indicate if we need to release the object
	  @return the json object detached
	  		if parameter del = true, a null jsonobject will returned
	 */
	jsonobject& detach(const char* name, bool del = false);

	/**
	  add an item to the array
	  @param obj the object to be added to array
	  @return true for success, false if parents is not an array
	 */
	bool add(jsonobject& obj);

	/**
	  add a new string to the array
	  @param str the string content to be added to array
	  @return the object created and added to the array
	 */
	jsonobject& new_string(const char* str);

	/**
	  add a new item to the array
	  @return the object created and added to the array
	  	the object could be further operated by other methods
	 */
	jsonobject& new_item(void);

	/**
	  replace the item specified in index to the inputted one
	  @param i the index in the array
	  @param obj the object used to replace the original one
	  @return the object successfully replaced
	  		or null object if error occurred
	 */
	jsonobject& replace(int i, jsonobject& obj);

	/**
	  add a new object to the json object tree
	  @param name the name of new added object to be added
	  @param obj the object to be added
	  @return the object successfully added to object tree
	  		or null object if any error occurred
			(eg., an object with same name exists)
	 */	
	bool add(const char* name, jsonobject& obj);

	/**
	  Save the object to a text file
	  @param filename the file to be created
	  @return true on success
	 */
	bool savefile(const char* filename);

private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(jsonobject);
};

namespace json {

/**
  Create an json array
  @return the created array
  */
UTILS_EXPORT jsonobject& new_array(void);

/**
  Create a json object
  @param mtx the mutex for synchronized operation
  @return the created json object
  */
UTILS_EXPORT jsonobject& new_object(mutex* mtx = NULL);

/**
  parse the json tree from text buffer
  @param buffer the text buffer for parsing
  @param mtx the mutex for synchronized operation
  @return the json object
  */
UTILS_EXPORT jsonobject& parse(const char* buffer, mutex* mtx = NULL);

/**
  parse the json tree from text file
  @param buffer the text file for parsing
  @param mtx the mutex for synchronized operation
  @return the json object
  */
UTILS_EXPORT jsonobject& loadfromfile(const uri& filename, mutex* mtx = NULL);

/**
  save the json tree to text file
  @param buffer the text file for saving
  @param mtx the mutex for synchronized operation
  @return true for success
  */
UTILS_EXPORT bool savefile(jsonobject& obj, const uri& filename);

/**
  get the null jsonobject
  @return return the null object
 */
UTILS_EXPORT jsonobject& get_nullobject(void);

}  // end of namespace zas::utils::json
}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_JSON_H__ */
#endif // UTILS_ENABLE_FBLOCK_ABSFILE
/* EOF */

