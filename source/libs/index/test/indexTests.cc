/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3 * or later ("AGPL"), as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <thread>
#include "index.h"
#include "indexInt.h"
#include "index_cache.h"
#include "index_fst.h"
#include "index_fst_counting_writer.h"
#include "index_fst_util.h"
#include "index_tfile.h"
#include "tskiplist.h"
#include "tutil.h"
using namespace std;

#define NUM_OF_THREAD 10

class DebugInfo {
 public:
  DebugInfo(const char* str) : info(str) {
    std::cout << "------------" << info << "\t"
              << "begin"
              << "-------------" << std::endl;
  }
  ~DebugInfo() {
    std::cout << "-----------" << info << "\t"
              << "end"
              << "--------------" << std::endl;
  }

 private:
  std::string info;
};

class FstWriter {
 public:
  FstWriter() {
    _wc = writerCtxCreate(TFile, "/tmp/tindex", false, 64 * 1024 * 1024);
    _b = fstBuilderCreate(NULL, 0);
  }
  bool Put(const std::string& key, uint64_t val) {
    FstSlice skey = fstSliceCreate((uint8_t*)key.c_str(), key.size());
    bool     ok = fstBuilderInsert(_b, skey, val);
    fstSliceDestroy(&skey);
    return ok;
  }
  ~FstWriter() {
    fstBuilderFinish(_b);
    fstBuilderDestroy(_b);

    writerCtxDestroy(_wc, false);
  }

 private:
  FstBuilder* _b;
  WriterCtx*  _wc;
};

class FstReadMemory {
 public:
  FstReadMemory(size_t size) {
    _wc = writerCtxCreate(TFile, "/tmp/tindex", true, 64 * 1024);
    _w = fstCountingWriterCreate(_wc);
    _size = size;
    memset((void*)&_s, 0, sizeof(_s));
  }
  bool init() {
    char* buf = (char*)calloc(1, sizeof(char) * _size);
    int   nRead = fstCountingWriterRead(_w, (uint8_t*)buf, _size);
    if (nRead <= 0) { return false; }
    _size = nRead;
    _s = fstSliceCreate((uint8_t*)buf, _size);
    _fst = fstCreate(&_s);
    free(buf);
    return _fst != NULL;
  }
  bool Get(const std::string& key, uint64_t* val) {
    FstSlice skey = fstSliceCreate((uint8_t*)key.c_str(), key.size());
    bool     ok = fstGet(_fst, &skey, val);
    fstSliceDestroy(&skey);
    return ok;
  }
  bool GetWithTimeCostUs(const std::string& key, uint64_t* val, uint64_t* elapse) {
    int64_t s = taosGetTimestampUs();
    bool    ok = this->Get(key, val);
    int64_t e = taosGetTimestampUs();
    *elapse = e - s;
    return ok;
  }
  // add later
  bool Search(AutomationCtx* ctx, std::vector<uint64_t>& result) {
    FstStreamBuilder*      sb = fstSearch(_fst, ctx);
    StreamWithState*       st = streamBuilderIntoStream(sb);
    StreamWithStateResult* rt = NULL;

    while ((rt = streamWithStateNextWith(st, NULL)) != NULL) { result.push_back((uint64_t)(rt->out.out)); }
    return true;
  }
  bool SearchWithTimeCostUs(AutomationCtx* ctx, std::vector<uint64_t>& result) {
    int64_t s = taosGetTimestampUs();
    bool    ok = this->Search(ctx, result);
    int64_t e = taosGetTimestampUs();
    return ok;
  }

  ~FstReadMemory() {
    fstCountingWriterDestroy(_w);
    fstDestroy(_fst);
    fstSliceDestroy(&_s);
    writerCtxDestroy(_wc, true);
  }

 private:
  FstCountingWriter* _w;
  Fst*               _fst;
  FstSlice           _s;
  WriterCtx*         _wc;
  size_t             _size;
};

#define L 100
#define M 100
#define N 100

