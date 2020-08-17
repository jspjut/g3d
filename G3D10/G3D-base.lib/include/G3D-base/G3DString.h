/**
  \file G3D-base.lib/include/G3D-base/G3DString.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#pragma once

#include "G3D-base/platform.h"

#include <string>

#include <stdint.h>
#include <assert.h>
#include <algorithm>
#include "G3D-base/G3DAllocator.h"


#ifndef G3D_ARM
    #include <smmintrin.h>

    // Available for debugging memory problems. Always set to 1 in a
    // release build
    #define USE_SSE_MEMCPY 1
#endif

namespace G3D {

/** Returns true if this C string pointer is definitely located in the constant program data segment
    and does not require memory management. Used by G3D::G3DString. */
bool inConstSegment(const char* c);

// G3D malloc that guarantees 16-byte alignment
void* System_malloc(size_t);
void System_free(void*);


/** 
   \brief Very fast string class that follows the `std::string`/`std::basic_string` interface.

   - Recognizes constant segment strings and avoids copying them (currently, on MSVC 64-bit only)
   - Stores small strings internally to avoid heap allocation
   - Uses SSE instructions to copy internal strings
   - Uses the G3D block allocator when heap allocation is required

   Assumes 8-byte (64-bit) alignment of the String (`G3DString`) class, and only corrects
   by up to 8 bytes to ensure 16-byte SSE alignment. This is the default behavior for compilers
   generating 64-bit code. If you need to use this class with a 32-bit target or inside of a
   packed block, then it must be modified with additional padding.
*/
template<size_t INTERNAL_SIZE = 32>
class 
#if 1
   // This alignment hint causes miscompilation of aligned copying in obscure exception
   // throwing cases on the OS X clang++ but succeeds on Linux clang++
   // See: - http://stackoverflow.com/questions/30885997/clang-runtime-fault-when-throwing-aligned-type-compiler-bug
   //      - https://bugs.llvm.org/show_bug.cgi?id=23868
   // This has since been fixed in LLVM.
   alignas(16)
