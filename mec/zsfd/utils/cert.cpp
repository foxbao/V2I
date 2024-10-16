/** @file evlclient.cpp
 * implementation of certification related features
 */

#include "utils/utils.h"
#ifdef UTILS_ENABLE_FBLOCK_CERT

#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include "std/list.h"
#include "utils/cert.h"
#include "utils/mutex.h"

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/rand.h>

namespace zas {
namespace utils {

using namespace std;

void certutils_init(void)
{
	// everything has been added
	// including digests and ciphers
	OpenSSL_add_all_algorithms();

	// randomize
	timeval tv;
	gettimeofday(&tv, nullptr);
	RAND_seed(&tv, sizeof(timeval));
}

// DES encryption
uint8_t* zdcfhost_generic_des_encrypt(uint8_t* plain, uint32_t plen, uint8_t* cipher, int& clen, uint8_t* key)
{
	uint8_t k[24];
	uint8_t iv[8];
	memcpy(k, key, 24);
	memcpy(iv, key + 24, 8);

	int tmplen = 0;
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	EVP_EncryptInit(ctx, EVP_des_cbc(), k, iv);

	uint32_t cbufsz = EVP_CIPHER_CTX_block_size(ctx) + plen + 32;
	if (!cipher || (uint32_t)clen < cbufsz) cipher = new uint8_t [cbufsz];

	// if (!(plen % 8)) EVP_CIPHER_CTX_set_padding(&ctx, 0);

	// encryption
	if (!EVP_EncryptUpdate(ctx, cipher, &clen, plain, plen))
		goto error;
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
	if (!EVP_EncryptFinal_ex(ctx, cipher + clen, &tmplen))
#else
	if (!EVP_EncryptFinal(ctx, cipher + clen, &tmplen))
#endif
		goto error;

	clen += tmplen;
	assert((uint32_t)clen < cbufsz);
	EVP_CIPHER_CTX_free(ctx);
	return cipher;

error:
	EVP_CIPHER_CTX_free(ctx);
	delete [] cipher;
	return nullptr;
}

// DES deryption
int zdcfhost_generic_des_decrypt(uint8_t* cipher, int clen, uint8_t* plain, uint8_t* key)
{
	int ret = 0;
	uint8_t k[24];
	uint8_t iv[8];
	memcpy(k, key, 24);
	memcpy(iv, key + 24, 8);
		
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	EVP_DecryptInit(ctx, EVP_des_cbc(), k, iv);

	int plen = 0, tmplen = 0;
	if (!EVP_DecryptUpdate(ctx, plain, &plen, cipher, clen))
		{ ret = -1; goto dexit; }

#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
	if (!EVP_DecryptFinal_ex(ctx, plain + plen, &tmplen))
#else
	if (!EVP_DecryptFinal(ctx, plain + plen, &tmplen))
#endif
		{ ret = -2; goto dexit; }
		
	ret = plen + tmplen;
dexit:
	EVP_CIPHER_CTX_free(ctx);
	return ret;
}

struct cert_node
{
	cert_node(FILE* fp) : certbuf(nullptr), x509(nullptr)
	{
		listnode_init(ownerlist);

		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		certbuf = new uint8_t[size];
		uint8_t* tmp = certbuf;
		fread(certbuf, size, 1, fp);

		x509 = d2i_X509(nullptr, (const uint8_t**)&tmp, (long)size);
		if (nullptr == x509) {
			delete [] certbuf;
			certbuf = nullptr;
		}
	}

	cert_node(uint8_t* buf, uint32_t bufsz)
	 : certbuf(nullptr), x509(nullptr)
	{
		listnode_init(ownerlist);

		certbuf = new uint8_t[bufsz];
		memcpy(certbuf, buf, bufsz);
		uint8_t* tmp = certbuf;

		x509 = d2i_X509(nullptr, (const uint8_t**)&tmp, (long)bufsz);
		if (nullptr == x509) {
			delete [] certbuf;
			certbuf = nullptr;
		}
	}
	
	~cert_node() {
		if (certbuf) delete [] certbuf;
		if (x509) X509_free(x509);
	}

