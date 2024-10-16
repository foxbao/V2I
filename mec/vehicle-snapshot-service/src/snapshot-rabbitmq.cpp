#include "snapshot-rabbitmq.h"
#include "webcore/logger.h"
#include "webcore/sysconfig.h"
#include "snapshot-service-def.h"
#include "service-worker.h"

#include "proto/snapshot_package.pb.h"

namespace zas {
namespace vehicle_snapshot_service {

#define RABBITMQ_INIT_RETRY_INTERVAL (1000)

using namespace zas::utils;
using namespace zas::webcore;
using namespace vss;

class rabbitmq_init_retry_timer : public timer
{
public:
	rabbitmq_init_retry_timer(timermgr* mgr, uint32_t interval,
		snapshot_rabbitmq* vss_rmq)
	: timer(mgr, interval)
	, _vss_rmq(vss_rmq) {
	}

	void on_timer(void) {
		assert(nullptr != _vss_rmq);
		_vss_rmq->on_timer();
	}

private:
	snapshot_rabbitmq* _vss_rmq;
	ZAS_DISABLE_EVIL_CONSTRUCTOR(rabbitmq_init_retry_timer);
};

snapshot_rabbitmq::snapshot_rabbitmq(rabbitmq_role erole,
	const char* name, service_backend* backend, void* context, timermgr* mgr)
: _rabbitmq_role(erole)
, _backend(backend)
, _cb(nullptr)
, _conn(nullptr)
, _retry_timer(nullptr)
, _context(context)
, _channel_id(1)
, _sock_fd(0)
, _port(0)
{
	_host.clear();
	_username.clear();
	_password.clear();
	assert(nullptr != _backend);
	assert(nullptr != _context);
	if (!name || !*name) {
		_mq_name = "defalut";
	} else {
		_mq_name = name;
	}
	_retry_timer = new rabbitmq_init_retry_timer(mgr,
		RABBITMQ_INIT_RETRY_INTERVAL, this);
}

snapshot_rabbitmq::~snapshot_rabbitmq()
{
	if (_retry_timer) {
		delete _retry_timer;
		_retry_timer = nullptr;
	}
	if (nullptr == _conn) {
		return;
	}
	if (_sock_fd && _backend) {
		_backend->removefd(_context, _sock_fd);
		_sock_fd = 0;
		_backend = nullptr;
		_context = nullptr;
	}
	log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"release rabbitmq %d\n", _rabbitmq_role);
	amqp_rpc_reply_t ret = amqp_channel_close(_conn,
			_channel_id, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close channel");

	ret = amqp_connection_close(_conn, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close connection");

	int retval = amqp_destroy_connection(_conn);
	if (AMQP_STATUS_OK != retval) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"destroy connection error [%d]", retval);
	}
}

int snapshot_rabbitmq::init(void)
{
	if (load_config()) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"rabbitmq load config error");
		return -ENOTAVAIL;
	}
	int ret = 0;
	if (rabbitmq_role_producing == _rabbitmq_role) {
		log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"init rabbitmq producing");
		ret = init_producing();
	} else {
		log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"init rabbitmq consuming");
		ret = init_consuming();
	}
	if (ret) {
		if (_retry_timer) {
			_retry_timer->start();
		}
	}
	return ret;
}

void snapshot_rabbitmq::on_timer(void)
{
	int ret = 0;
	log.d(SNAPSHOT_SNAPSHOT_TAG, "retry init ontimer");
	if (rabbitmq_role_producing == _rabbitmq_role) {
		log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"retry init rabbitmq producing");
		ret = retry_init_producing();
	} else {
		log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"retry init rabbitmq consuming");
		ret = retry_init_consuming();
	}
	if (ret) {
		if (_retry_timer) {
			_retry_timer->start();
		}
	}
}

