#include "oryx.h"
#include "dp_storage.h"

typedef struct StorageMapping_ {
    const char *name;
    StorageEnum type; // host, flow, tx, stream, ssn, etc
    unsigned int size;
    void *(*Alloc)(unsigned int);
    void (*Free)(void *);
} StorageMapping;

/** \brief list of StorageMapping used at registration time */
typedef struct StorageList_ {
    StorageMapping map;
    int id;
    struct StorageList_ *next;
} StorageList;

static StorageList *storage_list = NULL;
static int storage_max_id[STORAGE_MAX];
static int storage_registraton_closed = 0;
static StorageMapping **storage_map = NULL;

static const char *StoragePrintType(StorageEnum type)
{
    switch(type) {
        case STORAGE_HOST:
            return "host";
        case STORAGE_FLOW:
            return "flow";
        case STORAGE_IPPAIR:
            return "ippair";
        case STORAGE_MAX:
            return "max";
    }
    return "invalid";
}

void StorageInit(void)
{
    memset(&storage_max_id, 0x00, sizeof(storage_max_id));
    storage_list = NULL;
    storage_map = NULL;
    storage_registraton_closed = 0;
}

void StorageCleanup(void)
{
    if (storage_map) {
        int i;
        for (i = 0; i < STORAGE_MAX; i++) {
            if (storage_map[i] != NULL) {
                free(storage_map[i]);
                storage_map[i] = NULL;
            }
        }
        free(storage_map);
        storage_map = NULL;
    }

    StorageList *entry = storage_list;
    while (entry) {
        StorageList *next = entry->next;
        free(entry);
        entry = next;
    }

    storage_list = NULL;
}

int StorageRegister(const StorageEnum type, const char *name, const unsigned int size, void *(*Alloc)(unsigned int), void (*Free)(void *))
{
    if (storage_registraton_closed)
        return -1;

    if (type >= STORAGE_MAX || name == NULL || strlen(name) == 0 ||
            size == 0 || (size != sizeof(void *) && Alloc == NULL) || Free == NULL)
        return -1;

    StorageList *list = storage_list;
    while (list) {
        if (strcmp(name, list->map.name) == 0 && type == list->map.type) {
            oryx_loge(0,
				"storage for type \"%s\" with "
                "name \"%s\" already registered\n", StoragePrintType(type),
                name);
            return -1;
        }

        list = list->next;
    }

    StorageList *entry = malloc(sizeof(StorageList));
    if (unlikely(entry == NULL))
        return -1;

    memset(entry, 0x00, sizeof(StorageList));

    entry->map.type = type;
    entry->map.name = name;
    entry->map.size = size;
    entry->map.Alloc = Alloc;
    entry->map.Free = Free;

    entry->id = storage_max_id[type]++;
    entry->next = storage_list;
    storage_list = entry;

    return entry->id;
}

int StorageFinalize(void)
{
    int count = 0;
    int i;

    storage_registraton_closed = 1;

    for (i = 0; i < STORAGE_MAX; i++) {
        if (storage_max_id[i] > 0)
            count++;
    }
    if (count == 0)
        return 0;

    storage_map = malloc(sizeof(StorageMapping *) * STORAGE_MAX);
    if (unlikely(storage_map == NULL)) {
        return -1;
    }
    memset(storage_map, 0x00, sizeof(StorageMapping *) * STORAGE_MAX);

    for (i = 0; i < STORAGE_MAX; i++) {
        if (storage_max_id[i] > 0) {
            storage_map[i] = malloc(sizeof(StorageMapping) * storage_max_id[i]);
            if (storage_map[i] == NULL)
                return -1;
            memset(storage_map[i], 0x00, sizeof(StorageMapping) * storage_max_id[i]);
        }
    }

    StorageList *entry = storage_list;
    while (entry) {
        if (storage_map[entry->map.type] != NULL) {
            storage_map[entry->map.type][entry->id].name = entry->map.name;
            storage_map[entry->map.type][entry->id].type = entry->map.type;
            storage_map[entry->map.type][entry->id].size = entry->map.size;
            storage_map[entry->map.type][entry->id].Alloc = entry->map.Alloc;
            storage_map[entry->map.type][entry->id].Free = entry->map.Free;
        }

        entry = entry->next;
    };

#ifdef DEBUG
    for (i = 0; i < STORAGE_MAX; i++) {
        if (storage_map[i] == NULL)
            continue;

        int j;
        for (j = 0; j < storage_max_id[i]; j++) {
            StorageMapping *m = &storage_map[i][j];
            oryx_logd("type \"%s\" name \"%s\" size \"%"PRIuMAX"\"\n",
                    StoragePrintType(m->type), m->name, (uintmax_t)m->size);
        }
    }
#endif
    return 0;
}