	listnode_t ownerlist;
	uint8_t* certbuf;
	uint32_t size;
	X509 *x509;
};

class cert_bundlefile
{
public:
	cert_bundlefile(const char* filename, mutex* m = nullptr)
		: fp(fopen(filename, "rb"))
		, buffer(nullptr), sz(0), pos(0), mut(m) {}

	cert_bundlefile(const uint8_t* buf, size_t siz, mutex* m = nullptr)
		: fp(nullptr), buffer(buf), sz(siz), pos(0), mut(m) {}

	~cert_bundlefile() {
		fclose();
	}

	bool file_loaded(void) {
		return (fp) ? true : false;
	}

	void rewind(void) {
		if (fp) ::rewind(fp);
		else pos = 0;
	}

	int fseek(long int off, int whence)
	{
		if (fp) return ::fseek(fp, off, whence);
		
		auto_mutex am(mut);
		if (SEEK_SET == whence) {
			if (off >= sz) return -1;
			pos = off;
		}
		else if (SEEK_CUR == whence) {
			off += pos;
			if (off >= sz) return -1;
			pos = off;
		}
		else if (SEEK_END == whence) {
			if (-off > pos) return -1;
			pos = (size_t)(((long int)pos) + off);
		}
		else return -1;
		return 0;
	}

	size_t fread(void *ptr, size_t elmsz, size_t n)
	{
		if (fp) return ::fread(ptr, elmsz, n, fp);
		if (!ptr || !elmsz || !n || !buffer || pos > sz) return 0;

		auto_mutex am(mut);
		size_t rn = (sz - pos) / elmsz;
		if (n > rn) n = rn;
		size_t totalsz = elmsz * n;
		if (totalsz) memcpy(ptr, &buffer[pos], totalsz);
		pos += totalsz;

		return n;
	}

	void fclose(void) {
		if (fp) ::fclose(fp);
		fp = nullptr;
	}

private:
	mutex* mut;
	FILE* fp;
	const uint8_t* buffer;
	size_t sz;
	size_t pos;
};

class cert_impl : public cert_node
{
	friend class certstore_impl;
public:
	cert_impl(FILE* fp)
		: cert_node(fp)
		, flags(0)
		, pubkey(nullptr)
		, encrypted_deskey(nullptr)
	{
	}

	cert_impl(uint8_t* buf, uint32_t bufsz)
		: cert_node(buf, bufsz)
		, flags(0)
		, pubkey(nullptr)
		, encrypted_deskey(nullptr)
	{
	}

	virtual ~cert_impl()
	{
		if (encrypted_deskey) delete [] encrypted_deskey;
		if (pubkey) RSA_free(pubkey);
	}

	void release(void) {
		delete this;
	}

	enum cert_impl_flags
	{
		certimpl_flags_verifed	= 1,
	};

	int get_RSA_size(void)
	{
		if (load_pubkey()) return -1;
		return RSA_size(pubkey);
	}

	int encrypt(uint8_t* plain, uint32_t size, uint8_t* cipher)
	{
		if (!plain || !cipher || !size) return -1;
		uint32_t rsa_sz = get_RSA_size();
        if (rsa_sz <= size + 42) return -2;
		return RSA_public_encrypt(size, plain, cipher, pubkey, RSA_PKCS1_OAEP_PADDING);
	}

	int verify(const char* type, uint8_t* plain, uint32_t psz, uint8_t* cipherbuf, uint32_t csz)
	{
		int vtype;
		if (!type || *type == '\0') return -1;
		if (!strcmp(type, "sha256")) vtype = NID_sha256;
		else if (!strcmp(type, "sha512")) vtype = NID_sha512;
		else return -2;

		if (load_pubkey()) return -1;
		if (1 == RSA_verify(vtype, plain, psz, cipherbuf, csz, pubkey))
			return 0;
		return 3;
	}

	uint8_t* get_encrypted_DESkey(void) {
		return encrypted_deskey;
	}

