namespace zas {
namespace coreapp {
namespace servicebundle {

// extern initializer
void init_service_updater(void);

// extern destoryer

static void __attribute__ ((constructor)) coreapp_service_bundle_init(void)
{
	init_service_updater();
}

static void __attribute__ ((destructor)) coreapp_service_bundle_destroy(void)
{
}

}}} // end of namespace zas::coreapp::servicebundle
/* EOF */