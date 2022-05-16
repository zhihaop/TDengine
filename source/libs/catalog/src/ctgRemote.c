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

#include "trpc.h"
#include "query.h"
#include "tname.h"
#include "catalogInt.h"
#include "systable.h"

int32_t ctgProcessRspMsg(void* out, int32_t reqType, char* msg, int32_t msgSize, int32_t rspCode, char* target) {
  int32_t code = 0;
  
  switch (reqType) {
    case TDMT_MND_QNODE_LIST: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        ctgError("error rsp for qnode list, error:%s", tstrerror(rspCode));
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process qnode list rsp failed, error:%s", tstrerror(rspCode));
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got qnode list from mnode, listNum:%d", (int32_t)taosArrayGetSize(out));
      break;
    }
    case TDMT_MND_USE_DB: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        ctgError("error rsp for use db, error:%s, dbFName:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process use db rsp failed, error:%s, dbFName:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got db vgInfo from mnode, dbFName:%s", target);
      break;
    }
    case TDMT_MND_GET_DB_CFG: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        ctgError("error rsp for get db cfg, error:%s, db:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process get db cfg rsp failed, error:%s, db:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got db cfg from mnode, dbFName:%s", target);
      break;
    }
    case TDMT_MND_GET_INDEX: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        ctgError("error rsp for get index, error:%s, indexName:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process get index rsp failed, error:%s, indexName:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got index from mnode, indexName:%s", target);
      break;
    }
    case TDMT_MND_RETRIEVE_FUNC: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        if (TSDB_CODE_MND_FUNC_NOT_EXIST == rspCode) {
          ctgDebug("funcName %s not exist in mnode", target);
          taosMemoryFreeClear(*out);
          CTG_RET(TSDB_CODE_SUCCESS);
        }
        
        ctgError("error rsp for get udf, error:%s, funcName:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process get udf rsp failed, error:%s, funcName:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got udf from mnode, funcName:%s", target);
      break;
    }
    case TDMT_MND_GET_USER_AUTH: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        ctgError("error rsp for get user auth, error:%s, user:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process get user auth rsp failed, error:%s, user:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got user auth from mnode, user:%s", target);
      break;
    }
    case TDMT_MND_TABLE_META: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        if (CTG_TABLE_NOT_EXIST(rspCode)) {
          SET_META_TYPE_NULL(((STableMetaOutput*)out)->metaType);
          ctgDebug("stablemeta not exist in mnode, tbFName:%s", target);
          return TSDB_CODE_SUCCESS;
        }
        
        ctgError("error rsp for stablemeta from mnode, error:%s, tbFName:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process mnode stablemeta rsp failed, error:%s, tbFName:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got table meta from mnode, tbFName:%s", target);
      break;
    }
    case TDMT_VND_TABLE_META: {
      if (TSDB_CODE_SUCCESS != rspCode) {
        if (CTG_TABLE_NOT_EXIST(rspCode)) {
          SET_META_TYPE_NULL(((STableMetaOutput*)out)->metaType);
          ctgDebug("tablemeta not exist in vnode, tbFName:%s", target);
          return TSDB_CODE_SUCCESS;
        }
      
        ctgError("error rsp for table meta from vnode, code:%s, tbFName:%s", tstrerror(rspCode), target);
        CTG_ERR_RET(rspCode);
      }
      
      code = queryProcessMsgRsp[TMSG_INDEX(reqType)](out, msg, msgSize);
      if (code) {
        ctgError("Process vnode tablemeta rsp failed, code:%s, tbFName:%s", tstrerror(code), target);
        CTG_ERR_RET(code);
      }
      
      ctgDebug("Got table meta from vnode, tbFName:%s", target);
      break;
    }
  }
}


int32_t ctgGetQnodeListFromMnode(CTG_PARAMS, SArray *out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_QNODE_LIST;

  ctgDebug("try to get qnode list from mnode, mgmtEpInUse:%d", pMgmtEps->inUse);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)](NULL, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build qnode list msg failed, error:%s", tstrerror(code));
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosArrayInit(4, sizeof(struct SQueryNodeAddr));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, NULL));
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, NULL));

  return TSDB_CODE_SUCCESS;
}


int32_t ctgGetDBVgInfoFromMnode(CTG_PARAMS, SBuildUseDBInput *input, SUseDbOutput *out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_USE_DB;

  ctgDebug("try to get db vgInfo from mnode, dbFName:%s", input->db);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)](input, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build use db msg failed, code:%x, db:%s", code, input->db);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(SUseDbOutput));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, input->db));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, input->db));
  
  return TSDB_CODE_SUCCESS;
}