int Performance_fstWriteRecords(FstWriter* b) {
  std::string str("aa");
  for (int i = 0; i < L; i++) {
    str[0] = 'a' + i;
    str.resize(2);
    for (int j = 0; j < M; j++) {
      str[1] = 'a' + j;
      str.resize(2);
      for (int k = 0; k < N; k++) {
        str.push_back('a');
        b->Put(str, k);
        printf("(%d, %d, %d, %s)\n", i, j, k, str.c_str());
      }
    }
  }
  return L * M * N;
}
void Performance_fstReadRecords(FstReadMemory* m) {
  std::string str("aa");
  for (int i = 0; i < M; i++) {
    str[0] = 'a' + i;
    str.resize(2);
    for (int j = 0; j < N; j++) {
      str[1] = 'a' + j;
      str.resize(2);
      for (int k = 0; k < L; k++) {
        str.push_back('a');
        uint64_t val, cost;
        if (m->GetWithTimeCostUs(str, &val, &cost)) {
          printf("succes to get kv(%s, %" PRId64 "), cost: %" PRId64 "\n", str.c_str(), val, cost);
        } else {
          printf("failed to get key: %s\n", str.c_str());
        }
      }
    }
  }
}
void checkFstPerf() {
  FstWriter* fw = new FstWriter;
  int64_t    s = taosGetTimestampUs();

  int     num = Performance_fstWriteRecords(fw);
  int64_t e = taosGetTimestampUs();
  printf("write %d record cost %" PRId64 "us\n", num, e - s);
  delete fw;

  FstReadMemory* m = new FstReadMemory(1024 * 64);
  if (m->init()) { printf("success to init fst read"); }
  Performance_fstReadRecords(m);
  delete m;
}
void checkFstPrefixSearch() {
  FstWriter*  fw = new FstWriter;
  int64_t     s = taosGetTimestampUs();
  int         count = 2;
  std::string key("ab");

  for (int i = 0; i < count; i++) {
    key[1] = key[1] + i;
    fw->Put(key, i);
  }
  int64_t e = taosGetTimestampUs();

  std::cout << "insert data count :  " << count << "elapas time: " << e - s << std::endl;
  delete fw;

  FstReadMemory* m = new FstReadMemory(1024 * 64);
  if (m->init() == false) {
    std::cout << "init readMemory failed" << std::endl;
    delete m;
    return;
  }

  // prefix search
  std::vector<uint64_t> result;

  AutomationCtx* ctx = automCtxCreate((void*)"ab", AUTOMATION_PREFIX);
  m->Search(ctx, result);
  assert(result.size() == count);
  for (int i = 0; i < result.size(); i++) {
    assert(result[i] == i);  // check result
  }

  free(ctx);
  delete m;
}
void validateFst() {
  int        val = 100;
  int        count = 100;
  FstWriter* fw = new FstWriter;
  // write
  {
    std::string key("ab");
    for (int i = 0; i < count; i++) {
      key.push_back('a' + i);
      fw->Put(key, val - i);
    }
  }
  delete fw;

  // read
  FstReadMemory* m = new FstReadMemory(1024 * 64);
  if (m->init() == false) {
    std::cout << "init readMemory failed" << std::endl;
    delete m;
    return;
  }

  {
    std::string key("ab");
    uint64_t    out;
    if (m->Get(key, &out)) {
      printf("success to get (%s, %" PRId64 ")\n", key.c_str(), out);
    } else {
      printf("failed to get(%s)\n", key.c_str());
    }
    for (int i = 0; i < count; i++) {
      key.push_back('a' + i);
      if (m->Get(key, &out)) {
        assert(val - i == out);
        printf("success to get (%s, %" PRId64 ")\n", key.c_str(), out);
      } else {
        printf("failed to get(%s)\n", key.c_str());
      }
    }
  }
  delete m;
}
class IndexEnv : public ::testing::Test {
 protected:
  virtual void SetUp() {
    taosRemoveDir(path);
    opts = indexOptsCreate();
    int ret = indexOpen(opts, path, &index);
    assert(ret == 0);
  }
  virtual void TearDown() {
    indexClose(index);
    indexOptsDestroy(opts);
  }

  const char* path = "/tmp/tindex";
  SIndexOpts* opts;
  SIndex*     index;
};

