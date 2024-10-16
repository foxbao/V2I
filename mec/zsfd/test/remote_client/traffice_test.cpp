#include "traffic_signals.h"
#include "utils/evloop.h"
#include "utils/timer.h"
#include "utils/thread.h"
#include "std/zasbsc.h"

using namespace traffic_signals::proxy;

//这个是内部namespace，业务自己根据需求考虑是否需要
using namespace zas::utils;

//仅作为输出参数内容的函数
void output_intersection_info(::traffic_signals::intersection_info& info)
{
	printf("intersection approcach id:%s\n", std::string(info.approach_id).c_str());
	printf("intersection timestamp:%ld\n", (signed long int)(info.timestamp));
	printf("intersection in section:%d\n", bool(info.in_intersection));
	printf("intersection status:%d\n", int32_t(info.status));
	printf("intersection movent count:%d\n", int32_t(info.movement_count));
	for (int i = 0; i < info.movement_count; i++) {
		printf("movement %d, type is %d\n", i, traffic_signals::movement_type(info.items.get(i).type));	
		printf("movement %d, status is %d\n", i, traffic_signals::traffic_signal_status(info.items.get(i).status));	
		printf("movement %d, time is %d\n", i, int32_t(info.items.get(i).left_time));	
	}
}

//信号灯监听类
class tfc_sg_lnr :public traffic_signal_listener
{
	void on_traffic_light_changed(::traffic_signals::intersection_info& int_info) {
		printf("on_traffic_light_changed recv\n");
		output_intersection_info(int_info);
	}
};


//业务运行的独立线程。
//业务自己进行实现，这里仅做示例
class tfc_sg_run_thread : public thread
{
public:
	tfc_sg_run_thread(){}

	~tfc_sg_run_thread() {
	}

	int run(void) {
		printf("test start service begin\n");
		getchar();
		getchar();
		printf("test start service start\n");

		try {

			traffic_signals::vehicle_info vdinfo;
			std::string car = "test vehicle car";
			vdinfo.veh_id = car;
			vdinfo.veh_speed = 42.5;
			vdinfo.timestamp = 12345;
			vdinfo.gps.direction = 12.2;
			vdinfo.gps.latitude = 13.3;
			vdinfo.gps.longitude = 14.4;
			//车辆服务和信号灯服务，可以分开执行
			auto testcar = traffic_signal_mgr::inst().register_vehicle(vdinfo);

			auto sgn_info = traffic_signal_mgr::inst().get_traffic_signal(vdinfo);

			::traffic_signals::intersection_info ectinfo;
			ectinfo = sgn_info.get_current();
			output_intersection_info(ectinfo);
			tfc_sg_lnr lnr;
			sgn_info.register_traffic_signal_listener(lnr);
			long total_time = 0;
			long tmptick = gettick_millisecond();
			for (int i = 0; i < 10000; i++) {
				// msleep(100);
				vdinfo.timestamp = int64_t(vdinfo.timestamp) + 1000;
				tmptick = gettick_millisecond();
				testcar.update_vehicle_info(vdinfo);
				total_time += gettick_millisecond() - tmptick;
				if (i % 100 == 0)
				{
					printf("*******100 times time is %ld\n", total_time);
					total_time = 0;
				}
			}

		} catch(rpc_error& err)
		{
			printf("error test is %s\n", err.get_description().c_str());
		};
		getchar();
		getchar();
		getchar();
	}
	
private:
	ZAS_DISABLE_EVIL_CONSTRUCTOR(tfc_sg_run_thread);
};


//本地client与zlaunch中的通信server建立连接的回调。
//该类可以重新实现，但必须继承于evloop_listener
class tfc_sg_test_client_listener: public evloop_listener
{
public:
	tfc_sg_test_client_listener() {
	}
	void connected(evlclient client) {
		//收到连接zlaunch docker的server成功回调
		//启动单独的线程运行业务。（这里不允许做耗时处理）
		//开启独立的线程
		printf("connect to server\n");
		auto *runthread = new tfc_sg_run_thread();
		runthread->start();
		runthread->release();
	}

};


//main()为本次测试的入口函数。
int main()
{
	//客户端启动通信
	//该部分不需要被调整
	evloop *evl = evloop::inst();
	evl->setrole(evloop_role_client);

	//客户端内部通讯使用名称。
	//业务可以启用多个客户端，在不同程序中获取信号灯信息，或者上传GPS
	//那么不同的应用的客户端， evlcli_info_instance_name
	//若启动多个应用客户端，请修改evlcli_info_instance_name对应的"sysds"，保证其不同
	evl->updateinfo(evlcli_info_client_name, "zas.system.test")
		->updateinfo(evlcli_info_instance_name, "sysds")
		->updateinfo(evlcli_info_commit);

	//注册本地通信监听回调
	//当与zlaunch docker中的通信server建立连接后，将会进行回调connected
	//该部分不需要被调整
	tfc_sg_test_client_listener lnr;
	evl->add_listener("testlistener", &lnr);

	//start开始启动与zlaunch docker通信
	//要求调用start之前，zlaunch docker已经开始运行
	//该部分不需要被调整
	evl->start(false);

}
