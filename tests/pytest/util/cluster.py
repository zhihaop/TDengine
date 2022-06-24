from ssl import ALERT_DESCRIPTION_CERTIFICATE_UNOBTAINABLE
import taos
import sys
import time
import os 
import  socket

from util.log import *
from util.sql import *
from util.cases import *
from util.dnodes import *
from util.common import *

class ClusterDnodes(TDDnodes):
    """rewrite TDDnodes and make MyDdnodes as TDDnodes child class"""
    def __init__(self ,dnodes_lists):

        super(ClusterDnodes,self).__init__()
        self.dnodes = dnodes_lists  # dnode must be TDDnode instance
        self.simDeployed = False
        self.testCluster = False
        self.valgrind = 0
        self.killValgrind = 1


class ConfigureyCluster:
    """This will create defined number of dnodes and create a cluset.
       at the same time, it will return TDDnodes list:  dnodes, """
    hostname= socket.gethostname()

    def __init__(self):
        self.dnodes = []      
        self.dnodeNums = 5
        self.independent = True
        self.startPort = 6030
        self.portStep = 100
        self.mnodeNums = 0

    def configure_cluster(self ,dnodeNums=5,mnodeNums=0,startPort=6030,portStep=100,hostname="%s"%hostname): 
        self.startPort=int(startPort)
        self.portStep=int(portStep)
        self.hostname=hostname
        self.dnodeNums = int(dnodeNums)
        self.mnodeNums = int(mnodeNums)
        self.dnodes = []
        startPort_sec = int(startPort+portStep)
        for num in range(1, (self.dnodeNums+1)):
            dnode = TDDnode(num)
            dnode.addExtraCfg("firstEp", f"{hostname}:{self.startPort}")
            dnode.addExtraCfg("fqdn", f"{hostname}")
            dnode.addExtraCfg("serverPort", f"{self.startPort + (num-1)*self.portStep}")
            dnode.addExtraCfg("secondEp", f"{hostname}:{startPort_sec}")

            # configure  dnoe of independent mnodes
            if num <= self.mnodeNums and self.mnodeNums != 0 :
                dnode.addExtraCfg("supportVnodes", 0)
            # print(dnode)
            self.dnodes.append(dnode)
        return self.dnodes
        
    def create_dnode(self,conn):
        tdSql.init(conn.cursor())
        for dnode in self.dnodes[1:]:
            # print(dnode.cfgDict)
            dnode_id = dnode.cfgDict["fqdn"] +  ":" +dnode.cfgDict["serverPort"]
            tdSql.execute(" create dnode '%s';"%dnode_id)

        
        
    def check_dnode(self,conn):
        tdSql.init(conn.cursor())
        count=0
        while count < 5:
            tdSql.query("show dnodes")
            # tdLog.debug(tdSql.queryResult)
            status=0
            for i in range(self.dnodeNums):
                if tdSql.queryResult[i][4] == "ready":
                    status+=1
            # tdLog.debug(status)
            
            if status == self.dnodeNums:
                tdLog.debug(" create cluster with %d dnode and check cluster dnode all ready within 5s! " %self.dnodeNums)
                break 
            count+=1
            time.sleep(1)
        else:
            tdLog.debug("create cluster with %d dnode but  check dnode not ready within 5s ! "%self.dnodeNums)
            return -1


cluster = ConfigureyCluster()