/// TEST_F(IndexEnv, testPut) {
//  /  // single index column
//      / {
//    / std::string colName("tag1"), colVal("Hello world");
//    / SIndexTerm* term =
//        indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(), colVal.c_str(), /
//        colVal.size());
//    SIndexMultiTerm* terms = indexMultiTermCreate();
//    indexMultiTermAdd(terms, term);
//    / / for (size_t i = 0; i < 100; i++) {
//      / int tableId = i;
//      / int ret = indexPut(index, terms, tableId);
//      / assert(ret == 0);
//      /
//    }
//    / indexMultiTermDestroy(terms);
//    /
//  }
//  /  // multi index column
//      / {
//    / SIndexMultiTerm* terms = indexMultiTermCreate();
//    / {
//      / std::string colName("tag1"), colVal("Hello world");
//      / SIndexTerm* term =
//          / indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(), colVal.c_str(),
//          colVal.size());
//      / indexMultiTermAdd(terms, term);
//      /
//    }
//    / {
//      / std::string colName("tag2"), colVal("Hello world");
//      / SIndexTerm* term =
//          / indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(), colVal.c_str(),
//          colVal.size());
//      / indexMultiTermAdd(terms, term);
//      /
//    }
//    / / for (int i = 0; i < 100; i++) {
//      / int tableId = i;
//      / int ret = indexPut(index, terms, tableId);
//      / assert(ret == 0);
//      /
//    }
//    / indexMultiTermDestroy(terms);
//    /
//  }
//  /  //
//      /
//}

class TFileObj {
 public:
  TFileObj(const std::string& path = "/tmp/tindex", const std::string& colName = "voltage")
      : path_(path), colName_(colName) {
    colId_ = 10;
    reader_ = NULL;
    writer_ = NULL;
    // Do Nothing
    //
  }
  int Put(SArray* tv) {
    if (reader_ != NULL) {
      tfileReaderDestroy(reader_);
      reader_ = NULL;
    }
    if (writer_ == NULL) { InitWriter(); }
    return tfileWriterPut(writer_, tv, false);
  }
  bool InitWriter() {
    TFileHeader header;
    header.suid = 1;
    header.version = 1;
    memcpy(header.colName, colName_.c_str(), colName_.size());
    header.colType = TSDB_DATA_TYPE_BINARY;

    std::string path(path_);
    int         colId = 2;
    char        buf[64] = {0};
    sprintf(buf, "%" PRIu64 "-%d-%d.tindex", header.suid, colId_, header.version);
    path.append("/").append(buf);

    fileName_ = path;

    WriterCtx* ctx = writerCtxCreate(TFile, path.c_str(), false, 64 * 1024 * 1024);

    writer_ = tfileWriterCreate(ctx, &header);
    return writer_ != NULL ? true : false;
  }
  bool InitReader() {
    WriterCtx* ctx = writerCtxCreate(TFile, fileName_.c_str(), true, 64 * 1024 * 1024);
    reader_ = tfileReaderCreate(ctx);
    return reader_ != NULL ? true : false;
  }
  int Get(SIndexTermQuery* query, SArray* result) {
    if (writer_ != NULL) {
      tfileWriterDestroy(writer_);
      writer_ = NULL;
    }
    if (reader_ == NULL && InitReader()) {
      //
      //
    }
    return tfileReaderSearch(reader_, query, result);
  }
  ~TFileObj() {
    if (writer_) { tfileWriterDestroy(writer_); }
    if (reader_) { tfileReaderDestroy(reader_); }
  }

 private:
  std::string path_;
  std::string colName_;
  std::string fileName_;

  TFileWriter* writer_;
  TFileReader* reader_;

  int colId_;
};

class IndexTFileEnv : public ::testing::Test {
 protected:
  virtual void SetUp() {
    taosRemoveDir(dir.c_str());
    taosMkDir(dir.c_str());
    tfInit();
    fObj = new TFileObj(dir, colName);
  }

  virtual void TearDown() {
    // indexClose(index);
    // indexeptsDestroy(opts);
    delete fObj;
    tfCleanup();
    // tfileWriterDestroy(twrite);
  }
  TFileObj*   fObj;
  std::string dir = "/tmp/tindex";
  std::string colName = "voltage";

  int coldId = 2;
  int version = 1;
  int colType = TSDB_DATA_TYPE_BINARY;
};

