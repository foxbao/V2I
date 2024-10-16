#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "uicore/view.h"
#include "uicore/region.h"
#include "uicore/surface.h"

#include "utils/mutex.h"
#include "utils/evloop.h"
#include "utils/timer.h"
#include "uicore/window.h"

#include "hwgrph/imageloader.h"
#include "hwgrph/font.h"
#include "hwgrph/format.h"

using namespace zas::utils;
using namespace zas::hwgrph;
using namespace zas::uicore;

static void test_create_nameops(view_t** root)
{
	*root = view_t::create();
	assert(*root != NULL);
	view_t* r = *root;
	int ret = r->set_name("root");
	assert(ret == 0);

	// read name and check if it equals to "root"
	char buf[64];
	ret = r->get_name(buf, 10);
	assert(ret > 0);
	assert(!strcmp(buf, "root"));

	// change name to "12345678"
	ret = r->set_name("12345678");
	assert(ret == 0);
	ret = r->get_name(buf, 10);
	assert(ret > 0);
	assert(!strcmp(buf, "12345678"));

	// clear the name
	ret = r->set_name(NULL);
	assert(ret == 0);
	ret = r->get_name(buf, 10);
	assert(ret == 0);

	// check with setting a invalid name
	ret = r->set_name("abc.def");
	assert(ret == -EINVALID);

	// change to a long name
	ret = r->set_name("root_node name");
	assert(ret == 0);
	ret = r->get_name(buf, 32);
	assert(ret > 0);
	assert(!strcmp(buf, "root_node name"));
}

struct node_tree
{
	view_t* root;
	view_t* container1;
	view_t* container2;
	view_t* group1;
	view_t* grp1_chd1;
	view_t* grp1_chd2;
};

static void prepare_node_tree(view_t* root, node_tree* t)
{
	rect_t r;

	t->root = root;
	root->setpos(0, 0, 300, 300);
	t->container1 = view_t::create(t->root, "container1");
	t->container1->setpos(10, 10, 100, 250);
	t->container2 = view_t::create(t->root, rect_t(150, 10, 100, 250));
	t->group1 = view_t::create(t->root, rect_t(260, 15), true, "group1", true);
	t->grp1_chd1 = view_t::create(t->group1, rect_t(0, 0, 35, 35));

	// group1 shall become (260, 15, 35, 35)
	t->group1->get_clientrect(r);
	assert(r.left == 260 && r.top == 15 && r.width == 35 && r.height == 35);
	t->group1->get_wraprect(r);
	assert(r.left == 260 && r.top == 15 && r.width == 35 && r.height == 35);

	t->grp1_chd2 = view_t::create(t->group1, rect_t(-10, -10), true, "group2", true);

	// group1 shall become (250, 5, 45, 45)
	t->group1->get_clientrect(r);
	assert(r.left == 250 && r.top == 5 && r.width == 45 && r.height == 45);
	// and the grp1_chd1 shall become (10, 10, 35, 35)
	t->grp1_chd1->get_clientrect(r);
	assert(r.left == 10 && r.top == 10 && r.width == 35 && r.height == 35);
	// and the grp1_chd2 shall become (0, 0, 0, 0)
	t->grp1_chd2->get_clientrect(r);
	assert(r.left == 0 && r.top == 0 && r.width == 0 && r.height == 0);
}

static void test_server_evloop(void)
{
	int ret;
	evloop* evloop = evloop::inst(); 
	evloop->setrole(evloop_role_server);
	evloop->updateinfo(evlcli_info_client_name, "zas.syssvr")
		->updateinfo(evlcli_info_instance_name, "init");
	evloop->start(true, true);
	ret = evloop->start();
	assert(ret == -EEXISTS);

	// retrive the client
	evlclient cl = evloop->getclient("zas.syssvr", "init");
	assert(cl);
}

static void test_client_evloop(int argc, char* argv[])
{
	const char* client_name = NULL;
	const char* inst_name = NULL;

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "--client_name")) {
			if (i + 2 > argc) break;
			client_name = argv[i + 1];
			inst_name = argv[i + 2];
			break;
		}
	}
	evloop* evloop = evloop::inst();
	evloop->setrole(evloop_role_client);
	evloop->updateinfo(evlcli_info_client_name, client_name ? client_name : "test_client")
		->updateinfo(evlcli_info_instance_name, inst_name ? inst_name : "mytest");
	evloop->start(true, true);

	// retrive the client
	evlclient cl = evloop->getclient("abc", "def");
	if (!cl) {
		cl = evloop->getclient("test_client", "mytest");
		assert(cl);
	}
}

static void test_image(void)
{
}

static void test_font(void)
{
	auto* face = fontface::register_face(NULL, "file:///home/jimview/data/fonts/noto-Regular.otf", 0);
	publish_fonts();
	fontface::register_face("private: Noto Sans S Chinese",
		"file:///home/jimview/data/fonts/noto-Regular.otf", 0);
	font f = createfont("private: Noto Sans S Chinese", fts_regular, 10);
	textline tl = create_textline("gthis is a test");	
	tl->setfont(f);
	tl->setcacher(get_global_fontcacher());
}

namespace zas {
namespace uicore {
	int do_render(void);
	void wayland_client_start(void);
}}

class test_timer : public timer
{
public:
	test_timer(uint32_t interval)
	: timer(interval) {}

	void on_timer(void) {
		printf("test timer\n");
		start();
	}
};

int main(int argc, char* argv[])
{
	view_t* root;
	node_tree tree;

	test_create_nameops(&root);
	prepare_node_tree(root, &tree);

	if ((argc >= 2 && !strcmp(argv[1], "server"))) {
		printf("running as server.\n");
		test_server_evloop();
	}
	else {
		printf("running as client.\n");
#if 0
		test_client_evloop(argc, argv);
		
		wayland_client_start();
		msleep(1000);
		window* win = create_window();
		win->set_position(iposition_t(50, 50), igeometry_t(100, 100));
		win->show();
		msleep(1000);
		win->close();
		msleep(1000);
		win->set_position(iposition_t(100, 100), igeometry_t(200, 200));
		win->show();
		//msleep(1000);
		//win->close();
		test_image();
#endif
		set_as_font_manager();
		test_font();
	}
	do_render();

	root->release();
	fprintf(stderr, "all success.\n");
	getchar();
	return 0;
}