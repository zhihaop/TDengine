/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "index_cache.h"
#include "index_util.h"
#include "tcompare.h"
#include "tsched.h"

#define MAX_INDEX_KEY_LEN 256  // test only, change later

#define MEM_TERM_LIMIT 10 * 10000
// ref index_cache.h:22
//#define CACHE_KEY_LEN(p) \
//  (sizeof(int32_t) + sizeof(uint16_t) + sizeof(p->colType) + sizeof(p->nColVal) + p->nColVal + sizeof(uint64_t) +
//  sizeof(p->operType))

static void indexMemRef(MemTable* tbl);
static void indexMemUnRef(MemTable* tbl);

static void    cacheTermDestroy(CacheTerm* ct);
static char*   getIndexKey(const void* pData);
static int32_t compareKey(const void* l, const void* r);

static MemTable* indexInternalCacheCreate(int8_t type);

static void doMergeWork(SSchedMsg* msg);
static bool indexCacheIteratorNext(Iterate* itera);

static IterateValue* indexCacheIteratorGetValue(Iterate* iter);

IndexCache* indexCacheCreate(SIndex* idx, const char* colName, int8_t type) {
  IndexCache* cache = calloc(1, sizeof(IndexCache));
  if (cache == NULL) {
    indexError("failed to create index cache");
    return NULL;
  };
  cache->mem = indexInternalCacheCreate(type);

  cache->colName = calloc(1, strlen(colName) + 1);
  memcpy(cache->colName, colName, strlen(colName));
  cache->type = type;
  cache->index = idx;
  cache->version = 0;

  pthread_mutex_init(&cache->mtx, NULL);
  indexCacheRef(cache);
  return cache;
}
void indexCacheDebug(IndexCache* cache) {
  MemTable* tbl = NULL;

  pthread_mutex_lock(&cache->mtx);
  tbl = cache->mem;
  indexMemRef(tbl);
  pthread_mutex_unlock(&cache->mtx);

  {
    SSkipList*         slt = tbl->mem;
    SSkipListIterator* iter = tSkipListCreateIter(slt);
    while (tSkipListIterNext(iter)) {
      SSkipListNode* node = tSkipListIterGet(iter);
      CacheTerm*     ct = (CacheTerm*)SL_GET_NODE_DATA(node);
      if (ct != NULL) {
        // TODO, add more debug info
        indexInfo("{colVal: %s, version: %d} \t", ct->colVal, ct->version);
      }
    }
    tSkipListDestroyIter(iter);

    indexMemUnRef(tbl);
  }

  {
    pthread_mutex_lock(&cache->mtx);
    tbl = cache->imm;
    indexMemRef(tbl);
    pthread_mutex_unlock(&cache->mtx);
    if (tbl != NULL) {
      SSkipList*         slt = tbl->mem;
      SSkipListIterator* iter = tSkipListCreateIter(slt);
      while (tSkipListIterNext(iter)) {
        SSkipListNode* node = tSkipListIterGet(iter);
        CacheTerm*     ct = (CacheTerm*)SL_GET_NODE_DATA(node);
        if (ct != NULL) {
          // TODO, add more debug info
          indexInfo("{colVal: %s, version: %d} \t", ct->colVal, ct->version);
        }
      }
      tSkipListDestroyIter(iter);
    }

    indexMemUnRef(tbl);
  }
}

void indexCacheDestroySkiplist(SSkipList* slt) {
  SSkipListIterator* iter = tSkipListCreateIter(slt);
  while (tSkipListIterNext(iter)) {
    SSkipListNode* node = tSkipListIterGet(iter);
    CacheTerm*     ct = (CacheTerm*)SL_GET_NODE_DATA(node);
    if (ct != NULL) {
      free(ct->colVal);
      free(ct);
    }
  }
  tSkipListDestroyIter(iter);
  tSkipListDestroy(slt);
}
void indexCacheDestroyImm(IndexCache* cache) {
  if (cache == NULL) { return; }

  MemTable* tbl = NULL;
  pthread_mutex_lock(&cache->mtx);
  tbl = cache->imm;
  cache->imm = NULL;  // or throw int bg thread
  pthread_mutex_unlock(&cache->mtx);

  indexMemUnRef(tbl);
  indexMemUnRef(tbl);
}
void indexCacheDestroy(void* cache) {
  IndexCache* pCache = cache;
  if (pCache == NULL) { return; }
  indexMemUnRef(pCache->mem);
  indexMemUnRef(pCache->imm);
  free(pCache->colName);

  free(pCache);
}