int snapshot_rabbitmq::publishing(const char* exchange, std::string &routingkey,
	void* data, size_t sz)
{
	amqp_basic_properties_t props;
	amqp_bytes_t body;
	body.bytes = data;
	body.len = sz;
	props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
	props.content_type = amqp_cstring_bytes("application/octet-stream");
	props.delivery_mode = 2; /* persistent delivery mode */
	std::string rkey = routingkey;
	// rkey += "." + routingkey;
	// log.w(SNAPSHOT_SNAPSHOT_TAG, "routekey %s\n", rkey.c_str());
	if (nullptr == _conn) {
		retry_init_producing();
	}
	int ret = amqp_basic_publish(_conn, _channel_id, 
				amqp_cstring_bytes(exchange),
				amqp_cstring_bytes(rkey.c_str()),
				0,
				0,
				&props, 
				body);
	if (ret != AMQP_STATUS_OK){	
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting amqp error %d", ret);
		if (AMQP_STATUS_SOCKET_CLOSED == ret
			|| AMQP_STATUS_SOCKET_ERROR == ret) {
			retry_init_producing();
		}
		return ret;
	}	
	return 0;
}

int snapshot_rabbitmq::set_consuming_callback(ss_rabbitmq_callback* cb)
{
	_cb = cb;
	return 0;
}

int snapshot_rabbitmq::load_config(void)
{
	_exchange_info.clear();
	std::string path = "rabbitmq";
	if (rabbitmq_role_producing == _rabbitmq_role) {
		path += ".producing.";
	} else {
		path += ".consuming.";
	}
	path += _mq_name;
	path += ".";

	std::string mqattr = path + "host";
	int iret = 0;
	_host = get_sysconfig(mqattr.c_str(), _host.c_str(), &iret);
	if (iret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load hostconfig");
		return -ENOTAVAIL;
	}

	mqattr = path + "port";
	_port = get_sysconfig(mqattr.c_str(), _port, &iret);
	if (iret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load PORT");
		return -ENOTAVAIL;
	} else {
		log.d(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq load PORT %d", _port);
	}

	mqattr = path + "channel";
	_channel_id = get_sysconfig(mqattr.c_str(), _channel_id, &iret);
	if (iret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load channel");
		return -ENOTAVAIL;
	}

	mqattr = path + "username";
	_username = get_sysconfig(mqattr.c_str(),
			_username.c_str(), &iret);
	if (iret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load username");
		return -ENOTAVAIL;
	}

	mqattr = path + "password";
	_password = get_sysconfig(mqattr.c_str(),
			_password.c_str(), &iret);
	if (iret) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load username");
		return -ENOTAVAIL;
	}

	int ex_cnt = 0;
	mqattr = path + "exchange-count";
	ex_cnt = get_sysconfig(mqattr.c_str(), ex_cnt, &iret);
	if (iret || ex_cnt <= 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"rabbitmq not load exchange count");
		return -ENOTAVAIL;
	}
	for (int i = 0; i < ex_cnt; i++) {
		mqattr = path + "exchange-info.";
		mqattr += std::to_string(i);
		std::string rbmexinfo_path = mqattr + ".exchange";
		rabbitmq_exchange_info info;
		info.routekeys.clear();
		info.exchange = get_sysconfig(rbmexinfo_path.c_str(),
				info.exchange.c_str(), &iret);
		if (iret) {
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"rabbitmq not load %d exchange", i);
			return -ENOTAVAIL;
		}
		rbmexinfo_path = mqattr + ".exchangetype";
		info.exchange_type = get_sysconfig(rbmexinfo_path.c_str(),
				info.exchange_type.c_str(), &iret);
		if (iret) {
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"rabbitmq not load %d exchange type", i);
			return -ENOTAVAIL;
		}
		rbmexinfo_path = mqattr + ".routing-count";
		int routecnt = 0;
		routecnt = get_sysconfig(rbmexinfo_path.c_str(),
				routecnt, &iret);
		if (iret || routecnt < 0) {
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"rabbitmq not load %d routekey count", i);
			return -ENOTAVAIL;
		}
		for (int j = 0; j < routecnt; j++) {
			std::string rbmkey_path = mqattr + ".routing-info.";
			rbmkey_path += std::to_string(j) + ".routingkey";
			std::string rmbkey;
			rmbkey = get_sysconfig(rbmkey_path.c_str(),
				rmbkey.c_str(), &iret);
			if (iret) {
				log.e(SNAPSHOT_SNAPSHOT_TAG,
					"rabbitmq not load %d routekey [%d] key", i, j);
				return -ENOTAVAIL;
			}
			info.routekeys.push_back(rmbkey);
		}
		_exchange_info.push_back(info);
	}

	return 0;
}