static TFileValue* genTFileValue(const char* val) {
  TFileValue* tv = (TFileValue*)calloc(1, sizeof(TFileValue));
  int32_t     vlen = strlen(val) + 1;
  tv->colVal = (char*)calloc(1, vlen);
  memcpy(tv->colVal, val, vlen);

  tv->tableId = (SArray*)taosArrayInit(1, sizeof(uint64_t));
  for (size_t i = 0; i < 200; i++) {
    uint64_t v = i;
    taosArrayPush(tv->tableId, &v);
  }
  return tv;
}
static void destroyTFileValue(void* val) {
  TFileValue* tv = (TFileValue*)val;
  free(tv->colVal);
  taosArrayDestroy(tv->tableId);
  free(tv);
}
TEST_F(IndexTFileEnv, test_tfile_write) {
  TFileValue* v1 = genTFileValue("ab");

  SArray* data = (SArray*)taosArrayInit(4, sizeof(void*));

  taosArrayPush(data, &v1);
  // taosArrayPush(data, &v2);
  // taosArrayPush(data, &v3);
  // taosArrayPush(data, &v4);

  fObj->Put(data);
  for (size_t i = 0; i < taosArrayGetSize(data); i++) { destroyTFileValue(taosArrayGetP(data, i)); }
  taosArrayDestroy(data);

  std::string     colName("voltage");
  std::string     colVal("ab");
  SIndexTerm*     term = indexTermCreate(1, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                     colVal.c_str(), colVal.size());
  SIndexTermQuery query = {.term = term, .qType = QUERY_TERM};

  SArray* result = (SArray*)taosArrayInit(1, sizeof(uint64_t));
  fObj->Get(&query, result);
  assert(taosArrayGetSize(result) == 200);
  indexTermDestroy(term);

  // tfileWriterDestroy(twrite);
}
class CacheObj {
 public:
  CacheObj() {
    // TODO
    cache = indexCacheCreate(NULL, "voltage", TSDB_DATA_TYPE_BINARY);
  }
  int Put(SIndexTerm* term, int16_t colId, int32_t version, uint64_t uid) {
    int ret = indexCachePut(cache, term, uid);
    if (ret != 0) {
      //
      std::cout << "failed to put into cache: " << ret << std::endl;
    }
    return ret;
  }
  void Debug() {
    //
    indexCacheDebug(cache);
  }
  int Get(SIndexTermQuery* query, int16_t colId, int32_t version, SArray* result, STermValueType* s) {
    int ret = indexCacheSearch(cache, query, result, s);
    if (ret != 0) {
      //
      std::cout << "failed to get from cache:" << ret << std::endl;
    }
    return ret;
  }
  ~CacheObj() {
    // TODO
    indexCacheDestroy(cache);
  }

 private:
  IndexCache* cache = NULL;
};

class IndexCacheEnv : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // TODO
    coj = new CacheObj();
  }
  virtual void TearDown() {
    delete coj;
    // formate
  }
  CacheObj* coj;
};