	uint8_t* new_DESkey(void)
	{
		uint32_t rsa_sz = get_RSA_size();

		// check if the cert is loaded
		// and verifed
		if (rsa_sz < 0) return nullptr;
		if (nullptr == encrypted_deskey) {
			encrypted_deskey = new uint8_t [rsa_sz];
			assert(nullptr != encrypted_deskey);
		}

		// generate random key
		if (1 != RAND_pseudo_bytes(plain_deskey, 32))
			return nullptr;
		if (rsa_sz != encrypt(plain_deskey, 32, encrypted_deskey))
			return nullptr;
		return encrypted_deskey;
	}

	uint8_t* DES_encrypt(uint8_t* plain, uint32_t plen, int& clen) {
		if (!encrypted_deskey) return nullptr;
		return zdcfhost_generic_des_encrypt(plain, plen, nullptr, clen, plain_deskey);
	}

	uint8_t* DES_encrypt(uint8_t* plain, uint32_t plen, uint8_t* cipher, int& clen) {
		if (!encrypted_deskey) return nullptr;
		return zdcfhost_generic_des_encrypt(plain, plen, cipher, clen, plain_deskey);
	}

	uint8_t* DES_decrypt(uint8_t* cipher, int clen, int& plen)
	{
		if (!encrypted_deskey || !clen) return nullptr;
		uint8_t* plain = new uint8_t [clen + 32];
		plen = zdcfhost_generic_des_decrypt(cipher, clen, plain, plain_deskey);
		if (plen <= 0 || plen > clen + 32)
		{
			delete [] plain;
			return nullptr;
		}
		return plain;
	}

	int DES_decrypt(uint8_t* cipher, int clen, uint8_t* plain, int plen)
	{
		if (!encrypted_deskey || !clen) return -1;
		int rlen = zdcfhost_generic_des_decrypt(cipher, clen, plain, plain_deskey);
		if (rlen > plen) return -20;
		return rlen;
	}

private:

	int load_pubkey(void)
	{
		if (pubkey) return 0;

		if (!x509 || !(flags & certimpl_flags_verifed))
			return -1;
		EVP_PKEY *pkey = X509_get_pubkey(x509);
		if (nullptr == pkey) return -2;
		pubkey = EVP_PKEY_get1_RSA(pkey);
		if (nullptr == pubkey) {
			EVP_PKEY_free(pkey);
			return -3;
		}
		EVP_PKEY_free(pkey);
		return 0;
	}

private:

	uint32_t flags;
	RSA* pubkey;
	uint8_t plain_deskey[32];
	uint8_t* encrypted_deskey;
};

class prikey_impl
{
public:
	prikey_impl(const char* filename)
	: m_rsa(nullptr)
	{
		FILE *fp = fopen(filename, "rb");
		if (nullptr == fp) return;
		fseek(fp, 0, SEEK_END);
		long sz = ftell(fp);
		rewind(fp);
		uint8_t* buf = new uint8_t [sz], *tmp = buf;
		fread(buf, sz, 1, fp);
		fclose(fp);
		
		m_rsa = d2i_RSAPrivateKey(nullptr, (const uint8_t**)&tmp, sz);
		delete [] buf;
	}

	prikey_impl(uint8_t* pkbuf, uint32_t size)
	: m_rsa(nullptr)
	{
		uint8_t* tmp = pkbuf;
		m_rsa = d2i_RSAPrivateKey(nullptr, (const uint8_t**)&tmp, size);
	}

	virtual ~prikey_impl() {
		if (m_rsa) RSA_free(m_rsa);
	}

	void release(void) {
		delete this;
	}

	int sign(const char* type, uint8_t* plain, uint32_t psz, uint8_t* cipher, uint32_t& csz)
	{
		int vtype;
		if (!plain || !cipher || !csz || !psz) return -1;
		if (!type || *type == '\0') return -2;
		if (!strcmp(type, "sha256")) vtype = NID_sha256;
		else if (!strcmp(type, "sha512")) vtype = NID_sha512;
		else return -3;

		if (1 == RSA_sign(vtype, plain, psz, cipher, &csz, m_rsa))
			return 0;
		return 4;
	}