int snapshot_rabbitmq::init_producing(void)
{
	assert(nullptr != _backend);
	assert(nullptr != _context);
	amqp_socket_t *socket = NULL;
	amqp_bytes_t queuename;

	if (_exchange_info.size() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting amqp exchange info is null");
		return -ELOGIC;
	}

	_conn = amqp_new_connection();
	if (nullptr == _conn) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting amqp create error");
		return -ELOGIC;
	}

	socket = amqp_tcp_socket_new(_conn);
	if (!socket) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting creating TCP socket");
		return -ELOGIC;
	}

	int status = amqp_socket_open(socket, _host.c_str(), _port);
	if (status) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting connect TCP socket: host %s\n", _host.c_str());
		return -ELOGIC;
	}

	amqp_rpc_reply_t ret = amqp_login(_conn,
			"/",
			0,	//channel_max
			131072,	//frame_max, max frame size.
			0,	//heartbeat unknown?
			AMQP_SASL_METHOD_PLAIN,	//sasl_method
			_username.c_str(), _password.c_str());
	if (amqp_return_result(ret, "producting login")) {
		return -ELOGIC;
	}

	amqp_channel_open(_conn, _channel_id);
	ret = amqp_get_rpc_reply(_conn);
	if (amqp_return_result(ret, "producting open channel")) {
		return -ELOGIC;
	}

	// for(auto &exc_item:_exchange_info) {
	// 	amqp_exchange_declare(_conn,
	// 			_channel_id,
	// 			amqp_cstring_bytes(exc_item.exchange.c_str()),
	// 			amqp_cstring_bytes(exc_item.exchange_type.c_str()),	//"fanout"  "direct" "topic"
	// 			0,
	// 			0,
	// 			0,
	// 			0,
	// 			amqp_empty_table);
	// 	ret = amqp_get_rpc_reply(_conn);
	// 	if (amqp_return_result(ret, "producting declare exchange")) {
	// 		return -ELOGIC;
	// 	}
	// }
	log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"product init connect finished");
	return 0;
}

int snapshot_rabbitmq::retry_init_consuming(void)
{
	log.d(SNAPSHOT_SNAPSHOT_TAG, "retry init consuming\n");
	if (_sock_fd && _backend) {
		_backend->removefd(_context, _sock_fd);
		_sock_fd = 0;
	}
	amqp_rpc_reply_t ret = amqp_channel_close(_conn,
			_channel_id, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close channel");

	ret = amqp_connection_close(_conn, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close connection");

	int retval = amqp_destroy_connection(_conn);
	if (AMQP_STATUS_OK != retval) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"destroy connection error [%d]", retval);
	}

	_conn = nullptr;

	return init_consuming();
}

int snapshot_rabbitmq::retry_init_producing(void)
{
	amqp_rpc_reply_t ret = amqp_channel_close(_conn,
			_channel_id, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close channel");

	ret = amqp_connection_close(_conn, AMQP_REPLY_SUCCESS);
	amqp_return_result(ret, "close connection");

	int retval = amqp_destroy_connection(_conn);
	if (AMQP_STATUS_OK != retval) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"destroy connection error [%d]", retval);
	}

	_conn = nullptr;
	return init_producing();
}