int32_t ctgGetDBCfgFromMnode(CTG_PARAMS, const char *dbFName, SDbCfgInfo *out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_GET_DB_CFG;

  ctgDebug("try to get db cfg from mnode, dbFName:%s", dbFName);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)]((void *)dbFName, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build get db cfg msg failed, code:%x, db:%s", code, dbFName);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(SDbCfgInfo));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, dbFName));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = TDMT_MND_GET_DB_CFG,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, dbFName));

  return TSDB_CODE_SUCCESS;
}

int32_t ctgGetIndexInfoFromMnode(CTG_PARAMS, const char *indexName, SIndexInfo *out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_GET_INDEX;

  ctgDebug("try to get index from mnode, indexName:%s", indexName);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)]((void *)indexName, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build get index msg failed, code:%x, db:%s", code, indexName);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(SIndexInfo));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, indexName));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, indexName));
  
  return TSDB_CODE_SUCCESS;
}

int32_t ctgGetUdfInfoFromMnode(CTG_PARAMS, const char *funcName, SFuncInfo **out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_RETRIEVE_FUNC;

  ctgDebug("try to get udf info from mnode, funcName:%s", funcName);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)]((void *)funcName, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build get udf msg failed, code:%x, db:%s", code, funcName);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, POINTER_BYTES);
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, funcName));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, funcName));

  return TSDB_CODE_SUCCESS;
}

int32_t ctgGetUserDbAuthFromMnode(CTG_PARAMS, const char *user, SGetUserAuthRsp *out, SCtgTask* pTask) {
  char *msg = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_GET_USER_AUTH;

  ctgDebug("try to get user auth from mnode, user:%s", user);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)]((void *)user, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build get user auth msg failed, code:%x, db:%s", code, user);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(SGetUserAuthRsp));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, user));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }
  
  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, user));
  
  return TSDB_CODE_SUCCESS;
}


int32_t ctgGetTbMetaFromMnodeImpl(CTG_PARAMS, char *dbFName, char* tbName, STableMetaOutput* out, SCtgTask* pTask) {
  SBuildTableMetaInput bInput = {.vgId = 0, .dbFName = dbFName, .tbName = tbName};
  char *msg = NULL;
  SEpSet *pVnodeEpSet = NULL;
  int32_t msgLen = 0;
  int32_t reqType = TDMT_MND_TABLE_META;
  char tbFName[TSDB_TABLE_FNAME_LEN];
  sprintf(tbFName, "%s.%s", dbFName, tbName);

  ctgDebug("try to get table meta from mnode, tbFName:%s", tbFName);

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)](&bInput, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build mnode stablemeta msg failed, code:%x", code);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(STableMetaOutput));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, tbFName));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }

  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, (SEpSet*)pMgmtEps, &rpcMsg, &rpcRsp);
  
  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, tbFName));

  return TSDB_CODE_SUCCESS;
}

int32_t ctgGetTbMetaFromMnode(CTG_PARAMS, const SName* pTableName, STableMetaOutput* out, SCtgTask* pTask) {
  char dbFName[TSDB_DB_FNAME_LEN];
  tNameGetFullDbName(pTableName, dbFName);

  return ctgGetTbMetaFromMnodeImpl(CTG_PARAMS_LIST(), dbFName, (char *)pTableName->tname, out, async);
}

int32_t ctgGetTbMetaFromVnode(CTG_PARAMS, const SName* pTableName, SVgroupInfo *vgroupInfo, STableMetaOutput* out, SCtgTask* pTask) {
  char dbFName[TSDB_DB_FNAME_LEN];
  tNameGetFullDbName(pTableName, dbFName);
  int32_t reqType = TDMT_VND_TABLE_META;
  char tbFName[TSDB_TABLE_FNAME_LEN];
  sprintf(tbFName, "%s.%s", dbFName, pTableName->tname);

  ctgDebug("try to get table meta from vnode, vgId:%d, tbFName:%s", vgroupInfo->vgId, tbFName);

  SBuildTableMetaInput bInput = {.vgId = vgroupInfo->vgId, .dbFName = dbFName, .tbName = (char *)tNameGetTableName(pTableName)};
  char *msg = NULL;
  int32_t msgLen = 0;

  int32_t code = queryBuildMsg[TMSG_INDEX(reqType)](&bInput, &msg, 0, &msgLen);
  if (code) {
    ctgError("Build vnode tablemeta msg failed, code:%x, tbFName:%s", code, tbFName);
    CTG_ERR_RET(code);
  }

  if (pTask) {
    void* pOut = taosMemoryCalloc(1, sizeof(STableMetaOutput));
    if (NULL == pOut) {
      CTG_ERR_RET(TSDB_CODE_OUT_OF_MEMORY);
    }
    CTG_ERR_JRET(ctgUpdateMsgCtx(&pTask->msgCtx, reqType, pOut, tbFName));
    
    CTG_RET(ctgAsyncSendMsg(CTG_PARAMS_LIST(), pTask, reqType, msg, msgLen));
  }

  SRpcMsg rpcMsg = {
      .msgType = reqType,
      .pCont   = msg,
      .contLen = msgLen,
  };

  SRpcMsg rpcRsp = {0};
  rpcSendRecv(pTrans, &vgroupInfo->epSet, &rpcMsg, &rpcRsp);

  CTG_ERR_RET(ctgProcessRspMsg(out, reqType, rpcRsp.pCont, rpcRsp.contLen, rpcRsp.code, tbFName));  

  return TSDB_CODE_SUCCESS;
}