	int decrypt(uint8_t* cipherbuf, uint32_t size, uint8_t* plain)
	{
		return RSA_private_decrypt(size, (uint8_t*)cipherbuf,
			(uint8_t*)plain, m_rsa,
			RSA_PKCS1_OAEP_PADDING);
	}

	int get_RSA_size(void)
	{
		if (nullptr == m_rsa) return 0;
		return RSA_size(m_rsa);
	}

	int DESkey_extract(uint8_t* cipherbuf, uint32_t size)
	{
		if (!cipherbuf || !size || get_RSA_size() != size)
			return -1;

		uint8_t* buf = (uint8_t*)alloca(size);
		int len = decrypt(cipherbuf, size, buf);
		if (len > 32) return -2;

		memcpy(plain_deskey, buf, 32);
		return len;
	}

	uint8_t* DES_encrypt(uint8_t* plain, uint32_t plen, int& clen) {
		return zdcfhost_generic_des_encrypt(plain, plen, nullptr, clen, plain_deskey);
	}

	uint8_t* DES_encrypt(uint8_t* plain, uint32_t plen, uint8_t* cipher, int& clen) {
		return zdcfhost_generic_des_encrypt(plain, plen, cipher, clen, plain_deskey);
	}

	uint8_t* DES_decrypt(uint8_t* cipher, int clen, int& plen)
	{
		if (!clen) return nullptr;
		uint8_t* plain = new uint8_t [clen + 32];
		plen = zdcfhost_generic_des_decrypt(cipher, clen, plain, plain_deskey);
		if (plen <= 0 || plen > clen + 32)
		{
			delete [] plain;
			return nullptr;
		}
		return plain;
	}

	int DES_decrypt(uint8_t* cipher, int clen, uint8_t* plain, int plen)
	{
		if (!clen) return -1;
		int rlen = zdcfhost_generic_des_decrypt(cipher, clen, plain, plain_deskey);
		if (rlen > plen) return -20;
		return rlen;
	}

private:
	uint8_t plain_deskey[32];
	RSA* m_rsa;
};

#define CERT_BUNDLE_FILE_MAGIC		"ZCERTBUN"
#define CERT_BUNDLE_FILE_VERISON	(1)
#define certutils_offsetof(type, member)	((uint32_t)(size_t)(&(((type*)0)->member)))

struct cert_bundle_header
{
	uint8_t		magic[8];
	uint32_t	version;
	uint32_t 	cert_count;
	uint32_t	plain_size;
	uint32_t	encrypted_size;
	uint32_t	header_size;
	uint32_t	cert_item_size[1];
};

class certstore_impl
{
public:
	certstore_impl()
	: m_store(nullptr)
	, m_ctx(nullptr) {
		listnode_init(m_certlist);
		listnode_init(m_verified_certlist);
		m_store = X509_STORE_new();
		m_ctx = X509_STORE_CTX_new();
	}

	virtual ~certstore_impl() {
		release_all_cert_nodes();
		if (m_ctx) {
			X509_STORE_CTX_cleanup(m_ctx);
			X509_STORE_CTX_free(m_ctx);
		}
		if (m_store) X509_STORE_free(m_store);
	}

	ZAS_DISABLE_EVIL_CONSTRUCTOR(certstore_impl);

public:

	void release(void) {
		delete this;
	}

	bool add_cert(const char* filename)
	{
		if (nullptr == filename)
			return false;
		FILE * fp = fopen(filename, "rb");
		if (nullptr == fp) return false;

		// create the cert node
		cert_node * node = new cert_node(fp);
		fclose(fp);

		if (!node->certbuf || !node->x509) {
			delete node;
			return false;
		}

		// add to the store
		if (1 != X509_STORE_add_cert(m_store, node->x509)) {
			delete node;
			return false;
		}
		listnode_add(m_certlist, node->ownerlist);
		return true;
	}

