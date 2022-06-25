from distutils.log import error
import taos
import sys
import time
import socket
import os
import threading
import subprocess
import platform

from util.log import *
from util.sql import *
from util.cases import *
from util.dnodes import *
from util.common import *
sys.path.append("./7-tmq")
from tmqCommon import *

class TDTestCase:
    def init(self, conn, logSql):
        tdLog.debug(f"start to excute {__file__}")
        tdSql.init(conn.cursor())
        #tdSql.init(conn.cursor(), logSql)  # output sql.txt file
    
    def prepare_udf_so(self):
        selfPath = os.path.dirname(os.path.realpath(__file__))

        if ("community" in selfPath):
            projPath = selfPath[:selfPath.find("community")]
        else:
            projPath = selfPath[:selfPath.find("tests")]
        print(projPath)

        if platform.system().lower() == 'windows':
            self.libudf1 = subprocess.Popen('(for /r %s %%i in ("udf1.d*") do @echo %%i)|grep lib|head -n1'%projPath , shell=True, stdout=subprocess.PIPE,stderr=subprocess.STDOUT).stdout.read().decode("utf-8")
            if (not tdDnodes.dnodes[0].remoteIP == ""):
                tdDnodes.dnodes[0].remote_conn.get(tdDnodes.dnodes[0].config["path"]+'/debug/build/lib/libudf1.so',projPath+"\\debug\\build\\lib\\")
                self.libudf1 = self.libudf1.replace('udf1.dll','libudf1.so')
        else:
            self.libudf1 = subprocess.Popen('find %s -name "libudf1.so"|grep lib|head -n1'%projPath , shell=True, stdout=subprocess.PIPE,stderr=subprocess.STDOUT).stdout.read().decode("utf-8")
        self.libudf1 = self.libudf1.replace('\r','').replace('\n','')
        return

    def create_udf_function(self):
        # create  scalar functions
        tdSql.execute("create function udf1 as '%s' outputtype int bufSize 8;"%self.libudf1)

        functions = tdSql.getResult("show functions")
        function_nums = len(functions)
        if function_nums == 1:
            tdLog.info("create one udf functions success ")
        else:
            tdLog.exit("create udf functions fail")
        return

    def checkFileContent(self, consumerId, queryString):
        buildPath = tdCom.getBuildPath()
        cfgPath = tdCom.getClientCfgPath()
        dstFile = '%s/../log/dstrows_%d.txt'%(cfgPath, consumerId)
        cmdStr = '%s/build/bin/taos -c %s -s "%s >> %s"'%(buildPath, cfgPath, queryString, dstFile)
        tdLog.info(cmdStr)
        os.system(cmdStr)
        
        consumeRowsFile = '%s/../log/consumerid_%d.txt'%(cfgPath, consumerId)
        tdLog.info("rows file: %s, %s"%(consumeRowsFile, dstFile))

        consumeFile = open(consumeRowsFile, mode='r')
        queryFile = open(dstFile, mode='r')
        
        # skip first line for it is schema
        queryFile.readline()

        while True:
            dst = queryFile.readline()
            src = consumeFile.readline()
            
            if dst:
                if dst != src:
                    tdLog.exit("consumerId %d consume rows is not match the rows by direct query"%consumerId)
            else:
                break
        return 

    def tmqCase1(self):
        tdLog.printNoPrefix("======== test case 1: ")
        paraDict = {'dbName':     'db1',
                    'dropFlag':   1,
                    'event':      '',
                    'vgroups':    4,
                    'stbName':    'stb',
                    'colPrefix':  'c',
                    'tagPrefix':  't',
                    'colSchema':   [{'type': 'INT', 'count':2}, {'type': 'binary', 'len':20, 'count':1}],
                    'tagSchema':   [{'type': 'INT', 'count':1}, {'type': 'binary', 'len':20, 'count':1}],
                    'ctbPrefix':  'ctb',
                    'ctbNum':     1,
                    'rowsPerTbl': 1000,
                    'batchNum':   10,
                    'startTs':    1640966400000,  # 2022-01-01 00:00:00.000
                    'pollDelay':  10,
                    'showMsg':    1,
                    'showRow':    1}

        topicNameList = ['topic1', 'topic2']
        expectRowsList = []
        tmqCom.initConsumerTable()
        tdCom.create_database(tdSql, paraDict["dbName"],paraDict["dropFlag"], vgroups=4,replica=1)
        tdLog.info("create stb")
        tdCom.create_stable(tdSql, dbname=paraDict["dbName"],stbname=paraDict["stbName"], column_elm_list=paraDict['colSchema'], tag_elm_list=paraDict['tagSchema'])
        tdLog.info("create ctb")
        tdCom.create_ctable(tdSql, dbname=paraDict["dbName"],stbname=paraDict["stbName"],tag_elm_list=paraDict['tagSchema'],count=paraDict["ctbNum"], default_ctbname_prefix=paraDict['ctbPrefix'])
        tdLog.info("insert data")
        tmqCom.insert_data_1(tdSql,paraDict["dbName"],paraDict["ctbPrefix"],paraDict["ctbNum"],paraDict["rowsPerTbl"],paraDict["batchNum"],paraDict["startTs"])
        
        tdLog.info("create topics from stb with filter")
        queryString = "select ts,c1,udf1(c1),c2,udf1(c2) from %s.%s where c1 %% 7 == 0" %(paraDict['dbName'], paraDict['stbName'])
        sqlString = "create topic %s as %s" %(topicNameList[0], queryString)
        tdLog.info("create topic sql: %s"%sqlString)
        tdSql.execute(sqlString)
        tdSql.query(queryString)        
        expectRowsList.append(tdSql.getRows())

        # init consume info, and start tmq_sim, then check consume result
        tdLog.info("insert consume info to consume processor")
        consumerId   = 0
        expectrowcnt = paraDict["rowsPerTbl"] * paraDict["ctbNum"]
        topicList    = topicNameList[0]
        ifcheckdata  = 1
        ifManualCommit = 1
        keyList      = 'group.id:cgrp1, enable.auto.commit:false, auto.commit.interval.ms:6000, auto.offset.reset:earliest'
        tmqCom.insertConsumerInfo(consumerId, expectrowcnt,topicList,keyList,ifcheckdata,ifManualCommit)

        tdLog.info("start consume processor")
        tmqCom.startTmqSimProcess(paraDict['pollDelay'],paraDict["dbName"],paraDict['showMsg'], paraDict['showRow'])

        tdLog.info("wait the consume result") 
        expectRows = 1
        resultList = tmqCom.selectConsumeResult(expectRows)
        
        if expectRowsList[0] != resultList[0]:
            tdLog.info("expect consume rows: %d, act consume rows: %d"%(expectRowsList[0], resultList[0]))
            tdLog.exit("0 tmq consume rows error!")

        self.checkFileContent(consumerId, queryString)
        tdLog.printNoPrefix("consumerId %d check data ok!"%(consumerId))


        # reinit consume info, and start tmq_sim, then check consume result
        tmqCom.initConsumerTable()

        queryString = "select ts, c1,udf1(c1),sin(udf1(c2)), log(udf1(c2)) from %s.%s where udf1(c1) == 88 or sin(udf1(c1)) > 0" %(paraDict['dbName'], paraDict['stbName'])
        sqlString = "create topic %s as %s" %(topicNameList[1], queryString)
        tdLog.info("create topic sql: %s"%sqlString)
        tdSql.execute(sqlString)
        tdSql.query(queryString)        
        expectRowsList.append(tdSql.getRows())

        consumerId   = 1
        topicList    = topicNameList[1]
        tmqCom.insertConsumerInfo(consumerId, expectrowcnt,topicList,keyList,ifcheckdata,ifManualCommit)

        tdLog.info("start consume processor")
        tmqCom.startTmqSimProcess(paraDict['pollDelay'],paraDict["dbName"],paraDict['showMsg'], paraDict['showRow'])

        tdLog.info("wait the consume result") 
        expectRows = 1
        resultList = tmqCom.selectConsumeResult(expectRows)
        if expectRowsList[1] != resultList[0]:
            tdLog.info("expect consume rows: %d, act consume rows: %d"%(expectRowsList[1], resultList[0]))
            tdLog.exit("1 tmq consume rows error!")

        self.checkFileContent(consumerId, queryString)
        tdLog.printNoPrefix("consumerId %d check data ok!"%(consumerId))

        time.sleep(10)        
        for i in range(len(topicNameList)):
            tdSql.query("drop topic %s"%topicNameList[i])

        tdLog.printNoPrefix("======== test case 1 end ...... ")

    def run(self):
        tdSql.prepare()
        self.prepare_udf_so()
        self.create_udf_function()
        self.tmqCase1()

    def stop(self):
        tdSql.close()
        tdLog.success(f"{__file__} successfully executed")

event = threading.Event()

tdCases.addLinux(__file__, TDTestCase())
tdCases.addWindows(__file__, TDTestCase())