unsigned int StorageGetCnt(StorageEnum type)
{
    return storage_max_id[type];
}

/** \brief get the size of the void array used to store
 *         the pointers
 *  \retval size size in bytes, can return 0 if not storage is needed
 *
 *  \todo we could return -1 when registration isn't closed yet, however
 *        this will break lots of tests currently, so not doing it now */
unsigned int StorageGetSize(StorageEnum type)
{
    return storage_max_id[type] * sizeof(void *);
}

void *StorageGetById(const Storage *storage, const StorageEnum type, const int id)
{
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
    oryx_logd("storage %p id %d\n", storage, id);
    if (storage == NULL)
        return NULL;
    return storage[id];
}

int StorageSetById(Storage *storage, const StorageEnum type, const int id, void *ptr)
{
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
    oryx_logd("storage %p id %d\n", storage, id);
    if (storage == NULL)
        return -1;
    storage[id] = ptr;
    return 0;
}

void *StorageAllocByIdPrealloc(Storage *storage, StorageEnum type, int id)
{
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
    oryx_logd("storage %p id %d\n", storage, id);

    StorageMapping *map = &storage_map[type][id];
    if (storage[id] == NULL && map->Alloc != NULL) {
        storage[id] = map->Alloc(map->size);
        if (storage[id] == NULL) {
            return NULL;
        }
    }

    return storage[id];
}

void *StorageAllocById(Storage **storage, StorageEnum type, int id)
{
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
    oryx_logd("storage %p id %d\n", storage, id);

    StorageMapping *map = &storage_map[type][id];
    Storage *store = *storage;
    if (store == NULL) {
        store = malloc(sizeof(void *) * storage_max_id[type]);
        if (unlikely(store == NULL))
        return NULL;
        memset(store, 0x00, sizeof(void *) * storage_max_id[type]);
    }
    oryx_logd("store %p\n", store);

    if (store[id] == NULL && map->Alloc != NULL) {
        store[id] = map->Alloc(map->size);
        if (store[id] == NULL) {
            free(store);
            *storage = NULL;
            return NULL;
        }
    }

    *storage = store;
    return store[id];
}

void StorageFreeById(Storage *storage, StorageEnum type, int id)
{
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
#ifdef UNITTESTS
    if (storage_map == NULL)
        return;
#endif
    oryx_logd("storage %p id %d", storage, id);

    Storage *store = storage;
    if (store != NULL) {
        oryx_logd("store %p", store);
        if (store[id] != NULL) {
            StorageMapping *map = &storage_map[type][id];
            map->Free(store[id]);
            store[id] = NULL;
        }
    }
}

void StorageFreeAll(Storage *storage, StorageEnum type)
{
    if (storage == NULL)
        return;
#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
#ifdef UNITTESTS
    if (storage_map == NULL)
        return;
#endif

    Storage *store = storage;
    int i;
    for (i = 0; i < storage_max_id[type]; i++) {
        if (store[i] != NULL) {
            StorageMapping *map = &storage_map[type][i];
            map->Free(store[i]);
            store[i] = NULL;
        }
    }
}

void StorageFree(Storage **storage, StorageEnum type)
{
    if (*storage == NULL)
        return;

#ifdef DEBUG
    BUG_ON(!storage_registraton_closed);
#endif
#ifdef UNITTESTS
    if (storage_map == NULL)
        return;
#endif

    Storage *store = *storage;
    int i;
    for (i = 0; i < storage_max_id[type]; i++) {
        if (store[i] != NULL) {
            StorageMapping *map = &storage_map[type][i];
            map->Free(store[i]);
            store[i] = NULL;
        }
    }
    free(*storage);
    *storage = NULL;
}