	bool export_bundle(const char* bundlefile, uint8_t* key)
	{
		// todo: need lock
		uint8_t* buffer = nullptr;
		uint32_t count = get_cert_count();
		if (!count) return false;

		uint32_t chdrsz = sizeof(cert_bundle_header) + (count - 1) * sizeof(uint32_t);
		cert_bundle_header* chdr = (cert_bundle_header*)alloca(chdrsz);
		
		chdr->cert_count = count;
		chdr->header_size = chdrsz;

		FILE* fp = fopen(bundlefile, "wb");
		if (nullptr == fp) return false;

		buffer = load_create_bundle_header(chdr);
		if (!buffer) goto error;
		
		if (nullptr == key)
		{
			// no encryption
			fwrite(chdr, chdr->header_size, 1, fp);
			fwrite(buffer, chdr->plain_size, 1, fp);
		}
		else
		{
			int clen;
			uint8_t* encrypted_buf = zdcfhost_generic_des_encrypt(buffer, chdr->plain_size, nullptr, clen, key);
			if (!encrypted_buf) goto error;

			chdr->encrypted_size = (uint32_t)clen;
			fwrite(chdr, chdr->header_size, 1, fp);
			fwrite(encrypted_buf, clen, 1, fp);
			delete [] encrypted_buf;
		}

		if (buffer) delete buffer;
		if (fp) fclose(fp);
		return true;

	error:
		if (buffer) delete buffer;
		if (fp) fclose(fp);
		return false;
	}

	bool load_bundle(const char* bundlefile, uint8_t* key)
	{
		cert_bundlefile fp(bundlefile, &m_mut);
		if (!fp.file_loaded()) return false;
		return load_bundle(fp, key);
	}

	bool load_bundle(const uint8_t* buffer, size_t sz, uint8_t* key)
	{
		cert_bundlefile fp(buffer, sz, &m_mut);
		return load_bundle(fp, key);
	}

	bool load_bundle(cert_bundlefile& fp, uint8_t* key)
	{
		uint32_t *cert_sztbl;
		cert_bundle_header hdr;
		uint8_t *plain = nullptr, *cipher = nullptr;

		if (bundle_check(fp, hdr)) goto error;
		// the bundle request a key but we do not
		// have a key, this must be an error
		if (!hdr.plain_size || (hdr.encrypted_size && !key))
			goto error;
		// build size table for every cert
		cert_sztbl = (uint32_t*)alloca(sizeof(uint32_t) * hdr.cert_count);
		fp.fseek(certutils_offsetof(cert_bundle_header, cert_item_size), SEEK_SET);
		if (hdr.cert_count != fp.fread(cert_sztbl, sizeof(uint32_t), hdr.cert_count))
			goto error;
		fp.fseek(hdr.header_size, SEEK_SET);
		if (hdr.encrypted_size)
		{
			cipher = new uint8_t [hdr.encrypted_size];
			if (1 != fp.fread(cipher, hdr.encrypted_size, 1))
				goto error;
			plain = new uint8_t [hdr.plain_size];
			int len = zdcfhost_generic_des_decrypt(cipher, hdr.encrypted_size, plain, key);
			
			delete [] cipher; cipher = nullptr;
			if (len != hdr.plain_size) goto error;
		}
		else
		{
			plain = new uint8_t [hdr.plain_size];
			if (1 != fp.fread(plain, hdr.plain_size, 1))
				goto error;
		}
		// load cert data
		if (load_certs_from_buffer(cert_sztbl, plain, hdr.cert_count))
			goto error;
		delete [] plain;
		fp.fclose();
		return true;

	error:
		if (cipher) delete [] cipher;
		if (plain) delete [] plain;
		fp.fclose();
		return false;
	}

	cert_impl* verify_cert(const char* filename)
	{
		int ret;
		FILE* fp = fopen(filename, "rb");
		if (nullptr == fp) return nullptr;
	
		cert_impl* cert = new cert_impl(fp);
		fclose(fp);

		if (!cert->certbuf || !cert->x509)
			goto error;

		// verify the cert
		if (1 != X509_STORE_CTX_init(m_ctx, m_store, cert->x509, nullptr))
			goto error;
		ret = X509_verify_cert(m_ctx);
		X509_STORE_CTX_cleanup(m_ctx);

		if (1 == ret) {
			cert->flags |= cert_impl::certimpl_flags_verifed;
			listnode_add(m_verified_certlist, cert->ownerlist);
			return cert;
		}

	error:
		delete cert;
		return nullptr;
	}