#endif
G3DString {
    // Assume that the compiler ensures 8-byte alignment on its own
    static constexpr size_t             MAX_ALIGNMENT_PADDING
        
#if __STDCPP_DEFAULT_NEW_ALIGNMENT__ >= 16
        // Platforms that guarantee 16-byte alignment
        = 0;
#else
        = 8;
#endif

public:

    typedef char                        value_type;
    typedef std::char_traits<char>      traits_type;
    typedef char&                       reference;
    typedef const char&                 const_reference;
    typedef char*                       pointer;
    typedef const char*                 const_pointer;
    typedef ptrdiff_t                   difference_type;
    typedef size_t                      size_type;
    typedef const char*                 const_iterator;
    typedef char*                       iterator;

protected:
    /** This inline storage is used when strings are small. Only INTERNAL_SIZE bytes are used,
        but m_data may point up to MAX_ALIGNMENT_PADDING bytes into this array to ensure
        16-byte (SSE) alignment. Note that the m_buffer array must appear first in the 
        String class to match the allocator's guaranteed 8-byte alignment, reducing the alignment
        that each instance is required to perform.*/
    char            m_buffer[INTERNAL_SIZE + MAX_ALIGNMENT_PADDING];

    /*    
     Tests the invariants:

    if `inBuffer()`:
      - `m_data` points into the array of `m_buffer`
      - `m_allocated` = `INTERNAL_SIZE`
      - `m_length` < `m_allocated`
      - `! inConst()` 

    if `inConst()` and the string itself is not const:
      - `m_data` points into the const segment
      - `m_allocated` = 0
      - `! inBuffer()` 

    if `! (inBuffer() || inConst())`:
      - `m_data` points into the heap
      - `m_allocated` = bytes allocated from the heap, including null terminator
      - `m_length` < `m_allocated`

    `m_data` is never `nullptr`.
    */
    inline void testInvariants() const {
#   ifdef G3D_DEBUG
        // Use inConstSegment instead of inConst, which is optimized and assumes
        // that the invariant holds already.
        const bool b = inBuffer(m_data);
        const bool c = inConstSegment(m_data) && ! b;//inConstSegment((const char*)this);
        assert(! (b && c));
        assert(m_data != nullptr);
        assert(((b && ((m_allocated == INTERNAL_SIZE) && (m_length < m_allocated))) ||
                (c && (m_allocated == 0)) ||
                (! (c || b) && (m_length < m_allocated))));
#   endif
    }

    /** The current value of this string. Includes '\0' termination. 
    */
    value_type*     m_data;

    /** Bytes to, but not including, '\0'. */
    size_t          m_length;

    /** Total size of m_data, including '\0'... or 0 if m_data is in a const segment.  If
        m_data is a pointer into m_buffer, then m_allocated will be INTERNAL_SIZE. */
    size_t          m_allocated;

    inline static void memcpy(void* dst, const void* src, size_t len) {
        ::memcpy(dst, src, len);
    }

    /** Copies from one m_data to another where both are in the m_buffer
        of the corresponding string and have 16-byte alignment. */
    inline static void memcpyBuffer(void* dst, const void* src) {
#       if USE_SSE_MEMCPY
          // Everything must be 16-byte aligned
          assert((INTERNAL_SIZE & 15) == 0);
          assert((uintptr_t(dst) & 15) == 0);
          assert((uintptr_t(src) & 15) == 0);
          __m128* d = reinterpret_cast<__m128*>(dst);
          const __m128* s = reinterpret_cast<const __m128*>(src);
          for (int i = 0; i < int(INTERNAL_SIZE >> 4); ++i) {
              d[i] = s[i];
          }
#       else
            memcpy(dst, src, INTERNAL_SIZE);
#       endif
    }

    /** Total size to allocate, including the terminator. 
       If b <= INTERNAL_SIZE, allocates INTERNAL_SIZE using m_buffer,
       otherwise allocates exactly b bytes on the heap. */
    inline value_type* alloc(size_t b) {
        // We assume that the Strings class is always on a 64-bit (8-byte) boundary
        assert((uintptr_t(m_buffer) & 7) == 0); 
        
        // When m_buffer is on an 8 byte boundary but not a 16-byte one, add 8
        // to advance it
        value_type* ptr = (b <= INTERNAL_SIZE) ? 
            m_buffer + (uintptr_t(m_buffer) & 8) :
            (value_type*)System_malloc(b);

        // Allocations are 128-bit (16-byte) aligned
        assert((uintptr_t(ptr) & 15) == 0);
        return ptr;
    }
    
    inline bool inBuffer(const char* ptr) const {
        return (ptr >= m_buffer) && (ptr < m_buffer + INTERNAL_SIZE + MAX_ALIGNMENT_PADDING);
    }

    inline bool inBuffer() const {
        return inBuffer(m_data);
    }

    static inline bool inConst(const char* ptr) {
        return inConstSegment(ptr);
    }

    /** Optimized to assume the invariant holds and only check m_allocated */
    inline bool inConst() const {
        return m_allocated == 0;
    }

    /** Only frees if the value was not in the const segment or m_buffer */
    inline void free(value_type* p) const {
        if (p && ! inBuffer(p) && ! inConst(p)) {
            System_free(p);
        }
    }

    /** Choose the number of *bytes* to allocate to hold a string of length L *characters*. i.e., allocate one extra byte, and then
        decide if it should just use the internal buffer. If not in the internal buffer, allocate at least 64 bytes,
        or 2*L + 1, so that appends can be fast. */
    static size_t chooseAllocationSize(size_t L) {
        // Avoid allocating more than internal size unless required, but always allocate at least the internal size
        return (L < INTERNAL_SIZE) ? INTERNAL_SIZE : std::max((size_t)(2 * L + 1), (size_t)64);
    }

    void prepareToMutate() {
        testInvariants();
        if (inConst()) {
            const value_type* old = m_data;
            m_allocated = chooseAllocationSize(m_length);
            m_data = alloc(m_allocated);
            memcpy(m_data, old, m_length + 1);
        }
    }

    /** Ensures that there is enough space for newSize characters
        in the string (newSize + 1 bytes for the terminator). */
    void ensureAllocation(size_t newSize, bool copyOldData) {
        testInvariants();
        if ((m_allocated < newSize + 1) && ! (inBuffer() && (newSize < INTERNAL_SIZE))) {
            // Allocate a new string and copy over
            value_type* old = m_data;
            m_allocated = chooseAllocationSize(newSize);
            m_data = alloc(m_allocated);

            // Cannot be in the const segment if we're going to copy, so
            // something must have been allocated
            assert(m_allocated > m_length);

            if (copyOldData) { memcpy(m_data, old, m_length + 1); }

            if (! inConstSegment(old)) { free(old); }
        }
        testInvariants();
    }

    void maybeDeallocate() {
        testInvariants();
        if (m_allocated && ! inBuffer()) {
            assert(! inConst());
            // Free previously allocated data
            free(m_data);
            m_data = alloc(0);
            m_allocated = INTERNAL_SIZE;
            m_data[0] = '\0';
            m_length = 0;
        }
        testInvariants();
    }

public:

    static const size_type npos = size_type(-1);
    /*
    void* operator new(size_t s) {
        return System_malloc(s);
    }
    
    void operator delete(void* p) {
        System_free(p);
    }
    */

    /** Creates a zero-length string */
    inline G3DString() : m_data(alloc(1)), m_length(0), m_allocated(INTERNAL_SIZE) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        m_data[0] = '\0';
        testInvariants();
    }

    // Explicit because this is an extension of the std::basic_string spec
    // and can cause weird cast coercions if allowed to be implicit
    explicit inline G3DString(const value_type c) : m_data(alloc(2)), m_length(1), m_allocated(INTERNAL_SIZE) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        m_data[0] = c;
        m_data[1] = '\0';
        testInvariants();
    }

    inline G3DString(size_t count, const value_type c) : m_length(count), m_allocated(INTERNAL_SIZE) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        // Allocate more than needed for fast append
        m_allocated = chooseAllocationSize(m_length);
        m_data = alloc(m_allocated);
        memset(m_data, c, m_length);
        m_data[m_length] = '\0';
        testInvariants();
    }


    // Move constructor
    G3DString(G3DString&& s) : m_data(nullptr), m_length(s.m_length), m_allocated(INTERNAL_SIZE) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        assert((uintptr_t(&s) & 7) == 0); // 8-byte aligned class

        if (s.inConst()) {
            // Share this const_seg value
            m_data = s.m_data;
            m_allocated = 0;
        } else if (s.inBuffer()) {
            // Copy the inlined data, which will be inlined here
            m_data = alloc(m_length + 1);
            assert(inBuffer());
            memcpyBuffer(m_data, s.m_data);
        } else {
            // Steal the heap allocation
            m_allocated = s.m_allocated;
            m_data = s.m_data;

            s.m_length = 0;
            s.m_allocated = INTERNAL_SIZE;
            s.m_data = s.alloc(1);
            s.m_data[0] = '\0';
        }
        testInvariants();
    }


    // Copy constructor
    G3DString(const G3DString& s) : m_data(nullptr), m_length(s.m_length), m_allocated(0) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        assert((uintptr_t(&s) & 7) == 0); // 8-byte aligned class

        if (s.inConst()) {
            // Share this const_seg value
            m_data = s.m_data;
        } else {
            // Clone the value, putting it in the internal storage if possible
            m_allocated = chooseAllocationSize(m_length);
            m_data = alloc(m_allocated);

            if (inBuffer() && s.inBuffer()) {
                memcpyBuffer(m_data, s.m_data);
            } else {
                // Includes copying the '\0'
                memcpy(m_data, s.m_data, m_length + 1);
            }
        }
        testInvariants();
    }


    explicit G3DString(const std::string& s) : m_length(s.size()) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class
        // Allocate more than needed for fast append
        m_allocated = chooseAllocationSize(m_length);
        m_data = alloc(m_allocated);
        memcpy(m_data, s.c_str(), m_length + 1);
        testInvariants();
    }


    G3DString(const value_type* c) : m_length(::strlen(c)) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class

        if (inConstSegment(c)) {
            m_data = const_cast<value_type*>(c);
            m_allocated = 0;
        } else {
            // Allocate more than needed for fast append
            m_allocated = chooseAllocationSize(m_length);
            m_data = alloc(m_allocated);
            memcpy(m_data, c, m_length + 1);
        }
        testInvariants();
    }


    /** \param len Copy \a len characters from \a c. */
    G3DString(const value_type* c, size_t len) : m_length(len) {
        assert((uintptr_t(this) & 7) == 0); // 8-byte aligned class

        // Allocate more than needed for fast append
        m_allocated = chooseAllocationSize(m_length);
        m_data = alloc(m_allocated);
        memcpy(m_data, c, m_length);
        m_data[m_length] = '\0'; // null terminator
        testInvariants();
    }


    G3DString& operator=(const value_type* c) {
        if (c[0] == '\0') {
            // Very common case of empty string
            ensureAllocation(0, false);
            m_data[0] = '\0';
            m_length = 0;

        } else if (inConstSegment(c)) {

            // Constant storage
            maybeDeallocate();
            // Share this const_seg value
            m_data = const_cast<value_type*>(c);
            m_length = strlen(c);
            m_allocated = 0;

        } else if (c >= m_data && c <= m_data + m_length) {

            // Assigning to a pointer within the string itself.
            // This is rare and dangerous; clone the passed in string first.
            *this = G3DString(c);

        } else {
            // Clone the other value, putting it in the internal storage if possible
            const size_t len = strlen(c);
            ensureAllocation(len, false);
            m_length = len;
            memcpy(m_data, c, m_length + 1);
        }

        testInvariants();
        return *this;
    }


    G3DString& operator=(const G3DString& s) {
        // Unlike the copy constructor, space has already been
        // allocated for this, so we need to reuse or deallocate
        // that storage.
        
        if (&s == this) {

            // Nothing to do

        } else if (s.inConst()) {
            // Constant storage
            maybeDeallocate();
            // Share this const_seg value
            m_data = s.m_data;
            m_length = s.m_length;
            m_allocated = 0;

        } else {
            // s could either be in the buffer or in the heap
            //
            // Clone the other value, putting it in the internal storage if possible
            ensureAllocation(s.m_length, false);
            m_length = s.m_length;

            if (inBuffer(m_data) && s.inBuffer()) {
                memcpyBuffer(m_data, s.m_data);
            } else {
                memcpy(m_data, s.m_data, m_length + 1);
            }
        }

        testInvariants();
        return *this;
    }

    // Move operator
    G3DString& operator=(G3DString&& s) {
        // Unlike the copy constructor, space has already been
        // allocated for this, so we need to reuse or deallocate
        // that storage.
        
        if (&s == this) {

            // Nothing to do

        } else if (s.inConst()) {
            // Constant storage
            maybeDeallocate();

            // Share this const_seg value
            m_data = s.m_data;
            m_length = s.m_length;
            m_allocated = 0;

        } else if (s.inBuffer()) {
            // s is in its buffer, so we must copy it. If this had heap allocation,
            // deallocate so that we can switch to faster buffer storage.
            maybeDeallocate();
            ensureAllocation(s.m_length, false);
            assert(inBuffer());
            m_length = s.m_length;
            memcpyBuffer(m_data, s.m_data);

        } else {
            // Free this
            maybeDeallocate();

            // s is in the heap. Steal its storage.
            m_allocated = s.m_allocated;
            m_data = s.m_data;
            m_length = s.m_length;

            s.m_data = s.alloc(0);
            s.m_length = 0;
            s.m_allocated = INTERNAL_SIZE;
        }

        testInvariants();
        return *this;
    }    

    G3DString& assign(const G3DString& s) {
        *this = s;
        testInvariants();
        return *this;
    }


    G3DString& assign(const G3DString& s, size_t subpos, size_t sublen) {
        size_t copy_len = (sublen == npos || subpos + sublen >= s.size()) ? s.size() - subpos : sublen;
        if (subpos == 0 && copy_len == s.size()) {
            return (*this).assign(s);
        }

        if (s.inConst() && (subpos + copy_len == s.size())) {
            // Constant storage
            maybeDeallocate();
            // Share this const_seg value
            m_data      = s.m_data + subpos;
            m_length    = copy_len;
            m_allocated = s.m_allocated;
        } else {
            // Clone the other value, putting it in the internal storage if possible
            m_length = copy_len;
            m_allocated = chooseAllocationSize(m_length);
            m_data = alloc(m_allocated);
            memcpy(m_data, s.m_data + subpos, m_length); 
            m_data[m_length] = '\0';
        }

        testInvariants();
        return *this;
    }


    G3DString& assign(const value_type* c, size_t n) {
        m_length = n;
        // Clone the other value, putting it in the internal storage if possible
        m_allocated = chooseAllocationSize(m_length);
        m_data = alloc(m_allocated);
        memcpy(m_data, c, m_length); 
        m_data[m_length] = '\0'; // null terminator

        testInvariants();
        return *this;
    }

    
    G3DString& assign(size_t n, const value_type c) {
        m_length = n;
        // Clone the other value, putting it in the internal storage if possible
        m_allocated = chooseAllocationSize(m_length);
        m_data = alloc(m_allocated);
        memset(m_data, c, m_length); 
        m_data[m_length] = '\0'; // null terminator
        testInvariants();
        return *this;
    }


    ~G3DString() {
        testInvariants();
        if (m_data && m_allocated) {
            // If in buffer, this will do nothing
            free(m_data);
        }
    }


    G3DString& insert(size_t pos, const G3DString& str, size_t subpos, size_t sublen) {
        testInvariants();
        // TODO
    }


    G3DString& insert(size_t pos, const G3DString& str) {
        testInvariants();
        return insert(pos, str, 0, str.size());
    }


    G3DString& insert(size_t pos, const value_type* c) {
        testInvariants();
        // TODO
    }


    void clear() {
        testInvariants();
        *this = G3DString();
    }


    const_iterator cbegin() const {
        return m_data;
    }


    const_iterator cend() const {
        if (m_data) {
            return m_data + m_length;
        } else {
            return nullptr;
        }
    }


    iterator begin() {
        prepareToMutate();
        return m_data;
    }


    iterator end() {
        prepareToMutate();
        if (m_data) {
            return m_data + m_length;
        } else {
            return nullptr;
        }
    }

    
    G3DString& erase(size_t pos = 0, size_t len = npos) {
        prepareToMutate();
        if ((len == npos) || (pos + len > m_length)) {
            len = m_length - pos;
        }

        if ((pos == 0) && (len == m_length)) {
            // Optimize erasing the entire string
            clear();
        } else if (len > 0) {
            for (int i = 0; i <= (int)std::max(m_length - pos, len - 1); ++i) {
                size_t nextIndex = pos + i + len;
                m_data[pos + i] = (nextIndex >= m_length) ? '\0' : m_data[nextIndex];
            }
            m_length -= len;
        }

        testInvariants();
        return *this;
    }


    void reserve(size_t newLength) {
        if (newLength + 1 > m_allocated) {
            const char* old = m_data;

            // Reserve more space
            if (newLength + 1 > INTERNAL_SIZE) {
                // Need heap allocation
                // Allocate the exact size required
                m_allocated = newLength + 1;
                m_data = alloc(m_allocated);
                memcpy(m_data, old, m_length + 1);

                // Maybe free the old buffer, if it was not m_buffer
                free(const_cast<char*>(old));
            } else {
                // Requested space will fit in the buffer
                
                if (! inBuffer()) {
                    // Copy the old to the buffer

                    // old must be in a const segment, because it is small and not in the m_buffer.
                    // Just copy to the internal buffer.
                    m_data = alloc(newLength + 1);
                    memcpy(m_data, old, m_length + 1);
                }
                assert(m_allocated == INTERNAL_SIZE);
            }
        }
        testInvariants();
    }


    G3DString operator+(const G3DString& s) const {
        G3DString result;
        result.m_length = m_length + s.m_length;
        result.m_allocated = chooseAllocationSize(result.m_length);
        result.m_data = result.alloc(result.m_allocated);

        if (result.inBuffer() && inBuffer()) {
            memcpyBuffer(result.m_data, m_data);
        } else {
            // Skip the terminator on the first string
            memcpy(result.m_data, m_data, m_length);
        }

        // Copy the terminator from the second
        memcpy(result.m_data + m_length, s.m_data, s.m_length + 1);

        testInvariants();
        result.testInvariants();
        return result;
    }


    G3DString operator+(const value_type* c) const {
        assert(c);
        if (*c == '\0') {

            // Empty string: run the copy constructor
            return *this;

        } else {
            const size_t L(strlen(c));
            G3DString result;
            result.m_length = m_length + L;
            result.m_allocated = chooseAllocationSize(result.m_length);
            result.m_data = result.alloc(result.m_allocated);
            
            if (result.inBuffer() && inBuffer()) {
                memcpyBuffer(result.m_data, m_data);
            } else {
                memcpy(result.m_data, m_data, m_length);
            }

            memcpy(result.m_data + m_length, c, L + 1);
            result.testInvariants();
            return result;
        }
    }
    

    G3DString operator+(const value_type c) const {
        const size_t L(1);

        G3DString result;
        result.m_length = m_length + L;
        result.m_allocated = chooseAllocationSize(result.m_length);
        result.m_data = result.alloc(result.m_allocated);

        if (result.inBuffer() && inBuffer()) {
            memcpyBuffer(result.m_data, m_data);
        } else {
            memcpy(result.m_data, m_data, m_length);
        }

        result.m_data[result.m_length - 1] = c;
        result.m_data[result.m_length] = '\0';

        result.testInvariants();
        return result;
    }


    G3DString& operator+=(const G3DString& s) {
        if (s.empty()) {
            return *this;
        }

        ensureAllocation(m_length + s.m_length, true);
        memcpy(m_data + m_length, s.m_data, s.m_length + 1);
        m_length += s.m_length;

        testInvariants();
        return *this;
    }


    G3DString& append(const G3DString& s, size_t subpos, size_t sublen) {
        if (s.empty() || (subpos >= s.size()) || (sublen == 0)) {
            return *this;
        }

        const size_t copy_len = ((sublen == npos) || (subpos + sublen >= s.size())) ? s.size() - subpos : sublen;
        ensureAllocation(m_length + copy_len, true);
        memcpy(m_data + m_length, s.m_data + subpos, copy_len);
        m_length += copy_len;
        m_data[m_length] = '\0';

        testInvariants();
        return *this;
    }


    G3DString& operator+=(const value_type c) {
        testInvariants();
        ensureAllocation(m_length + 1, true);
        m_data[m_length] = c;
        ++m_length;
        m_data[m_length] = '\0';

        testInvariants();
        return *this;
    }


    void push_back(value_type c) {
        *this += c;
    }


    G3DString& append(const G3DString& s) {
        return (*this += s);
    }


    G3DString& append(size_t n, value_type c) {
        ensureAllocation(m_length + n, true);
        memset(m_data + m_length, c, n);
        m_length += n;
        m_data[m_length] = '\0';
        testInvariants();
        return *this;
    }


    G3DString& append(const value_type* c, size_t t) {
        ensureAllocation(m_length + t, true);
        memcpy(m_data + m_length, c, t);
        m_length += t;
        m_data[m_length] = '\0';
        testInvariants();
        return *this;
    }


    G3DString& operator+=(const value_type* c) {
        const size_t t = strlen(c);
        ensureAllocation(m_length + t, true);
        memcpy(m_data + m_length, c, t + 1);
        m_length += t;
        testInvariants();
        return *this;
    }


    G3DString& append(const value_type* c) {
        return (*this) += c;
    }


    bool operator==(const G3DString& s) const {
        return (m_data == s.m_data) || ((m_length == s.m_length) && (memcmp(m_data, s.m_data, m_length) == 0));
    }


    bool operator==(const value_type* c) const {
        return (m_data == c) || (strcmp(m_data, c) == 0);
    }

    bool operator!=(const G3DString& s) const {
        return ! (*this == s);
    }


    bool operator!=(const value_type* c) const {
        return ! (*this == c);
    }


    size_type size() const {
        return m_length;
    }


    size_type length() const {
        return m_length;
    }


    size_type capacity() const {
        if (inBuffer()) {
            return INTERNAL_SIZE - 1;
        } else {
            return m_allocated - 1;
        }
    }


    size_type max_size() const {
        return size_type(-1);
    }


    bool empty() const {
        return (m_length == 0);
    }

    
    const value_type* c_str() const {
        return m_data;
    }


    const value_type* data() const {
        return m_data;
    }


    const_reference operator[](size_t x) const {
        assert(x < m_length && x >= 0); // "Index out of bounds"
        return m_data[x];
    }

    
    reference operator[](size_t x) {
        assert(x < m_length && x >= 0); // "Index out of bounds"
        return m_data[x];
    }


    const_reference at(size_t x) const {
        assert(x < m_length && x >= 0); // "Index out of bounds"
        return m_data[x];
    }


    reference at(size_t x) {
        assert(x < m_length && x >= 0); // "Index out of bounds"
        return m_data[x];
    }


    const_reference front() const {
        assert(m_data); // "Empty string"
        return m_data[0];
    }


    reference front(size_t x) {
        assert(m_data); // "Empty string"
        return m_data[0];
    }


    const_reference back() const {
        assert(m_data); // "Empty string"
        return m_data[m_length - 1];
    }


    reference back(size_t x) {
        assert(m_data); // "Empty string"
        return m_data[m_length - 1];
    }


    size_t find(const G3DString& str, size_t pos = 0) const {
        return find(str.c_str(), pos, str.m_length);
    }


    size_t find(const char* s, size_t pos = 0) const {
        return find(s, pos, ::strlen(s));
    }


    size_t find(const char* s, size_t pos, size_t n) const {
        if (n > m_length) {
            return npos;
        }
        const size_t j = m_length - n + 1;

        if (n == 1) {
            // Special case the extremely common 1-character version
            for (size_t i = pos; i < j; ++i) {
                if (m_data[i] == *s) { return (size_t)i; }
            }
        } else {
            for (size_t i = pos; i < j; ++i) {
                if (compare(m_data + i, n, s, n) == 0) {
                    return (size_t)i;
                }
            }
        }

        return npos;
    }


    size_t find(char c, size_t pos = 0) const {
        for (size_t i = pos; i < m_length; ++i) {
            if (m_data[i] == c) { return i; }
        }

        return npos;
    }

    
    size_t rfind(const G3DString& str, size_t pos = npos) const {
        return rfind(str.c_str(), pos, str.m_length);
    }


    size_t rfind(const char* s, size_t pos = npos) const {
        return rfind(s, pos, ::strlen(s));
    }

    
    size_t rfind(const char* s, size_t pos, size_t n) const {
        if (n > m_length ) {
            return npos;
        }
        
        if (n == 1) {
            for (int i = (int)std::min(pos, m_length - (int)n); i >= 0; --i) {
                if (m_data[i] == *s) { return (size_t)i; }
            }
        } else {
            for (int i = (int)std::min(pos, m_length - (int)n); i >= 0; --i) {
                if (compare(m_data + i, n, s, n) == 0) {
                    return (size_t)i;
                }
            }
        }
        return npos;
    }

    
    size_t rfind(char c, size_t pos = npos) const {
        int start = (pos == npos || pos > m_length) ? (int)m_length - 1 : (int)pos;
        for (int i = start; i >= 0; --i) {
            if (m_data[i] == c) { return i; }
        }
        return npos;
    }


    size_t find_first_of(const value_type* s, size_t pos, size_t n) const {
        for (size_t j = pos; j < m_length; ++j) {
            const value_type c = m_data[j];
            for (size_t i = 0; i < n; ++i) {
                if (c == s[i]) {
                    return j;
                }
            }
        }

        return npos;
    }

    
    size_t find_first_of(const G3DString& str, size_t pos = 0) const {
        return find_first_of(str.c_str(), pos, str.length());
    }


    size_t find_first_of(const value_type* s, size_t pos = 0) const {
        return find_first_of(s, pos, ::strlen(s));
    }


    size_t find_first_of(value_type c, size_t pos = 0) const {
        return find_first_of(&c, pos, 1);
    }


    size_t find_last_of(const value_type* s, size_t pos, size_t n) const {
        // When pos is specified, the search only includes characters at or before position pos, ignoring any possible occurrences after pos.
        int start = (int)m_length - 1;
        start = (size_t)start > pos ? (int)pos : start;
        for (int j = start; j >= 0; --j) {
            const value_type c = m_data[j];
            for (size_t i = 0; i < n; ++i) {
                if (c == s[i]) {
                    return (size_t)j;
                }
            }
        }

        return npos;
    }


    size_t find_last_of(const G3DString& str, size_t pos = npos) const {
        return find_last_of(str.c_str(), pos, str.length());
    }


    size_t find_last_of(const value_type* s, size_t pos = npos) const {
        return find_last_of(s, pos, ::strlen(s));
    }


    size_t find_last_of(value_type c, size_t pos = npos) const {
        return find_last_of(&c, pos, 1);
    }


    G3DString substr(size_t pos = 0, size_t len = npos) const {
        const size_t slen = std::max((size_t)0, std::min(m_length - pos, len));
        if (slen == 0) { return G3DString(); }

        // If copying from a const segment and ending at the end of the string, do not allocate
        if (inConst() && (m_length == pos + slen)) {
            return G3DString(m_data + pos);
        } else {
            return G3DString(m_data + pos, slen);
        }
    }