int snapshot_rabbitmq::init_consuming(void)
{
	assert(nullptr != _backend);
	assert(nullptr != _context);
	amqp_socket_t *socket = NULL;
	amqp_bytes_t queuename;
	log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"init_consuming start");
	if (_exchange_info.size() == 0) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"producting amqp exchange info is null");
		return -ELOGIC;
	}

	_conn = amqp_new_connection();
	if (nullptr == _conn) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"consuming amqp create error");
		return -ELOGIC;
	}

	socket = amqp_tcp_socket_new(_conn);
	if (!socket) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"consuming creating TCP socket");
		return -ELOGIC;
	}

	int status = amqp_socket_open(socket, _host.c_str(), _port);
	if (status) {
		log.e(SNAPSHOT_SNAPSHOT_TAG, 
			"consuming connect TCP socket %s", _host.c_str());
		return -ELOGIC;
	}

	amqp_rpc_reply_t ret = amqp_login(_conn,
			"/",
			0,	//channel_max
			131072,	//frame_max, max frame size.
			0,	//heartbeat unknown?
			AMQP_SASL_METHOD_PLAIN,	//sasl_method
			_username.c_str(), _password.c_str());

	if (amqp_return_result(ret, "consuming login")) {
		return -ELOGIC;
	}

	amqp_channel_open(_conn, _channel_id);
	ret = amqp_get_rpc_reply(_conn);
	if (amqp_return_result(ret, "consuming open channel")) {
		return -ELOGIC;
	}
	
	for(auto &exc_item:_exchange_info) {
		amqp_exchange_declare(_conn,
				_channel_id,
				amqp_cstring_bytes(exc_item.exchange.c_str()),
				amqp_cstring_bytes(exc_item.exchange_type.c_str()),	//"fanout"  "direct" "topic"
				0,
				0,
				0,
				0,
				amqp_empty_table);
		ret = amqp_get_rpc_reply(_conn);
		if (amqp_return_result(ret, "consuming declare exchange")) {
			return -ELOGIC;
		}

		amqp_queue_declare_ok_t *r = amqp_queue_declare(_conn,
				_channel_id,
				amqp_empty_bytes,
				0,
				0,
				0,
				1,
				amqp_empty_table);
		ret = amqp_get_rpc_reply(_conn);		
		if (amqp_return_result(ret, "consuming declare queue")) {
			return -ELOGIC;
		}

		queuename = amqp_bytes_malloc_dup(r->queue);
		if (queuename.bytes == NULL) {
			log.e(SNAPSHOT_SNAPSHOT_TAG,
			"Out of memory while copying queue name");
			return -ENOMEMORY;
		}

		for (auto&key_item:exc_item.routekeys) {
			amqp_queue_bind(_conn, 
				_channel_id, 
				queuename,
				amqp_cstring_bytes(exc_item.exchange.c_str()),
				amqp_cstring_bytes(key_item.c_str()),
				amqp_empty_table);
			ret = amqp_get_rpc_reply(_conn);		
			if (amqp_return_result(ret, "consuming bind queue")) {
				return -ELOGIC;
			}
		}

		amqp_basic_consume(_conn,
				_channel_id,
				queuename,
				amqp_empty_bytes,
				0,
				1,
				0,
				amqp_empty_table);
		ret = amqp_get_rpc_reply(_conn);		
		if (amqp_return_result(ret, "consuming")) {
			return -ELOGIC;
		}
		amqp_bytes_free(queuename);
	}

	_sock_fd = amqp_get_sockfd(_conn);
	_backend->addfd(_context, _sock_fd, BACKEND_POLLIN,
		consume_cb, (void*) this);
	log.d(SNAPSHOT_SNAPSHOT_TAG, 
			"consuming init connect finished");
	return 0;
}


int snapshot_rabbitmq::amqp_return_result(amqp_rpc_reply_t &x,
	char const *desc) {
	switch (x.reply_type) {
	case AMQP_RESPONSE_NORMAL:
		return 0;

	case AMQP_RESPONSE_NONE:
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"%s: missing RPC reply type!\n", desc);
		break;

	case AMQP_RESPONSE_LIBRARY_EXCEPTION:
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"%s: %s\n", desc, amqp_error_string2(x.library_error));
		return -ENOTCONN;

	case AMQP_RESPONSE_SERVER_EXCEPTION:
		switch (x.reply.id) {
		case AMQP_CONNECTION_CLOSE_METHOD: {
			amqp_connection_close_t *m =
				(amqp_connection_close_t *)x.reply.decoded;
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"%s: server connection error %uh, message: %.*s\n",
				desc, m->reply_code, (int)m->reply_text.len,
				(char *)m->reply_text.bytes);
			break;
		}
		case AMQP_CHANNEL_CLOSE_METHOD: {
			amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"%s: server channel error %uh, message: %.*s\n",
				desc, m->reply_code, (int)m->reply_text.len,
				(char *)m->reply_text.bytes);
			break;
		}
		default:
			log.e(SNAPSHOT_SNAPSHOT_TAG,
				"%s: unknown server error, method id 0x%08X\n",
				desc, x.reply.id);
			break;
		}
		break;
	}
	return -ELOGIC;
}

int snapshot_rabbitmq::consume_cb(int fd, int revents, void* data)
{
	if (nullptr == data) {
		return -EBADPARM;
	}
	if (!(revents & BACKEND_POLLIN)) {
		log.e(SNAPSHOT_SNAPSHOT_TAG,
			"unknown event %d", revents);
		return -ENOTAVAIL;
	}
	auto* idx_rmq = reinterpret_cast<snapshot_rabbitmq*>(data);
	idx_rmq->on_consume(fd, revents);
	return 0;
}