	cert_impl* verify_cert(uint8_t* certbuf, uint32_t size)
	{
		int ret;
		if (!certbuf || !size) return nullptr;	
		cert_impl* cert = new cert_impl(certbuf, size);
		if (!cert->certbuf || !cert->x509)
			goto error;

		// verify the cert
		if (1 != X509_STORE_CTX_init(m_ctx, m_store, cert->x509, nullptr))
			goto error;
		ret = X509_verify_cert(m_ctx);
	 	if (1 != ret)
	 	{
	 		long nCode = X509_STORE_CTX_get_error(m_ctx);
	 		const char * pChError = X509_verify_cert_error_string(nCode);
	 		printf("Error is %s\n", pChError);
	 	}		
		X509_STORE_CTX_cleanup(m_ctx);

		if (1 == ret) {
			cert->flags |= cert_impl::certimpl_flags_verifed;
			listnode_add(m_verified_certlist, cert->ownerlist);
			return cert;
		}

	error:
		delete cert;
		return nullptr;

	}

private:

	int load_certs_from_buffer(uint32_t* cert_sztbl, uint8_t* buf, uint32_t count)
	{
		listnode_t locallist = LISTNODE_INITIALIZER(locallist);
		for (uint32_t i = 0; i < count; ++i)
		{
			cert_node* node = new cert_node(buf, cert_sztbl[i]);
			listnode_add(locallist, node->ownerlist);

			if (!node->certbuf || !node->x509)
				goto error;

			// move to next cert
			buf += cert_sztbl[i];
		}

		// need lock
		m_mut.lock();
		while (!listnode_isempty(locallist))
		{
			listnode_t* item = locallist.next;
			cert_node* node = list_entry(cert_node, ownerlist, item);

			// add to store
			if (1 != X509_STORE_add_cert(m_store, node->x509)) {
				m_mut.unlock();
				goto error;
			}

			// all success, add to cert list
			listnode_del(*item);
			listnode_add(m_certlist, node->ownerlist);
		}
		m_mut.unlock();
		return 0;

	error:
		while (!listnode_isempty(locallist))
		{
			listnode_t* item = locallist.next;
			cert_node* node = list_entry(cert_node, ownerlist, item);
			listnode_del(*item);
			delete node;
		}
		return -1;
	}

	// check the bundle file
	int bundle_check(cert_bundlefile& fp, cert_bundle_header& hdr)
	{
		fp.rewind();
		if (1 != fp.fread(&hdr, sizeof(hdr), 1))
			return -1;
		if (memcmp(hdr.magic, CERT_BUNDLE_FILE_MAGIC, 8))
			return -2;
		if (hdr.version > CERT_BUNDLE_FILE_VERISON)
			return -3;
		if (!hdr.cert_count) return -4;
		return 0;
	}

	// this function is not locked
	uint8_t* load_create_bundle_header(cert_bundle_header* hdr)
	{
		memcpy(hdr->magic, CERT_BUNDLE_FILE_MAGIC, 8);
		hdr->version = CERT_BUNDLE_FILE_VERISON;
		
		uint32_t i, sz;
		listnode_t* item;
		for (i = 0, sz = 0, item = m_certlist.next;
			item != &m_certlist; item = item->next, ++i)
		{
			if (i >= hdr->cert_count)
				return nullptr;

			cert_node* node = list_entry(cert_node, ownerlist, item);
			hdr->cert_item_size[i] = node->size;
			sz += node->size;
		}

		// merge all certs
		uint8_t* buf = new uint8_t [sz], *tmp = buf;
		hdr->plain_size = sz;
		hdr->encrypted_size = 0;

		for (i = 0, item = m_certlist.next; item != &m_certlist;
			item = item->next, tmp += hdr->cert_item_size[i++])
		{
			cert_node* node = list_entry(cert_node, ownerlist, item);
			memcpy(tmp, node->certbuf, node->size);
		}
		return buf;
	}

