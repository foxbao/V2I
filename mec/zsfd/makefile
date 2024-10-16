MK		= make

.PHONY: all

all :
	$(MK) -f .vscode/utils_mkdep
	$(MK) -f .vscode/utils_mk
	$(MK) -f .vscode/mware_mkdep 
	$(MK) -f .vscode/mware_mk 
	$(MK) -f .vscode/webcore_mkdep 
	$(MK) -f .vscode/webcore_mk 
	$(MK) -f .vscode/host_mkdep
	$(MK) -f .vscode/host_mk
	$(MK) -f .vscode/launch_mkdep
	$(MK) -f .vscode/launch_mk
	$(MK) -f .vscode/sysd_mkdep
	$(MK) -f .vscode/sysd_mk
	$(MK) -f .vscode/rpcservice_mkdep
	$(MK) -f .vscode/rpcservice_mk
	$(MK) -f .vscode/rpctest_mkdep
	$(MK) -f .vscode/rpctest_mk
	$(MK) -f .vscode/rpcbridge_mkdep
	$(MK) -f .vscode/rpcbridge_mk
	$(MK) -f .vscode/coreapp_servicebundle_mkdep
	$(MK) -f .vscode/coreapp_servicebundle_mk
	$(MK) -f .vscode/remote_client_mkdep
	$(MK) -f .vscode/remote_client_mk

.PHONY: x86_dbg

x86_dbg :
	$(MK) -f .vscode/utils_mkdep DEBUG=1
	$(MK) -f .vscode/utils_mk  DEBUG=1 -j8
	$(MK) -f .vscode/mware_mkdep DEBUG=1
	$(MK) -f .vscode/mware_mk  DEBUG=1 -j8
	$(MK) -f .vscode/host_mkdep DEBUG=1
	$(MK) -f .vscode/host_mk  DEBUG=1 -j8
	$(MK) -f .vscode/launch_mkdep DEBUG=1
	$(MK) -f .vscode/launch_mk  DEBUG=1 -j8
	$(MK) -f .vscode/sysd_mkdep DEBUG=1
	$(MK) -f .vscode/sysd_mk  DEBUG=1 -j8
	$(MK) -f .vscode/rpcservice_mkdep DEBUG=1
	$(MK) -f .vscode/rpcservice_mk  DEBUG=1 -j8
	$(MK) -f .vscode/rpctest_mkdep DEBUG=1
	$(MK) -f .vscode/rpctest_mk  DEBUG=1 -j8
	$(MK) -f .vscode/rpcbridge_mkdep  DEBUG=1 -j8
	$(MK) -f .vscode/rpcbridge_mk  DEBUG=1 -j8
	$(MK) -f .vscode/coreapp_servicebundle_mkdep  DEBUG=1 -j8
	$(MK) -f .vscode/coreapp_servicebundle_mk  DEBUG=1 -j8
	$(MK) -f .vscode/remote_client_mkdep  DEBUG=1 -j8
	$(MK) -f .vscode/remote_client_mk  DEBUG=1 -j8
	$(MK) -f .vscode/webcore_mkdep  DEBUG=1 -j8
	$(MK) -f .vscode/webcore_mk  DEBUG=1 -j8

.PHONY: nvidia

nvidia :
	$(MK) -f .vscode/utils_mkdep
	$(MK) -f .vscode/utils_mk
	$(MK) -f .vscode/mware_mkdep 
	$(MK) -f .vscode/mware_mk 
	$(MK) -f .vscode/host_mkdep
	$(MK) -f .vscode/host_mk
	$(MK) -f .vscode/launch_mkdep
	$(MK) -f .vscode/launch_mk
	$(MK) -f .vscode/sysd_mkdep
	$(MK) -f .vscode/sysd_mk
	$(MK) -f .vscode/coreapp_servicebundle_mkdep
	$(MK) -f .vscode/coreapp_servicebundle_mk
	$(MK) -f .vscode/webcore_mkdep
	$(MK) -f .vscode/webcore_mk

.PHONY: rpcc

rpcc :
	$(MK) -f .vscode/rpcc_mkdep
	$(MK) -f .vscode/rpcc_mk

 .PHONY : syssvr

syssvr:
	$(MK) -f .vscode/syssvr_mkdep
	$(MK) -f .vscode/syssvr_mk

 .PHONY : launch

launch:
	$(MK) -f .vscode/launch_mkdep
	$(MK) -f .vscode/launch_mk

 .PHONY : arm

