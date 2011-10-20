#ifndef QDICT_H
#define QDICT_H

#include "qobject.h"
#include "qlist.h"
#include "qemu-queue.h"
#include <stdint.h>

#define QDICT_HASH_SIZE 512

typedef struct QDictEntry {
    char *key;
    QObject *value;
    QLIST_ENTRY(QDictEntry) next;
} QDictEntry;

typedef struct QDict {
    QObject_HEAD;
    size_t size;
    QLIST_HEAD(,QDictEntry) table[QDICT_HASH_SIZE];
} QDict;

/* Object API */
QDict *qdict_new(void);
size_t qdict_size(const QDict *qdict);
void qdict_put_obj(QDict *qdict, const char *key, QObject *value);
void qdict_del(QDict *qdict, const char *key);
int qdict_haskey(const QDict *qdict, const char *key);
QObject *qdict_get(const QDict *qdict, const char *key);
QDict *qobject_to_qdict(const QObject *obj);
void qdict_iter(const QDict *qdict,
                void (*iter)(const char *key, QObject *obj, void *opaque),
                void *opaque);

/* Helper to qdict_put_obj(), accepts any object */
#define qdict_put(qdict, key, obj) \
        qdict_put_obj(qdict, key, QOBJECT(obj))

/* High level helpers */
double qdict_get_double(const QDict *qdict, const char *key);
int64_t qdict_get_int(const QDict *qdict, const char *key);
int qdict_get_bool(const QDict *qdict, const char *key);
QList *qdict_get_qlist(const QDict *qdict, const char *key);
QDict *qdict_get_qdict(const QDict *qdict, const char *key);
const char *qdict_get_str(const QDict *qdict, const char *key);
int64_t qdict_get_try_int(const QDict *qdict, const char *key,
                          int64_t err_value);
const char *qdict_get_try_str(const QDict *qdict, const char *key);

#endif /* QDICT_H */