int snapshot_rabbitmq::on_consume(int fd, int revents)
{
	assert(nullptr != _conn);
	assert(_sock_fd == fd);
	amqp_rpc_reply_t res;
	amqp_envelope_t envelope;
	amqp_maybe_release_buffers(_conn);
	res = amqp_consume_message(_conn, &envelope, NULL, 0);
	int iret = amqp_return_result(res, "consuming");
	if (iret) {
		if (_cb) {
			_cb->on_consuming_error(iret);
		}
		if (-ENOTCONN == iret) {
			if (retry_init_consuming()) {
				if (_retry_timer) {
					_retry_timer->start();
				}
			}
		}
		printf("consuming error is %d\n", iret);
		amqp_destroy_envelope(&envelope);
		return -ELOGIC;
	}
	log.d(SNAPSHOT_SNAPSHOT_TAG,
		"Delivery %u, exchange %.*s routingkey %.*s\n",
			(unsigned)envelope.delivery_tag, (int)envelope.exchange.len,
			(char *)envelope.exchange.bytes, (int)envelope.routing_key.len,
			(char *)envelope.routing_key.bytes);

	if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
		log.d(SNAPSHOT_SNAPSHOT_TAG,
			"Content-type: %.*s\n",
			(int)envelope.message.properties.content_type.len,
			(char *)envelope.message.properties.content_type.bytes);
	}
	if (envelope.message.body.len == 0) {
		amqp_destroy_envelope(&envelope);
		log.e(SNAPSHOT_SNAPSHOT_TAG, "no message data\n");
		return -EDATANOTAVAIL;
	}
	if (_cb) {
		std::string exchange((char *)envelope.exchange.bytes,
			(int)envelope.exchange.len);
		std::string routingkey((char *)envelope.routing_key.bytes,
			envelope.routing_key.len);
		exchange += "." + routingkey;
		_cb->on_consuming((char *)exchange.c_str(),
			exchange.length(),
			envelope.message.body.bytes,
			envelope.message.body.len);
	}
	// amqp_dump(envelope.message.body.bytes, envelope.message.body.len);

	amqp_destroy_envelope(&envelope);
	return 0;
}


static void dump_row(long count, int numinrow, int *chs) {
  int i;

  printf("%08lX:", count - numinrow);

  if (numinrow > 0) {
    for (i = 0; i < numinrow; i++) {
      if (i == 8) {
        printf(" :");
      }
      printf(" %02X", chs[i]);
    }
    for (i = numinrow; i < 16; i++) {
      if (i == 8) {
        printf(" :");
      }
      printf("   ");
    }
    printf("  ");
    for (i = 0; i < numinrow; i++) {
      if (isprint(chs[i])) {
        printf("%c", chs[i]);
      } else {
        printf(".");
      }
    }
  }
  printf("\n");
}

static int rows_eq(int *a, int *b) {
  int i;

  for (i = 0; i < 16; i++)
    if (a[i] != b[i]) {
      return 0;
    }

  return 1;
}

void snapshot_rabbitmq::amqp_dump(void const *buffer, size_t len)
{
	unsigned char *buf = (unsigned char *)buffer;
	long count = 0;
	int numinrow = 0;
	int chs[16];
	int oldchs[16] = {0};
	int showed_dots = 0;
	size_t i;

	for (i = 0; i < len; i++) {
	int ch = buf[i];

	if (numinrow == 16) {
		int j;

		if (rows_eq(oldchs, chs)) {
		if (!showed_dots) {
			showed_dots = 1;
			printf(
				"          .. .. .. .. .. .. .. .. : .. .. .. .. .. .. .. ..\n");
		}
		} else {
		showed_dots = 0;
		dump_row(count, numinrow, chs);
		}

		for (j = 0; j < 16; j++) {
		oldchs[j] = chs[j];
		}

		numinrow = 0;
	}

	count++;
	chs[numinrow++] = ch;
	}

	dump_row(count, numinrow, chs);

	if (numinrow != 0) {
	printf("%08lX:\n", count);
	}
}


}}	//zas::vehicle_snapshot_service




