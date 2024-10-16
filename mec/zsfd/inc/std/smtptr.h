/** @file smtptr.h
 * definition of the smart pointer
 */

#ifndef __CXX_ZAS_STD_SMART_PTR_H__
#define __CXX_ZAS_STD_SMART_PTR_H__

#include <new>
#include <stdio.h>

namespace zas {

// hwgraph smart pointer
template <typename T> class smtptr
{
public:
	smtptr() : p(NULL) {}
	smtptr(const smtptr& ct)
	: p(NULL)
	{
		if (this == &ct) return;
		if (ct.p) ct.p->addref();
		T* old = __atomic_exchange_n(&p, ct.p, __ATOMIC_ACQ_REL);
		if (old) old->release();
	}

	smtptr(T* tp)
	: p(NULL)
	{
		if (tp) tp->addref();
		T* old = __atomic_exchange_n(&p, tp, __ATOMIC_ACQ_REL);
		if (old) old->release();
	}

	~smtptr() {
		if (p) p->release();
	}

	smtptr& operator=(const smtptr& ct) {
		if (this == &ct) return *this;
		if (ct.p) ct.p->addref();
		T* old = __atomic_exchange_n(&p, ct.p, __ATOMIC_ACQ_REL);
		if (old) old->release();
		return *this;		
	}

	smtptr& operator=(T* tp) {
		if (tp) tp->addref();
		T* old = __atomic_exchange_n(&p, tp, __ATOMIC_ACQ_REL);
		if (old) old->release();
		return *this;
	}

	bool operator==(const smtptr& ct) {
		if (this == &ct) return true;
		if (p == ct.p) return true;
		return false;
	}

	bool operator!=(const smtptr& ct) {
		return !operator==(ct);
	}

	bool operator==(T* tp) {
		return (p == tp) ? true : false;
	}

	bool operator!=(T* tp) {
		return (p != tp) ? true : false;
	}

	operator bool(void) {
		return p ? true : false;
	}

	bool operator<(const smtptr& b) const {
		return size_t(p) > size_t(b.p);
	}

	T* operator->(void) { return p; }
	T* get(void) const { return p; }
	void set(T* tp) { p = tp; }

private: T* p;
};

template <class T, class U>
inline static smtptr<T> static_ptr_cast(const smtptr<U>& sp) noexcept {
	return smtptr<T>(static_cast<T*>(sp.get()));
}

template <class T, typename... _Args>
inline static smtptr<T> create_shared(_Args&&... __args) {
	return smtptr<T>(new (std::nothrow) T(__args...));
}

} // end of namespace zas
#endif /* __CXX_ZAS_STD_SMART_PTR_H__ */
