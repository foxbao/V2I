MK		= make

 .PHONY : indexing

 indexing:
	$(MK) -f .vscode/mapcore_mkdep
	$(MK) -f .vscode/mapcore_mk
	$(MK) -f .vscode/vehicle_indexing_mkdep
	$(MK) -f .vscode/vehicle_indexing_mk
	$(MK) -f .vscode/vehicle_snapshot_service_mkdep
	$(MK) -f .vscode/vehicle_snapshot_service_mk

 .PHONY : protofile

protofile:
	@-rm -rf ./vehicle-indexing/protomsg
	@mkdir -p ./vehicle-indexing/protomsg

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg snapshot_package.pro
	@mv ./vehicle-indexing/protomsg/snapshot_package.pro.pb.cc ./vehicle-indexing/protomsg/snapshot_package.pro.pb.cpp	

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg vehicle_snapshot.pro
	@mv ./vehicle-indexing/protomsg/vehicle_snapshot.pro.pb.cc ./vehicle-indexing/protomsg/vehicle_snapshot.pro.pb.cpp	

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg snapshot_request.pro
	@mv ./vehicle-indexing/protomsg/snapshot_request.pro.pb.cc ./vehicle-indexing/protomsg/snapshot_request.pro.pb.cpp	

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg register_reply.pro
	@mv ./vehicle-indexing/protomsg/register_reply.pro.pb.cc ./vehicle-indexing/protomsg/register_reply.pro.pb.cpp	

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg register_request.pro
	@mv ./vehicle-indexing/protomsg/register_request.pro.pb.cc ./vehicle-indexing/protomsg/register_request.pro.pb.cpp

	@protoc -I=./vehicle-indexing/proto --cpp_out=./vehicle-indexing/protomsg remove_reply.pro
	@mv ./vehicle-indexing/protomsg/remove_reply.pro.pb.cc ./vehicle-indexing/protomsg/remove_reply.pro.pb.cpp	

	@protoc -I=./vehicle-snapshot-service/proto --cpp_out=./vehicle-snapshot-service/protomsg snapshot_package.pro
	@mv ./vehicle-snapshot-service/protomsg/snapshot_package.pro.pb.cc ./vehicle-snapshot-service/protomsg/snapshot_package.pro.pb.cpp	

	@protoc -I=./vehicle-snapshot-service/proto --cpp_out=./vehicle-snapshot-service/protomsg vehicle_snapshot.pro
	@mv ./vehicle-snapshot-service/protomsg/vehicle_snapshot.pro.pb.cc ./vehicle-snapshot-service/protomsg/vehicle_snapshot.pro.pb.cpp	

	@protoc -I=./vehicle-snapshot-service/proto --cpp_out=./vehicle-snapshot-service/protomsg junction_package.pro
	@mv ./vehicle-snapshot-service/protomsg/junction_package.pro.pb.cc ./vehicle-snapshot-service/protomsg/junction_package.pro.pb.cpp	

	@protoc -I=./vehicle-snapshot-service/proto --cpp_out=./vehicle-snapshot-service/protomsg fusion_package.pro
	@mv ./vehicle-snapshot-service/protomsg/fusion_package.pro.pb.cc ./vehicle-snapshot-service/protomsg/fusion_package.pro.pb.cpp	

	@echo "protobuf project finished"