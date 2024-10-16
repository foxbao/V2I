#ifndef __CXX_ZAS_RPC_ERROR_DEF_H__
#define __CXX_ZAS_RPC_ERROR_DEF_H__


namespace zas {
namespace mware {
namespace rpc {

#define RPC_ERR_FILEINFO __FILE__, __LINE__

enum err_category
{
	errcat_unknown = 0,
	errcat_invalidargument,
	errcat_logicerror,
	errcat_proxy,
};

enum err_scenario
{
	err_scenario_unknown = 0,
	err_scenario_get_instance,
	err_scenario_get_namespace,
	err_scenario_get_classname,
	err_scenario_checkcreate_namespace,
	err_scenario_checkcreate_classnode,
	err_scenario_checkcreate_instancenode,
	err_scenario_checkcreate_methodnode,
	err_scenario_sync_namspace_node,
	err_scenario_sync_class_node,
	err_scenario_add_instance,
	err_scenario_add_method,
	err_scenario_handle_namespace_request,
	err_scenario_handle_interface_request,
	err_scenario_regist_interface,
	err_scenario_invoke,
	err_scenario_force_trigger_last_event,
	err_scenario_write,
	err_scenario_read,
	err_scenario_release,
	err_scenario_others,
};

enum errid
{
	errid_unknown = 0,
	errid_service_notready,
	errid_service_timeout,
	errid_evloop_error,
	errid_missing_param,
	errid_invalid_param,
	errid_server_notready,
	errid_return_failure,
	errid_createobj_failure,
	errid_unsupported_feature,
	errid_sanitycheck_failure,
	errid_not_found,
	errid_parsingfile_error,
	errid_empty_object,
	errid_multiple_host_instance,
	errid_host_unexpected_error,
	errid_array_exceed_boundary,
};

}}} // end of namespace zas::mware::zrpc

#endif /*__CXX_ZAS_ZRPC_ERROR_DEF_H__*/