	// this function is not locked
	uint32_t get_cert_count(void)
	{
		uint32_t ret = 0;
		for (listnode_t* item = m_certlist.next;
			item != &m_certlist; item = item->next, ++ret);
		return ret;
	}

	void release_all_cert_nodes(void)
	{
		// we lock here to make sure safe
		auto_mutex am(m_mut);
		while (!listnode_isempty(m_certlist))
		{
			listnode_t* item = m_certlist.next;
			cert_node* node = list_entry(cert_node, ownerlist, item);
			listnode_del(*item);
			delete node;
		}

		while (!listnode_isempty(m_verified_certlist))
		{
			listnode_t* item = m_verified_certlist.next;
			cert_impl* node = list_entry(cert_impl, ownerlist, item);
			listnode_del(*item);
			delete node;
		}
	}

private:
	mutex m_mut;
	listnode_t m_certlist;
	listnode_t m_verified_certlist;
	X509_STORE* m_store;
	X509_STORE_CTX* m_ctx;
};

class digest_impl
{
public:
	digest_impl(const char* algorithm)
	: md(nullptr)
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
	, mdctx(nullptr)
#endif
	{
		md = EVP_get_digestbyname(algorithm);
		if (nullptr == md) return;

#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
		mdctx = EVP_MD_CTX_new();
		if (nullptr == mdctx) goto error;

		if (EVP_DigestInit_ex(mdctx, md, nullptr))
			return;

	error:
		if (mdctx) EVP_MD_CTX_free(mdctx);
		mdctx = nullptr;
		md = nullptr;
#else
		EVP_MD_CTX_init(&mdctx);
		if (!EVP_DigestInit_ex(&mdctx, md, nullptr))
		{
			EVP_MD_CTX_destroy(&mdctx);
			md = nullptr;
		}
#endif
	}

	virtual ~digest_impl()
	{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
		if (mdctx) EVP_MD_CTX_free(mdctx);
		mdctx = nullptr;
#else
		EVP_MD_CTX_cleanup(&mdctx);
#endif
		md = nullptr;
	}

	int append(void* data, size_t sz)
	{
		if (!data || !sz) return -1;
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
		if (!mdctx) return -2;
		if (!EVP_DigestUpdate(mdctx, data, sz))
#else
		if (!md) return -2;
		if (!EVP_DigestUpdate(&mdctx, data, sz))
#endif
			return -3;
		return 0;
	}

