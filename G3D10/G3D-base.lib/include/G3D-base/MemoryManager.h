/**
  \file G3D-base.lib/include/G3D-base/MemoryManager.h

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/
#ifndef G3D_MemoryManager_h
#define G3D_MemoryManager_h

#include "G3D-base/platform.h"
#include "G3D-base/ReferenceCount.h"

namespace G3D {

/** 
   Abstraction of memory management.
   Default implementation uses G3D::System::malloc and is threadsafe.

   \sa LargePoolMemoryManager, CRTMemoryManager, AlignedMemoryManager, AreaMemoryManager */
class MemoryManager : public ReferenceCountedObject {
protected:

    MemoryManager();

public:

    /** Return a pointer to \a s bytes of memory that are unused by
        the rest of the program.  The contents of the memory are
        undefined */
    virtual void* alloc(size_t s);

    /** Invoke to declare that this memory will no longer be used by
        the program.  The memory manager is not required to actually
        reuse or release this memory. */
    virtual void free(void* ptr);

    /** Returns true if this memory manager is threadsafe (i.e., alloc
        and free can be called asychronously) */
    virtual bool isThreadsafe() const;

    /** Return the instance. There's only one instance of the default
        MemoryManager; it is cached after the first creation. */
    static shared_ptr<MemoryManager> create();
};

/** 
   Allocates memory on 16-byte boundaries.
   \sa MemoryManager, CRTMemoryManager, AreaMemoryManager */
class AlignedMemoryManager : public MemoryManager {
protected:

    AlignedMemoryManager();

public:
    
    virtual void* alloc(size_t s);

    virtual void free(void* ptr);

    virtual bool isThreadsafe() const;

    static shared_ptr<AlignedMemoryManager> create();
};


/** A MemoryManager implemented using the C runtime. Not recommended
    for general use; this is largely for debugging. */
class CRTMemoryManager : public MemoryManager {
protected:
    CRTMemoryManager();

public:

    virtual void* alloc(size_t s);
    virtual void free(void* ptr);
    virtual bool isThreadsafe() const;

    /** There's only one instance of this memory manager; it is 
        cached after the first creation. */
    static shared_ptr<CRTMemoryManager> create();
};

}

#endif
