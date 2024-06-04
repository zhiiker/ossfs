/*
 * ossfs - FUSE-based file system backed by Alibaba Cloud OSS
 *
 * Copyright(C) 2007 Randy Rizun <rrizun@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef S3FS_TYPES_H_
#define S3FS_TYPES_H_

#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <list>
#include <vector>

//
// For extended attribute
// (HAVE_XXX symbols are defined in config.h)
//
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#elif HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#if __cplusplus < 201103L
  #define OPERATOR_EXPLICIT
#else
  #define OPERATOR_EXPLICIT     explicit
#endif

//-------------------------------------------------------------------
// xattrs_t
//-------------------------------------------------------------------
//
// Header "x-oss-meta-xattr" is for extended attributes.
// This header is url encoded string which is json formatted.
//   x-oss-meta-xattr:urlencode({"xattr-1":"base64(value-1)","xattr-2":"base64(value-2)","xattr-3":"base64(value-3)"})
//
typedef struct xattr_value
{
    unsigned char* pvalue;
    size_t         length;

    explicit xattr_value(unsigned char* pval = NULL, size_t len = 0) : pvalue(pval), length(len) {}
    ~xattr_value()
    {
        delete[] pvalue;
    }
}XATTRVAL, *PXATTRVAL;

typedef std::map<std::string, PXATTRVAL> xattrs_t;

//-------------------------------------------------------------------
// acl_t
// Note: Header "x-oss-object-acl" is for acl. OSS's acl is not compatible with S3. 
// OSS object's acl is "private", "public-read", "public-read-write", "default"
// ref: https://help.aliyun.com/zh/oss/developer-reference/putobjectacl?spm=a2c4g.11186623.0.i26
//      https://docs.aws.amazon.com/AmazonS3/latest/userguide/acl-overview.html#canned-acl
//-------------------------------------------------------------------
class acl_t{
    public:
        enum Value{
            PRIVATE,
            PUBLIC_READ,
            PUBLIC_READ_WRITE,
            DEFAULT,
            UNKNOWN
        };

        // cppcheck-suppress noExplicitConstructor
        acl_t(Value value) : value_(value) {}

        operator Value() const { return value_; }

        const char* str() const
        {
            switch(value_){
                case PRIVATE:
                    return "private";
                case PUBLIC_READ:
                    return "public-read";
                case PUBLIC_READ_WRITE:
                    return "public-read-write";
                case DEFAULT:
                    return "default";
                case UNKNOWN:
                    return NULL;
            }
            abort();
        }

        static acl_t from_str(const char *acl)
        {
            if(0 == strcmp(acl, "private")){
                return PRIVATE;
            }else if(0 == strcmp(acl, "public-read")){
                return PUBLIC_READ;
            }else if(0 == strcmp(acl, "public-read-write")){
                return PUBLIC_READ_WRITE;
            }else if(0 == strcmp(acl, "default")){
                return DEFAULT;
            }else{
                return UNKNOWN;
            }
        }

    private:
        OPERATOR_EXPLICIT operator bool();
        Value value_;
};

//-------------------------------------------------------------------
// sse_type_t
//-------------------------------------------------------------------
class sse_type_t{
    public:
        enum Value{
            SSE_DISABLE = 0,      // not use server side encrypting
            SSE_OSS,               // server side encrypting by OSS key
            SSE_C,                // server side encrypting by custom key
            SSE_KMS               // server side encrypting by kms id
        };

        // cppcheck-suppress noExplicitConstructor
        sse_type_t(Value value) : value_(value) {}

        operator Value() const { return value_; }

    private:
        //OPERATOR_EXPLICIT operator bool();
        Value value_;
};

enum signature_type_t {
    V1_ONLY,
    V4_ONLY,
    V1_OR_V4
};

//----------------------------------------------
// etaglist_t / filepart / untreatedpart
//----------------------------------------------
//
// Etag string and part number pair
//
struct etagpair
{
    std::string  etag;        // expected etag value
    int          part_num;    // part number

    etagpair(const char* petag = NULL, int part = -1) : etag(petag ? petag : ""), part_num(part) {}

    ~etagpair()
    {
      clear();
    }

    void clear()
    {
        etag.erase();
        part_num = -1;
    }
};

typedef std::list<etagpair> etaglist_t;

//
// Each part information for Multipart upload
//
struct filepart
{
    bool         uploaded;      // does finish uploading
    std::string  etag;          // expected etag value
    int          fd;            // base file(temporary full file) descriptor
    off_t        startpos;      // seek fd point for uploading
    off_t        size;          // uploading size
    bool         is_copy;       // whether is copy multipart
    etagpair*    petag;         // use only parallel upload
    char*        streambuffer;  // use only direct read 
    off_t        streampos;     // use only direct read 

    filepart(bool is_uploaded = false, 
            int _fd = -1, 
            off_t part_start = 0, 
            off_t part_size = -1, 
            bool is_copy_part = false, 
            etagpair* petagpair = NULL) 
        : uploaded(false), fd(_fd), startpos(part_start), size(part_size), is_copy(is_copy_part), petag(petagpair), streambuffer(NULL), streampos(0) {}

    ~filepart()
    {
      clear();
    }

    void clear()
    {
        uploaded      = false;
        etag          = "";
        fd            = -1;
        startpos      = 0;
        size          = -1;
        is_copy       = false;
        petag         = NULL;
        streambuffer  = NULL;
        streampos     = 0;
    }

    void add_etag_list(etaglist_t& list, int partnum = -1)
    {
        if(-1 == partnum){
            partnum = static_cast<int>(list.size()) + 1;
        }
        list.push_back(etagpair(NULL, partnum));
        petag = &list.back();
    }

    void set_etag(etagpair* petagobj)
    {
        petag = petagobj;
    }

    int get_part_number()
    {
        if(!petag){
            return -1;
        }
        return petag->part_num;
    }
};

typedef std::list<filepart> filepart_list_t;

//
// Each part information for Untreated parts
//
struct untreatedpart
{
    off_t start;            // untreated start position
    off_t size;             // number of untreated bytes
    long  untreated_tag;    // untreated part tag

    untreatedpart(off_t part_start = 0, off_t part_size = 0, long part_untreated_tag = 0) : start(part_start), size(part_size), untreated_tag(part_untreated_tag)
    {
        if(part_start < 0 || part_size <= 0){
            clear();        // wrong parameter, so clear value.
        }
    }

    ~untreatedpart()
    {
        clear();
    }

    void clear()
    {
        start  = 0;
        size   = 0;
        untreated_tag = 0;
    }

    bool check_overlap(off_t chk_start, off_t chk_size)
    {
        if(chk_start < 0 || chk_size <= 0 || (chk_start + chk_size) < start || (start + size) < chk_start){
            return false;
        }
        return true;
    }

    bool stretch(off_t add_start, off_t add_size, long tag)
    {
        if(!check_overlap(add_start, add_size)){
            return false;
        }
        off_t new_start      = std::min(start, add_start);
        off_t new_next_start = std::max((start + size), (add_start + add_size));

        start         = new_start;
        size          = new_next_start - new_start;
        untreated_tag = tag;

        return true;
    }
};

typedef std::list<untreatedpart> untreated_list_t;

//-------------------------------------------------------------------
// mimes_t
//-------------------------------------------------------------------
struct case_insensitive_compare_func
{
    bool operator()(const std::string& a, const std::string& b) const {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    }
};
typedef std::map<std::string, std::string, case_insensitive_compare_func> mimes_t;

//-------------------------------------------------------------------
// Typedefs specialized for use
//-------------------------------------------------------------------
typedef std::list<std::string>             readline_t;
typedef std::map<std::string, std::string> kvmap_t;
typedef std::map<std::string, kvmap_t>     bucketkvmap_t;

#endif // S3FS_TYPES_H_

/*
* Local variables:
* tab-width: 4
* c-basic-offset: 4
* End:
* vim600: expandtab sw=4 ts=4 fdm=marker
* vim<600: expandtab sw=4 ts=4
*/