#define MAX_TERM_KEY_LEN 128
TEST_F(IndexCacheEnv, cache_test) {
  int     version = 0;
  int16_t colId = 0;
  int16_t othColId = 10;

  uint64_t    suid = 0;
  std::string colName("voltage");
  {
    std::string colVal("v1");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, colId, version++, suid++);
    // indexTermDestry(term);
  }
  {
    std::string colVal("v3");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, colId, version++, suid++);
  }
  {
    std::string colVal("v2");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, colId, version++, suid++);
  }
  {
    std::string colVal("v3");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, colId, version++, suid++);
  }
  {
    std::string colVal("v3");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, colId, version++, suid++);
  }
  coj->Debug();
  std::cout << "--------first----------" << std::endl;
  {
    std::string colVal("v3");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, othColId, version++, suid++);
  }
  {
    std::string colVal("v4");
    SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    coj->Put(term, othColId, version++, suid++);
  }
  coj->Debug();
  std::cout << "--------second----------" << std::endl;
  {
    std::string colVal("v4");
    for (size_t i = 0; i < 10; i++) {
      colVal[colVal.size() - 1] = 'a' + i;
      SIndexTerm* term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                         colVal.c_str(), colVal.size());
      coj->Put(term, colId, version++, suid++);
    }
  }
  coj->Debug();
  // begin query
  {
    std::string     colVal("v3");
    SIndexTerm*     term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexTermQuery query = {.term = term, .qType = QUERY_TERM};
    SArray*         ret = (SArray*)taosArrayInit(4, sizeof(suid));
    STermValueType  valType;

    coj->Get(&query, colId, 10000, ret, &valType);
    std::cout << "size : " << taosArrayGetSize(ret) << std::endl;
    assert(taosArrayGetSize(ret) == 4);
  }
  {
    std::string     colVal("v2");
    SIndexTerm*     term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexTermQuery query = {.term = term, .qType = QUERY_TERM};
    SArray*         ret = (SArray*)taosArrayInit(4, sizeof(suid));
    STermValueType  valType;

    coj->Get(&query, colId, 10000, ret, &valType);
    assert(taosArrayGetSize(ret) == 1);
  }
}
class IndexObj {
 public:
  IndexObj() {
    // opt
    numOfWrite = 0;
    numOfRead = 0;
    indexInit();
  }
  int Init(const std::string& dir) {
    taosRemoveDir(dir.c_str());
    taosMkDir(dir.c_str());
    int ret = indexOpen(&opts, dir.c_str(), &idx);
    if (ret != 0) {
      // opt
      std::cout << "failed to open index: %s" << dir << std::endl;
    }
    return ret;
  }
  int WriteMillonData(const std::string& colName, const std::string& colVal = "Hello world",
                      size_t numOfTable = 100 * 10000) {
    SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexMultiTerm* terms = indexMultiTermCreate();
    indexMultiTermAdd(terms, term);
    for (size_t i = 0; i < numOfTable; i++) {
      int ret = Put(terms, i);
      assert(ret == 0);
    }
    indexMultiTermDestroy(terms);
    return numOfTable;
  }
  int WriteMultiMillonData(const std::string& colName, const std::string& colVal = "Hello world",
                           size_t numOfTable = 100 * 10000) {
    std::string tColVal = colVal;
    for (int i = 0; i < numOfTable; i++) {
      tColVal[tColVal.size() - 1] = 'a' + i % 26;
      SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                         colVal.c_str(), colVal.size());
      SIndexMultiTerm* terms = indexMultiTermCreate();
      indexMultiTermAdd(terms, term);
      for (size_t i = 0; i < 10; i++) {
        int ret = Put(terms, i);
        assert(ret == 0);
      }
      indexMultiTermDestroy(terms);
    }
    return numOfTable;
  }

  int Put(SIndexMultiTerm* fvs, uint64_t uid) {
    numOfWrite += taosArrayGetSize(fvs);
    return indexPut(idx, fvs, uid);
  }
  int Search(SIndexMultiTermQuery* multiQ, SArray* result) {
    SArray* query = multiQ->query;
    numOfRead = taosArrayGetSize(query);
    return indexSearch(idx, multiQ, result);
  }

  int SearchOne(const std::string& colName, const std::string& colVal) {
    SIndexMultiTermQuery* mq = indexMultiTermQueryCreate(MUST);
    SIndexTerm*           term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    indexMultiTermQueryAdd(mq, term, QUERY_TERM);

    SArray* result = (SArray*)taosArrayInit(1, sizeof(uint64_t));
    if (Search(mq, result) == 0) { std::cout << "search one successfully" << std::endl; }
    return taosArrayGetSize(result);
    // assert(taosArrayGetSize(result) == targetSize);
  }
  void PutOne(const std::string& colName, const std::string& colVal) {
    SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexMultiTerm* terms = indexMultiTermCreate();
    indexMultiTermAdd(terms, term);
    Put(terms, 10);
    indexMultiTermDestroy(terms);
  }
  void Debug() {
    std::cout << "numOfWrite:" << numOfWrite << std::endl;
    std::cout << "numOfRead:" << numOfRead << std::endl;
  }

  ~IndexObj() {
    indexCleanUp();
    indexClose(idx);
  }

 private:
  SIndexOpts opts;
  SIndex*    idx;
  int        numOfWrite;
  int        numOfRead;
};