Iterate* indexCacheIteratorCreate(IndexCache* cache) {
  Iterate* iiter = calloc(1, sizeof(Iterate));
  if (iiter == NULL) { return NULL; }

  pthread_mutex_lock(&cache->mtx);

  indexMemRef(cache->imm);

  MemTable* tbl = cache->imm;
  iiter->val.val = taosArrayInit(1, sizeof(uint64_t));
  iiter->iter = tbl != NULL ? tSkipListCreateIter(tbl->mem) : NULL;
  iiter->next = indexCacheIteratorNext;
  iiter->getValue = indexCacheIteratorGetValue;

  pthread_mutex_unlock(&cache->mtx);

  return iiter;
}
void indexCacheIteratorDestroy(Iterate* iter) {
  if (iter == NULL) { return; }
  tSkipListDestroyIter(iter->iter);
  iterateValueDestroy(&iter->val, true);
  free(iter);
}

int indexCacheSchedToMerge(IndexCache* pCache) {
  SSchedMsg schedMsg = {0};
  schedMsg.fp = doMergeWork;
  schedMsg.ahandle = pCache;
  schedMsg.thandle = NULL;
  schedMsg.msg = NULL;

  taosScheduleTask(indexQhandle, &schedMsg);
}
static void indexCacheMakeRoomForWrite(IndexCache* cache) {
  while (true) {
    if (cache->nTerm < MEM_TERM_LIMIT) {
      cache->nTerm += 1;
      break;
    } else if (cache->imm != NULL) {
      // TODO: wake up by condition variable
      pthread_mutex_unlock(&cache->mtx);
      taosMsleep(50);
      pthread_mutex_lock(&cache->mtx);
    } else {
      indexCacheRef(cache);
      cache->imm = cache->mem;
      cache->mem = indexInternalCacheCreate(cache->type);
      cache->nTerm = 1;
      // sched to merge
      // unref cache in bgwork
      indexCacheSchedToMerge(cache);
    }
  }
}

int indexCachePut(void* cache, SIndexTerm* term, uint64_t uid) {
  if (cache == NULL) { return -1; }

  IndexCache* pCache = cache;
  indexCacheRef(pCache);
  // encode data
  CacheTerm* ct = calloc(1, sizeof(CacheTerm));
  if (cache == NULL) { return -1; }
  // set up key
  ct->colType = term->colType;
  ct->colVal = (char*)calloc(1, sizeof(char) * (term->nColVal + 1));
  memcpy(ct->colVal, term->colVal, term->nColVal);
  ct->version = atomic_add_fetch_32(&pCache->version, 1);
  // set value
  ct->uid = uid;
  ct->operaType = term->operType;

  // ugly code, refactor later
  pthread_mutex_lock(&pCache->mtx);

  indexCacheMakeRoomForWrite(pCache);
  MemTable* tbl = pCache->mem;
  indexMemRef(tbl);
  tSkipListPut(tbl->mem, (char*)ct);
  indexMemUnRef(tbl);

  pthread_mutex_unlock(&pCache->mtx);

  indexCacheUnRef(pCache);
  return 0;
  // encode end
}
int indexCacheDel(void* cache, const char* fieldValue, int32_t fvlen, uint64_t uid, int8_t operType) {
  IndexCache* pCache = cache;
  return 0;
}

