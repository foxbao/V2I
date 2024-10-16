#include <iostream>
#include <vector>

namespace graph {

using namespace std;

class dijgraph {
public:
	dijgraph(int v_count)
		: v_count_(v_count) {
		adj_ = vector<vector<edge>>(v_count, vector<edge>{});
	}
	void addedge(int s, int e, int w);

	void dijkstra(int s, int e);

	void print(int s, int e,vector<int> parent);

	private:
	struct edge { // 表示边
		int sid_; // 边的起始节点
		int eid_; // 边的结束节点
		int w_;   // 边的权重
		edge() = default;
		edge(int s, int e, int w)
			: sid_(s), eid_(e), w_(w) {}
	};
	struct vertex { // 算法实现中，记录第一个节点到这个节点的距离
		int id_;
		int dist_;
		vertex() = default;
		vertex(int id, int dist)
			: id_(id), dist_(dist) {}
	};
	vector<vector<edge>> adj_; // 邻接表
	int v_count_;           // 顶点数
};
}