arm:
	$(MK) -f .vscode/utils_mkdep ARM=1
	$(MK) -f .vscode/utils_mk ARM=1
	$(MK) -f .vscode/mware_mkdep  ARM=1
	$(MK) -f .vscode/mware_mk  ARM=1
	$(MK) -f .vscode/host_mkdep ARM=1
	$(MK) -f .vscode/host_mk ARM=1
	$(MK) -f .vscode/launch_mkdep ARM=1
	$(MK) -f .vscode/launch_mk ARM=1
	$(MK) -f .vscode/sysd_mkdep ARM=1
	$(MK) -f .vscode/sysd_mk ARM=1
	$(MK) -f .vscode/rpcbridge_mkdep ARM=1
	$(MK) -f .vscode/rpcbridge_mk ARM=1
	$(MK) -f .vscode/coreapp_servicebundle_mkdep ARM=1
	$(MK) -f .vscode/coreapp_servicebundle_mk ARM=1
	$(MK) -f .vscode/webcore_mkdep ARM=1
	$(MK) -f .vscode/webcore_mk ARM=1


 .PHONY : protofile

protofile:
#	@-rm -r ./test/mware/test.pro.pb.h
#	@-rm -r ./test/mware/test.pro.pb.cpp
#	@protoc -I=./test/mware --cpp_out=./test/mware test.pro
#	@mv ./test/mware/test.pro.pb.cc ./test/mware/test.pro.pb.cpp	
#	@-rm -r ./mware/rpc/proto/bsctype_array.pb.h
#	@-rm -r ./mware/rpc/proto/bsctype_array.pb.cpp
#	@protoc -I=./mware/rpc/proto --cpp_out=./mware/rpc/proto bsctype_array.proto
#	@mv ./mware/rpc/proto/bsctype_array.pb.cc ./mware/rpc/proto/bsctype_array.pb.cpp
#	@-rm -r ./test/remote_client/proto/traffic_signals.pro.pb.h
#	@-rm -r ./test/remote_client/proto/traffic_signals.pro.pb.cpp
#	@protoc -I=./test/remote_client/proto --cpp_out=./test/remote_client/proto traffic_signals.pro
#	@mv ./test/remote_client/proto/traffic_signals.pro.pb.cc ./test/remote_client/proto/traffic_signals.pro.pb.cpp
#	@echo "make proto finished"
#
#	@-rm -r ./test/inc/rpc/mytest-internal.h
#	@-rm -r ./test/inc/rpc/mytest.h
#	@-rm -r ./test/inc/rpc/test.pro.pb.h
#	@ln ./test/mware/mytest-internal.h ./test/inc/rpc/mytest-internal.h
#	@ln ./test/mware/mytest.h ./test/inc/rpc/mytest.h
#	@ln ./test/mware/test.pro.pb.h ./test/inc/rpc/test.pro.pb.h
#	@-rm -r ./test/client/mytest-proxy.cpp
#	@-rm -r ./test/client/mytest-structs.cpp
#	@-rm -r ./test/client/test.pro.pb.cpp
#	@ln ./test/mware/mytest-proxy.cpp ./test/client/mytest-proxy.cpp
#	@ln ./test/mware/mytest-structs.cpp ./test/client/mytest-structs.cpp
#	@ln ./test/mware/test.pro.pb.cpp ./test/client/test.pro.pb.cpp
#	@-rm -r ./test/rpcservice/mytest-skeleton.cpp
#	@-rm -r ./test/rpcservice/mytest-structs.cpp
#	@-rm -r ./test/rpcservice/mytest.cpp
#	@-rm -r ./test/rpcservice/test.pro.pb.cpp
#	@ln ./test/mware/mytest-skeleton.cpp ./test/rpcservice/mytest-skeleton.cpp
#	@ln ./test/mware/mytest-structs.cpp ./test/rpcservice/mytest-structs.cpp
#	@ln ./test/mware/mytest.cpp ./test/rpcservice/mytest.cpp
#	@ln ./test/mware/test.pro.pb.cpp ./test/rpcservice/test.pro.pb.cpp
#	@echo "teset project finished"
	@protoc -I=./mware/rpc/proto --cpp_out=./mware/rpc/proto bsctype_array.proto
	@mv ./mware/rpc/proto/bsctype_array.pb.cc ./mware/rpc/proto/bsctype_array.pb.cpp

 .PHONY : install_arm

install_arm:
	@sudo cp -r ./targets/arm64-rel/libmware.so /zassys/lib/arm_64-linux-gnu/
	@sudo cp -r ./targets/arm64-rel/libutils.so /zassys/lib/arm_64-linux-gnu/
	@sudo cp -r ./targets/arm64-rel/libwebcore.so /zassys/lib/arm_64-linux-gnu/
	@sudo cp -r ./targets/arm64-rel/sys/zhost /zassys/bin/
	@sudo cp -r ./targets/arm64-rel/sys/zlaunch /zassys/bin/
	@sudo cp -r ./targets/arm64-rel/sys/coreapp/zsysd /zassys/sysapp/zas.system/daemons/
	@sudo cp -r ./targets/arm64-rel/sys/coreapp/rpcbridge /zassys/sysapp/zas.system/daemons/
	@sudo cp -r ./targets/arm64-rel/sys/coreapp/services/servicebundle.so /zassys/sysapp/zas.system/services/
	
clean:
	rm -rf ./targets/*
