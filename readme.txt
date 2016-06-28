
1. build P2P device agent v3.0 PC emulator

    Do not use TortoiseSVN to check out, the linux link file may corrupt
    
    svn co http://svn/svn/Project-P2PV2
    
    make PLATFORM=X86 SUPPORT_SSL_REG=yes REVISION=`svn info |grep '^Last Changed Rev:' | sed -e 's/^Last Changed Rev: //'`
    
1.1 build P2P device agent for camera device    
    
    // for GM
    export LD_LIBRARY_PATH=/opt/arm-GM8139/usr/lib
    
    // for brickcom
    export LD_LIBRARY_PATH=/opt/crosstool/toolchain_gnueabi-4.4.0_ARMv5TE/usr/lib
    
    make PLATFORM=B813X SUPPORT_SSL_REG=yes BRICKCOM=yes REVISION=`svn info |grep '^Last Changed Rev:' | sed -e 's/^Last Changed Rev: //'`
    
    // not secure 
    make PLATFORM=B813X BRICKCOM=yes REVISION=`svn info |grep '^Last Changed Rev:' | sed -e 's/^Last Changed Rev: //'`
    
2. ssl library 查詢

apt-file list libssl-dev

  2.1 libssl-dev 補包安裝
  sudo apt-get install libssl-dev

update the include path and library path in makefile

3. install curl development (PC client)

sudo apt-get update

apt-get install libcurl4-gnutls-dev



4. use PC agent 模擬Device

 ./triclouds_device_agent <device UID> <stun server URL> 
 
 It need program local camera address
 cfg -a<local camera address>
 cfg -m<device agent's ip address>
 
 ex:
./triclouds_device_agent SICAM194A1RAISSA123 dev-auth.triclouds-iot.com 
./triclouds_device_agent SICAM194A1RAISSA123 130.211.133.152

// for auth test new session key
./triclouds_device_agent SICAM194A1RAISSA123 104.197.23.244
./triclouds_device_agent SICAM104E676A2B2F9 104.197.23.244
#
# for gsb debug
#
gdb ./triclouds_device_agent
set args SICAM194A1RAISSA123 dev-auth.triclouds-iot.com

4.client_app_3.0

P2P device client_app_3.0

1.  build P2P client
    cd client_app_3.0
    make clean
    make

2. operation
2.1 login 

    P2PC>login <username> <password>
    
    for test: sicam1TestAccount, sicam1TestAccount
    
    login sicam1TestAccount sicam1TestAccount
    
2.2 get device list in this account

    P2PC>getlist    
    
2.3 show devices

    P2PC>show
    
2.4 connected to specified camera

    P2PC>open <UID>


login sicam1TestAccount sicam1TestAccount
getlist
open SICAM104E676A2B2F9
open SICAM194A1RAISSA123