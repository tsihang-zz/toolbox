#ifndef DP_STORAGE_H
#define DP_STORAGE_H

typedef enum StorageEnum_ {
    STORAGE_HOST,
    STORAGE_FLOW,
    STORAGE_IPPAIR,

    STORAGE_MAX,
} StorageEnum;

/** void ptr array for now */
typedef void* Storage;

void StorageInit(void);
void StorageCleanup(void);

/** \brief Register new storage
 *
 *  \param type type from StorageEnum
 *  \param name name
 *  \param size size of the per instance storage
 *  \param Alloc alloc function for per instance storage
 *  \param Free free function for per instance storage
 *
 *  \note if size == ptr size (so sizeof(void *)) and Alloc == NULL the API just
 *        gives the caller a ptr to store something it alloc'ed itself.
 */
int StorageRegister(const StorageEnum type, const char *name, const unsigned int size, void *(*Alloc)(unsigned int), void (*Free)(void *));
int StorageFinalize(void);

unsigned int StorageGetCnt(const StorageEnum type);
unsigned int StorageGetSize(const StorageEnum type);

/** \brief get storage for id */
void *StorageGetById(const Storage *storage, const StorageEnum type, const int id);
/** \brief set storage for id */
int StorageSetById(Storage *storage, const StorageEnum type, const int id, void *ptr);

/** \brief AllocById func for prealloc'd base storage (storage ptrs are part
 *         of another memory block) */
void *StorageAllocByIdPrealloc(Storage *storage, StorageEnum type, int id);
/** \brief AllocById func for when we manage the Storage ptr itself */
void *StorageAllocById(Storage **storage, const StorageEnum type, const int id);
void StorageFreeById(Storage *storage, const StorageEnum type, const int id);
void StorageFreeAll(Storage *storage, const StorageEnum type);
void StorageFree(Storage **storage, const StorageEnum type);


#endif