class IndexEnv2 : public ::testing::Test {
 protected:
  virtual void SetUp() {
    tfInit();
    index = new IndexObj();
    //
  }
  virtual void TearDown() {
    delete index;
    tfCleanup();
  }
  IndexObj* index;
};
TEST_F(IndexEnv2, testIndexOpen) {
  std::string path = "/tmp/test";
  if (index->Init(path) != 0) {
    std::cout << "failed to init index" << std::endl;
    exit(1);
  }

  int targetSize = 200;
  {
    std::string colName("tag1"), colVal("Hello");

    SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexMultiTerm* terms = indexMultiTermCreate();
    indexMultiTermAdd(terms, term);
    for (size_t i = 0; i < targetSize; i++) {
      int tableId = i;
      int ret = index->Put(terms, tableId);
      assert(ret == 0);
    }
    indexMultiTermDestroy(terms);
  }
  {
    size_t      size = 200;
    std::string colName("tag1"), colVal("hello");

    SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexMultiTerm* terms = indexMultiTermCreate();
    indexMultiTermAdd(terms, term);
    for (size_t i = 0; i < size; i++) {
      int tableId = i;
      int ret = index->Put(terms, tableId);
      assert(ret == 0);
    }
    indexMultiTermDestroy(terms);
  }
  {
    size_t      size = 200;
    std::string colName("tag1"), colVal("Hello");

    SIndexTerm*      term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    SIndexMultiTerm* terms = indexMultiTermCreate();
    indexMultiTermAdd(terms, term);
    for (size_t i = size * 3; i < size * 4; i++) {
      int tableId = i;
      int ret = index->Put(terms, tableId);
      assert(ret == 0);
    }
    indexMultiTermDestroy(terms);
  }

  {
    std::string           colName("tag1"), colVal("Hello");
    SIndexMultiTermQuery* mq = indexMultiTermQueryCreate(MUST);
    SIndexTerm*           term = indexTermCreate(0, ADD_VALUE, TSDB_DATA_TYPE_BINARY, colName.c_str(), colName.size(),
                                       colVal.c_str(), colVal.size());
    indexMultiTermQueryAdd(mq, term, QUERY_TERM);

    SArray* result = (SArray*)taosArrayInit(1, sizeof(uint64_t));
    index->Search(mq, result);
    std::cout << "target size: " << taosArrayGetSize(result) << std::endl;
    assert(taosArrayGetSize(result) == 400);
  }
}

TEST_F(IndexEnv2, testIndex_TrigeFlush) {
  std::string path = "/tmp/test1";
  if (index->Init(path) != 0) {
    // r
    std::cout << "failed to init" << std::endl;
  }
  int numOfTable = 100 * 10000;
  index->WriteMillonData("tag1", "Hello Wolrd", numOfTable);
  int target = index->SearchOne("tag1", "Hello Wolrd");
  std::cout << "Get Index: " << target << std::endl;
  assert(numOfTable == target);
}

static void write_and_search(IndexObj* idx) {
  std::string colName("tag1"), colVal("Hello");

  int target = idx->SearchOne("tag1", "Hello");
  idx->PutOne(colName, colVal);
}
TEST_F(IndexEnv2, testIndex_serarch_cache_and_tfile) {
  std::string path = "/tmp/cache_and_tfile";
  if (index->Init(path) != 0) {
    // opt
  }
  index->WriteMultiMillonData("tag1", "Hello", 200000);
  std::thread threads[NUM_OF_THREAD];

  for (int i = 0; i < NUM_OF_THREAD; i++) {
    //
    threads[i] = std::thread(write_and_search, index);
  }
  for (int i = 0; i < NUM_OF_THREAD; i++) {
    // TOD
    threads[i].join();
  }
}

TEST_F(IndexEnv2, testIndex_restart) {
  std::string path = "/tmp";
  if (index->Init(path) != 0) {}
}

TEST_F(IndexEnv2, testIndex_performance) {
  std::string path = "/tmp";
  if (index->Init(path) != 0) {}
}
TEST_F(IndexEnv2, testIndexMultiTag) {
  std::string path = "/tmp";
  if (index->Init(path) != 0) {}
}