int32_t ctgHandleMsgCallback(void *param, const SDataBuf *pMsg, int32_t rspCode) {
  SCtgTaskCallbackParam* cbParam = (SCtgTaskCallbackParam*)param;
  int32_t code = 0;
  
  CTG_API_ENTER();

  SCtgJob* pJob = taosAcquireRef(gCtgMgmt.jobPool, cbParam->refId);
  if (NULL == pJob) {
    ctgDebug("job refId %" PRIx64 " already dropped", cbParam->refId);
    goto _return;
  }

  SCtgTask *pTask = taosArrayGet(pJob->pTasks, cbParam->taskId);

  CTG_ERR_JRET((*gCtgAsyncFps[pTask->type].rspFp)(pTask, cbParam->reqType, pMsg, rspCode));
 
_return:

  CTG_API_LEAVE(code);
}


int32_t ctgMakeMsgSendInfo(SCtgTask* pTask, int32_t msgType, SMsgSendInfo **pMsgSendInfo) {
  int32_t       code = 0;
  SMsgSendInfo *msgSendInfo = taosMemoryCalloc(1, sizeof(SMsgSendInfo));
  if (NULL == msgSendInfo) {
    ctgError("calloc %d failed", (int32_t)sizeof(SMsgSendInfo));
    CTG_ERR_RET(TSDB_CODE_QRY_OUT_OF_MEMORY);
  }

  SCtgTaskCallbackParam *param = taosMemoryCalloc(1, sizeof(SCtgTaskCallbackParam));
  if (NULL == param) {
    ctgError("calloc %d failed", (int32_t)sizeof(SSchTaskCallbackParam));
    CTG_ERR_JRET(TSDB_CODE_QRY_OUT_OF_MEMORY);
  }

  param->reqType = msgType;
  param->queryId = pTask->pJob->queryId;
  param->refId = pTask->pJob->refId;
  param->taskId = pTask->taskId;

  msgSendInfo->param = param;
  msgSendInfo->fp = ctgHandleMsgCallback;

  *pMsgSendInfo = msgSendInfo;

  return TSDB_CODE_SUCCESS;

_return:

  taosMemoryFree(param);
  taosMemoryFree(msgSendInfo);

  SCH_RET(code);
}

int32_t ctgAsyncSendMsg(CTG_PARAMS, SCtgTask* pTask, int32_t msgType, void *msg, uint32_t msgSize) {
  int32_t code = 0;
  SMsgSendInfo *pMsgSendInfo = NULL;
  SCH_ERR_JRET(ctgMakeMsgSendInfo(pTask, msgType, &pMsgSendInfo));

  pMsgSendInfo->msgInfo.pData = msg;
  pMsgSendInfo->msgInfo.len = msgSize;
  pMsgSendInfo->msgInfo.handle = NULL;
  pMsgSendInfo->msgType = msgType;

  int64_t transporterId = 0;
  code = asyncSendMsgToServer(pTrans, pMgmtEps, &transporterId, pMsgSendInfo);
  if (code) {
    ctgError("asyncSendMsgToSever failed, error: %s", tstrerror(code));
    CTG_ERR_JRET(code);
  }

  SCH_TASK_DLOG("req msg sent, reqId:%" PRIx64 ", msg type:%d, %s", pTask->pJob->queryId, msgType, TMSG_INFO(msgType));
  return TSDB_CODE_SUCCESS;

_return:

  if (pMsgSendInfo) {
    taosMemoryFreeClear(pMsgSendInfo->param);
    taosMemoryFreeClear(pMsgSendInfo);
  }

  SCH_RET(code);
}