private:

    // Does not stop for internal null terminators.  Does not include the null
    // terminators.
    // See http://www.cplusplus.com/reference/string/string/compare/
    int compare(const char* a, size_t alen, const char* b, size_t blen) const {
        const size_t len = std::min(alen, blen);
        for (size_t i = 0; i < len; ++i, ++a, ++b) {
            const int c = *a - *b;
            if (c != 0) { return c; }
        }

        // Whichever was shorter is the lesser
        return (int)alen - (int)blen;
    }

public:

    int compare(const G3DString& str) const {
        if (m_data == str.m_data) {
            return 0;
        } else {
            return compare(m_data, m_length, str.m_data, str.m_length);
        }
    }

    int compare(size_t pos, size_t len, const G3DString& str) const {
        return compare(m_data + pos, std::min(m_length - pos, len), str.m_data, str.m_length); 
    }

    int compare(size_t pos, size_t len, const G3DString& str, size_t subpos, size_t sublen) const {
        return compare(m_data + pos, std::min(m_length - pos, len), str.m_data + subpos, std::min(str.m_length - subpos, sublen)); 
    }

    int compare(const char* s) const {
        return compare(m_data, m_length, s, ::strlen(s));
    }

    int compare(size_t pos, size_t len, const char* s) const {
        return compare(m_data + pos, std::min(m_length - pos, len), s, ::strlen(s)); 
    }

    int compare(size_t pos, size_t len, const char* s, size_t n) const {
        return compare(m_data + pos, std::min(m_length - pos, len), s, n); 
    }

    bool operator>(const G3DString& s) const {
        return compare(s) > 0;
    }
    
    bool operator<(const G3DString& s) const {
        return compare(s) < 0;
    }
};


/** \def G3D_STRING_DESTRUCTOR
 The name of the destructor for the class currently typedef'ed to String.
*/

// Choose the implementation of G3D::String (for debugging or legacy platforms)
#if !defined(G3D_64_BIT) // 32-bit

    typedef std::string String;
    #define G3D_STRING_DESTRUCTOR ~basic_string

#elif 0//
    // // Faster: use a good allocator with std::basic_string. This requires implementing
    // casts between different std::strings, however
    typedef std::basic_string<char, std::char_traits<char>, G3DAllocator<char> >  String;
    #define G3D_STRING_DESTRUCTOR ~basic_string

#else

    // Fastest
    typedef G3DString<64> String;
#   define G3D_STRING_DESTRUCTOR ~G3DString

#endif

/** For use with default output arguments. The value is always undefined. */
extern String ignoreString;

} // namespace G3D

inline G3D::G3DString<64> operator+(const char* s1, const G3D::G3DString<64>& s2) {
    return G3D::G3DString<64>(s1) + s2;
}

inline G3D::G3DString<64> operator+(const char s1, const G3D::G3DString<64>& s2) {
    return G3D::G3DString<64>(s1) + s2;
}

