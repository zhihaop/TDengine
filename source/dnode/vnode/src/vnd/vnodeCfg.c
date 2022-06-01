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

#include "vnd.h"

const SVnodeCfg vnodeCfgDefault = {
    .vgId = -1,
    .dbname = "",
    .dbId = 0,
    .szPage = 4096,
    .szCache = 256,
    .szBuf = 96 * 1024 * 1024,
    .isHeap = false,
    .isWeak = 0,
    .tsdbCfg = {.precision = TSDB_TIME_PRECISION_MILLI,
                .update = 1,
                .compression = 2,
                .slLevel = 5,
                .days = 10,
                .minRows = 100,
                .maxRows = 4096,
                .keep2 = 3650,
                .keep0 = 3650,
                .keep1 = 3650},
    .walCfg =
        {.vgId = -1, .fsyncPeriod = 0, .retentionPeriod = 0, .rollPeriod = 0, .segSize = 0, .level = TAOS_WAL_WRITE},
    .hashBegin = 0,
    .hashEnd = 0,
    .hashMethod = 0};

int vnodeCheckCfg(const SVnodeCfg *pCfg) {
  // TODO
  return 0;
}

int vnodeEncodeConfig(const void *pObj, SJson *pJson) {
  const SVnodeCfg *pCfg = (SVnodeCfg *)pObj;

  if (tjsonAddIntegerToObject(pJson, "vgId", pCfg->vgId) < 0) return -1;
  if (tjsonAddStringToObject(pJson, "dbname", pCfg->dbname) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "dbId", pCfg->dbId) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "szPage", pCfg->szPage) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "szCache", pCfg->szCache) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "szBuf", pCfg->szBuf) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "isHeap", pCfg->isHeap) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "isWeak", pCfg->isWeak) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "isTsma", pCfg->isTsma) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "precision", pCfg->tsdbCfg.precision) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "update", pCfg->tsdbCfg.update) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "compression", pCfg->tsdbCfg.compression) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "slLevel", pCfg->tsdbCfg.slLevel) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "daysPerFile", pCfg->tsdbCfg.days) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "minRows", pCfg->tsdbCfg.minRows) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "maxRows", pCfg->tsdbCfg.maxRows) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "keep0", pCfg->tsdbCfg.keep0) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "keep1", pCfg->tsdbCfg.keep1) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "keep2", pCfg->tsdbCfg.keep2) < 0) return -1;
  if (pCfg->tsdbCfg.retentions[0].freq > 0) {
    int32_t nRetention = 1;
    if (pCfg->tsdbCfg.retentions[1].freq > 0) {
      ++nRetention;
      if (pCfg->tsdbCfg.retentions[2].freq > 0) {
        ++nRetention;
      }
    }
    SJson *pNodeRetentions = tjsonCreateArray();
    tjsonAddItemToObject(pJson, "retentions", pNodeRetentions);
    for (int32_t i = 0; i < nRetention; ++i) {
      SJson      *pNodeRetention = tjsonCreateObject();
      const SRetention *pRetention = pCfg->tsdbCfg.retentions + i;
      tjsonAddIntegerToObject(pNodeRetention, "freq", pRetention->freq);
      tjsonAddIntegerToObject(pNodeRetention, "freqUnit", pRetention->freqUnit);
      tjsonAddIntegerToObject(pNodeRetention, "keep", pRetention->keep);
      tjsonAddIntegerToObject(pNodeRetention, "keepUnit", pRetention->keepUnit);
      tjsonAddItemToArray(pNodeRetentions, pNodeRetention);
    }
  }
  if (tjsonAddIntegerToObject(pJson, "wal.vgId", pCfg->walCfg.vgId) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.fsyncPeriod", pCfg->walCfg.fsyncPeriod) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.retentionPeriod", pCfg->walCfg.retentionPeriod) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.rollPeriod", pCfg->walCfg.rollPeriod) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.retentionSize", pCfg->walCfg.retentionSize) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.segSize", pCfg->walCfg.segSize) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "wal.level", pCfg->walCfg.level) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "hashBegin", pCfg->hashBegin) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "hashEnd", pCfg->hashEnd) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "hashMethod", pCfg->hashMethod) < 0) return -1;

  if (tjsonAddIntegerToObject(pJson, "syncCfg.replicaNum", pCfg->syncCfg.replicaNum) < 0) return -1;
  if (tjsonAddIntegerToObject(pJson, "syncCfg.myIndex", pCfg->syncCfg.myIndex) < 0) return -1;
  SJson *pNodeInfoArr = tjsonCreateArray();
  tjsonAddItemToObject(pJson, "syncCfg.nodeInfo", pNodeInfoArr);
  for (int i = 0; i < pCfg->syncCfg.replicaNum; ++i) {
    SJson *pNodeInfo = tjsonCreateObject();
    tjsonAddIntegerToObject(pNodeInfo, "nodePort", (pCfg->syncCfg.nodeInfo)[i].nodePort);
    tjsonAddStringToObject(pNodeInfo, "nodeFqdn", (pCfg->syncCfg.nodeInfo)[i].nodeFqdn);
    tjsonAddItemToArray(pNodeInfoArr, pNodeInfo);
  }

  return 0;
}

