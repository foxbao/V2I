/** @file dir.h
 * definition of directory operations
 */

#ifndef __CXX_ZAS_UTILS_DIR_H__
#define __CXX_ZAS_UTILS_DIR_H__

#include <string>
#include "std/smtptr.h"
#include "utils/utils.h"

namespace zas {
namespace utils {

/**
  Create the directory
  this will create all sub directories in the path
  @param dir the path
  @return 0 for success
  */
UTILS_EXPORT int createdir(const char* dir);

/**
  Remove the directory
  this will remove all its sub-directories and files
  @param dir the path
  @return 0 for success
  */
UTILS_EXPORT int removedir(const char* dir);

/**
  Check if a file exists
  @param filename the filename to determine if it exists
  @return true for exists
 */
UTILS_EXPORT bool fileexists(const char* filename);

/**
  change the name of a file or directory
  @param oldname the old name to be changed from
  @param newname the new name to be changed to
  @param return ture means success
 */
UTILS_EXPORT bool rename(const char* oldname, const char* newname);

/**
  Delete the file
  @param filename to be deleted
  @return true for success
 */
UTILS_EXPORT bool deletefile(const char *filename);

/**
  Get the file size
  @param path the file to be check
  @return file size to be returned
 */
UTILS_EXPORT long filesize(const char *path);

/**
  Get the full file name
  @param fielname source filename
  @parem remove_slash remove the last '/' in result
  @return the full file name
 */
UTILS_EXPORT std::string canonicalize_filename(
  const char* filename, bool remove_slash = false);

/**
  check if the specific path is a directory
  @param path the file to be check
  @return true means a directory
 */
UTILS_EXPORT bool isdir(const char* path);

enum devfile_type
{
  devfile_type_unknown = 0,
  devfile_type_char,
  devfile_type_block,
};

class UTILS_EXPORT dir_object
{
public:
	dir_object() = delete;
  ~dir_object() = delete;

	int addref(void);
  int release(void);

  /**
    Move to next directory entry
    @return -EEOF shows the end of records
   */
  int nextdir_entry(void);

  /**
    Get the directory entry name
    @return the name string
   */
  const char* get_dirname(void);

  /**
    Get the full path of the current dir
    @return the full path
   */
  const char* get_fullpath(void);

  /**
    Get the inode of the current dir
    @return the inode
   */
  size_t get_inode(void);
  
  /**
    Check if the current entry is a directory
    @return true indicates a directory
   */
  bool entry_isdir(void);

  /**
    Check if the current entry is a regular file
    @return true indicates a regular file
   */
  bool entry_is_regular_file(void);

  /**
    Check if the current entry is a device file
    @param type distinguish if it is char device or block device
    @return true indicates a device file
   */
  bool entry_is_devfile(devfile_type& type);

  /**
    Check if the current entry is a symbolic link
    @return true means it is a symbolic link
   */
  bool entry_is_symbolic_link(void);

  /**
    Check if the current entry is a named pipe
    @return true means it is a named pipe
   */
  bool entry_is_named_pipe(void);

  /**
    Check if the current entry is a unix domain socket
    @return true means it is a unix domain socket
   */
  bool entry_is_domain_socket(void);

};

typedef zas::smtptr<dir_object> dir;

/**
  open the specific dir
  @return dir object smart pointer
 */
dir UTILS_EXPORT opendir(const char* dirname);

/**
 * @brief Get the hostdir
 * 
 * @return the host dir
 */
UTILS_EXPORT std::string get_hostdir(void);

}} // end of namespace zas::utils
#endif /* __CXX_ZAS_UTILS_DIR_H__ */
/* EOF */