	const uint8_t* getresult(size_t* sz)
	{
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
		if (!mdctx) return nullptr;
#else
		if (!md) return nullptr;
#endif
		unsigned int len;
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
		int ret = EVP_DigestFinal_ex(mdctx, md_value, &len);
		EVP_MD_CTX_free(mdctx);
		mdctx = nullptr;
#else
		int ret = EVP_DigestFinal_ex(&mdctx, md_value, &len);
		EVP_MD_CTX_cleanup(&mdctx);
#endif
		md = nullptr;

		if (!ret) return nullptr;
		if (sz) *sz = (size_t)len;
		return (const uint8_t*)md_value;
	}
	
private:
	uint8_t md_value[EVP_MAX_MD_SIZE];
	const EVP_MD *md;
#if (OPENSSL_VERSION_NUMBER >= 0x10100000)
	EVP_MD_CTX* mdctx;
#else
	EVP_MD_CTX mdctx;
#endif
};

digest::digest(const char* algorithm)
: _data(nullptr)
{
	digest_impl* dig = new digest_impl(algorithm);
	assert(nullptr != dig);
	_data = reinterpret_cast<void*>(dig);
}

digest::~digest()
{
	digest_impl* dig = reinterpret_cast<digest_impl*>(_data);
	if (nullptr != dig) {
		delete dig;
		_data = nullptr;
	}
}

int digest::append(void* data, size_t sz)
{
	digest_impl* di = reinterpret_cast<digest_impl*>(_data);
	if (nullptr == di) return -ENOTAVAIL;
	return di->append(data, sz);
}

const uint8_t* digest::getresult(size_t* sz)
{
	digest_impl* di = reinterpret_cast<digest_impl*>(_data);
	if (nullptr == di) {
		if (sz) *sz = 0;
		return nullptr;
	}
	return di->getresult(sz);
}

int base64_encode(const void* _input, size_t sz, string& output)
{
	auto* input = (const uint8_t*)_input;
	if (nullptr == input || !sz) {
		return -EBADPARM;
	}
	// encoding table
	const char encode_table[]
		= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	uint8_t tmp[4] = {0};

	for(int i = 0; i < (int)(sz / 3); i++)
	{
		tmp[1] = *input++;
		tmp[2] = *input++;
		tmp[3] = *input++;
		output += encode_table[tmp[1] >> 2];
		output += encode_table[((tmp[1] << 4) | (tmp[2] >> 4)) & 0x3F];
		output += encode_table[((tmp[2] << 2) | (tmp[3] >> 6)) & 0x3F];
		output += encode_table[tmp[3] & 0x3F];
	}

	// encoding the remaining
	int mod = sz % 3;
	if(mod == 1)
	{
		tmp[1] = *input++;
		output+= encode_table[(tmp[1] & 0xFC) >> 2];
		output+= encode_table[((tmp[1] & 0x03) << 4)];
		output+= "==";
	}
	else if(mod == 2)
	{
		tmp[1] = *input++;
		tmp[2] = *input++;
		output+= encode_table[(tmp[1] & 0xFC) >> 2];
		output+= encode_table[((tmp[1] & 0x03) << 4) | ((tmp[2] & 0xF0) >> 4)];
		output+= encode_table[((tmp[2] & 0x0F) << 2)];
		output+= "=";
	}
	return 0;
}

int base64_encode(const string &input, string &output)
{
	return base64_encode((const void*)input.c_str(),
		input.length(), output);
}

// decoding table
static const uint8_t decode_table[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	62, // '+'
	0, 0, 0,
	63, // '/'
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
	0, 0, 0, 0, 0, 0, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
	0, 0, 0, 0, 0, 0,
	26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
	39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, // 'a'-'z'
};

int base64_decode(const char* input, size_t sz, uint8_t* output, size_t outsz)
{
	if (nullptr == input || nullptr == output)
		return -EBADPARM;
	if (!sz) sz = strlen(input);

	int nval;
	size_t i = 0;
	size_t outbypte = 0;

	while (i < sz)
	{
		if (outbypte > outsz) return -((int)outbypte);

		nval = decode_table[*input++] << 18;
		nval += decode_table[*input++] << 12;
		*output = (nval & 0x00FF0000) >> 16;
		++output, ++outbypte;

		if (*input != '=')
		{
			if (outbypte > outsz) return -((int)outbypte);

			nval += decode_table[*input++] << 6;
			*output = (nval & 0x0000FF00) >> 8;
			++output, ++outbypte;

			if (*input != '=')
			{
				if (outbypte > outsz) return -((int)outbypte);

				nval += decode_table[*input++];
				*output = nval & 0x000000FF;
				++output, ++outbypte;
			}
		}
		i += 4;
	}
	return ((int)outbypte);
}

int base64_decode(const char* input, size_t sz, string& output)
{
	if (nullptr == input) {
		return -EBADPARM;
	}
	if (!sz) sz = strlen(input);

	int nval;
	size_t i = 0;
	output.clear();

	while (i < sz)
	{
		nval = decode_table[*input++] << 18;
		nval += decode_table[*input++] << 12;
		output.append(1, char(uint8_t((nval & 0x00FF0000) >> 16)));

		if (*input != '=')
		{
			nval += decode_table[*input++] << 6;
			output.append(1, char(uint8_t((nval & 0x0000FF00) >> 8)));

			if (*input != '=')
			{
				nval += decode_table[*input++];
				output.append(1, char(uint8_t(nval & 0x000000FF)));
			}
		}
		i += 4;
	}
	return output.size();
}

}} // end of namespace zas::utils
#endif // UTILS_ENABLE_FBLOCK_CERT
/* EOF */