int vnodeDecodeConfig(const SJson *pJson, void *pObj) {
  SVnodeCfg *pCfg = (SVnodeCfg *)pObj;

  int32_t code;
  tjsonGetNumberValue(pJson, "vgId", pCfg->vgId, code);
  if(code < 0) return -1;
  if (tjsonGetStringValue(pJson, "dbname", pCfg->dbname) < 0) return -1;
  tjsonGetNumberValue(pJson, "dbId", pCfg->dbId, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "szPage", pCfg->szPage, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "szCache", pCfg->szCache, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "szBuf", pCfg->szBuf, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "isHeap", pCfg->isHeap, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "isWeak", pCfg->isWeak, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "isTsma", pCfg->isTsma, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "precision", pCfg->tsdbCfg.precision, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "update", pCfg->tsdbCfg.update, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "compression", pCfg->tsdbCfg.compression, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "slLevel", pCfg->tsdbCfg.slLevel, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "daysPerFile", pCfg->tsdbCfg.days, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "minRows", pCfg->tsdbCfg.minRows, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "maxRows", pCfg->tsdbCfg.maxRows, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "keep0", pCfg->tsdbCfg.keep0, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "keep1", pCfg->tsdbCfg.keep1, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "keep2", pCfg->tsdbCfg.keep2, code);
  if(code < 0) return -1;
  SJson *pNodeRetentions = tjsonGetObjectItem(pJson, "retentions");
  int32_t nRetention = tjsonGetArraySize(pNodeRetentions);
  if (nRetention > TSDB_RETENTION_MAX) {
    nRetention = TSDB_RETENTION_MAX;
  }
  for (int32_t i = 0; i < nRetention; ++i) {
    SJson *pNodeRetention = tjsonGetArrayItem(pNodeRetentions, i);
    ASSERT(pNodeRetention != NULL);
    tjsonGetNumberValue(pNodeRetention, "freq", (pCfg->tsdbCfg.retentions)[i].freq, code);
    tjsonGetNumberValue(pNodeRetention, "freqUnit", (pCfg->tsdbCfg.retentions)[i].freqUnit, code);
    tjsonGetNumberValue(pNodeRetention, "keep", (pCfg->tsdbCfg.retentions)[i].keep, code);
    tjsonGetNumberValue(pNodeRetention, "keepUnit", (pCfg->tsdbCfg.retentions)[i].keepUnit, code);
  }
  tjsonGetNumberValue(pJson, "wal.vgId", pCfg->walCfg.vgId, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.fsyncPeriod", pCfg->walCfg.fsyncPeriod, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.retentionPeriod", pCfg->walCfg.retentionPeriod, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.rollPeriod", pCfg->walCfg.rollPeriod, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.retentionSize", pCfg->walCfg.retentionSize, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.segSize", pCfg->walCfg.segSize, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "wal.level", pCfg->walCfg.level, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "hashBegin", pCfg->hashBegin, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "hashEnd", pCfg->hashEnd, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "hashMethod", pCfg->hashMethod, code);
  if(code < 0) return -1;

  tjsonGetNumberValue(pJson, "syncCfg.replicaNum", pCfg->syncCfg.replicaNum, code);
  if(code < 0) return -1;
  tjsonGetNumberValue(pJson, "syncCfg.myIndex", pCfg->syncCfg.myIndex, code);
  if(code < 0) return -1;

  SJson *pNodeInfoArr = tjsonGetObjectItem(pJson, "syncCfg.nodeInfo");
  int    arraySize = tjsonGetArraySize(pNodeInfoArr);
  assert(arraySize == pCfg->syncCfg.replicaNum);

  for (int i = 0; i < arraySize; ++i) {
    SJson *pNodeInfo = tjsonGetArrayItem(pNodeInfoArr, i);
    assert(pNodeInfo != NULL);
    tjsonGetNumberValue(pNodeInfo, "nodePort", (pCfg->syncCfg.nodeInfo)[i].nodePort, code);
    tjsonGetStringValue(pNodeInfo, "nodeFqdn", (pCfg->syncCfg.nodeInfo)[i].nodeFqdn);
  }

  return 0;
}

int vnodeValidateTableHash(SVnode *pVnode, char *tableFName) {
  uint32_t hashValue = 0;

  switch (pVnode->config.hashMethod) {
    default:
      hashValue = MurmurHash3_32(tableFName, strlen(tableFName));
      break;
  }

    // TODO OPEN THIS !!!!!!!
#if 0
  if (hashValue < pVnodeOptions->hashBegin || hashValue > pVnodeOptions->hashEnd) {
    terrno = TSDB_CODE_VND_HASH_MISMATCH;
    return TSDB_CODE_VND_HASH_MISMATCH;
  }
#endif

  return 0;
}