static int indexQueryMem(MemTable* mem, CacheTerm* ct, EIndexQueryType qtype, SArray* result, STermValueType* s) {
  if (mem == NULL) { return 0; }
  char* key = getIndexKey(ct);

  SSkipListIterator* iter = tSkipListCreateIterFromVal(mem->mem, key, TSDB_DATA_TYPE_BINARY, TSDB_ORDER_ASC);
  while (tSkipListIterNext(iter)) {
    SSkipListNode* node = tSkipListIterGet(iter);
    if (node != NULL) {
      CacheTerm* c = (CacheTerm*)SL_GET_NODE_DATA(node);
      if (c->operaType == ADD_VALUE || qtype == QUERY_TERM) {
        if (strcmp(c->colVal, ct->colVal) == 0) {
          taosArrayPush(result, &c->uid);
          *s = kTypeValue;
        } else {
          break;
        }
      } else if (c->operaType == DEL_VALUE) {
        // table is del, not need
        *s = kTypeDeletion;
        break;
      }
    }
  }
  tSkipListDestroyIter(iter);
  return 0;
}
int indexCacheSearch(void* cache, SIndexTermQuery* query, SArray* result, STermValueType* s) {
  if (cache == NULL) { return -1; }
  IndexCache* pCache = cache;

  MemTable *mem = NULL, *imm = NULL;
  pthread_mutex_lock(&pCache->mtx);
  mem = pCache->mem;
  imm = pCache->imm;
  indexMemRef(mem);
  indexMemRef(imm);
  pthread_mutex_unlock(&pCache->mtx);

  SIndexTerm*     term = query->term;
  EIndexQueryType qtype = query->qType;
  CacheTerm       ct = {.colVal = term->colVal, .version = atomic_load_32(&pCache->version)};
  // indexCacheDebug(pCache);

  int ret = indexQueryMem(mem, &ct, qtype, result, s);
  if (ret == 0 && *s != kTypeDeletion) {
    // continue search in imm
    ret = indexQueryMem(imm, &ct, qtype, result, s);
  }
  // cacheTermDestroy(ct);

  indexMemUnRef(mem);
  indexMemUnRef(imm);

  return ret;
}

void indexCacheRef(IndexCache* cache) {
  if (cache == NULL) { return; }
  int ref = T_REF_INC(cache);
  UNUSED(ref);
}
void indexCacheUnRef(IndexCache* cache) {
  if (cache == NULL) { return; }
  int ref = T_REF_DEC(cache);
  if (ref == 0) { indexCacheDestroy(cache); }
}

void indexMemRef(MemTable* tbl) {
  if (tbl == NULL) { return; }
  int ref = T_REF_INC(tbl);
  UNUSED(ref);
}
void indexMemUnRef(MemTable* tbl) {
  if (tbl == NULL) { return; }
  int ref = T_REF_DEC(tbl);
  if (ref == 0) {
    SSkipList* slt = tbl->mem;
    indexCacheDestroySkiplist(slt);
    free(tbl);
  }
}

static void cacheTermDestroy(CacheTerm* ct) {
  if (ct == NULL) { return; }
  free(ct->colVal);
  free(ct);
}
static char* getIndexKey(const void* pData) {
  CacheTerm* p = (CacheTerm*)pData;
  return (char*)p;
}

static int32_t compareKey(const void* l, const void* r) {
  CacheTerm* lt = (CacheTerm*)l;
  CacheTerm* rt = (CacheTerm*)r;

  // compare colVal
  int32_t cmp = strcmp(lt->colVal, rt->colVal);
  if (cmp == 0) { return rt->version - lt->version; }
  return cmp;
}

static MemTable* indexInternalCacheCreate(int8_t type) {
  MemTable* tbl = calloc(1, sizeof(MemTable));
  indexMemRef(tbl);
  if (type == TSDB_DATA_TYPE_BINARY) {
    tbl->mem = tSkipListCreate(MAX_SKIP_LIST_LEVEL, type, MAX_INDEX_KEY_LEN, compareKey, SL_ALLOW_DUP_KEY, getIndexKey);
  }
  return tbl;
}

static void doMergeWork(SSchedMsg* msg) {
  IndexCache* pCache = msg->ahandle;
  SIndex*     sidx = (SIndex*)pCache->index;
  indexFlushCacheTFile(sidx, pCache);
}
static bool indexCacheIteratorNext(Iterate* itera) {
  SSkipListIterator* iter = itera->iter;
  if (iter == NULL) { return false; }
  IterateValue* iv = &itera->val;
  if (iv->colVal != NULL && iv->val != NULL) {
    // indexError("value in cache: colVal: %s, size: %d", iv->colVal, (int)taosArrayGetSize(iv->val));
  }
  iterateValueDestroy(iv, false);

  bool next = tSkipListIterNext(iter);
  if (next) {
    SSkipListNode* node = tSkipListIterGet(iter);
    CacheTerm*     ct = (CacheTerm*)SL_GET_NODE_DATA(node);

    iv->type = ct->operaType;
    iv->colVal = calloc(1, strlen(ct->colVal) + 1);
    memcpy(iv->colVal, ct->colVal, strlen(ct->colVal));

    taosArrayPush(iv->val, &ct->uid);
  }
  return next;
}

static IterateValue* indexCacheIteratorGetValue(Iterate* iter) { return &iter->val; }
