#include <stdio.h> 

#include<limits>
#include<queue>

#include "dijgraph.h"

namespace graph {

	using namespace std;

	void dijgraph::addedge(int s, int e, int w) {
		if (s < v_count_ && e < v_count_) {
			adj_[s].emplace_back(s, e, w) ;
		}
	}

	void dijgraph::dijkstra(int s, int e) {
		vector<int> parent(v_count_); // 用于还原最短路径, 当前点最短路径的父节点
		vector<vertex> vertexes(v_count_);   // 用于记录s到当前点的最短距离
		for (int i = 0; i < v_count_; ++i) {
			vertexes[i] = vertex(i, numeric_limits<int>::max());
		}
		struct cmp {
			bool operator() (const vertex &v1, const vertex &v2) { return v1.dist_ > v2.dist_;}
		};
		// 小顶堆
		priority_queue<vertex, vector<vertex>, cmp> queue;
		// 标记是否已找到最短距离
		vector<bool> shortest(v_count_, false);

		vertexes[s].dist_ = 0;
		queue.push(vertexes[s]);
		while (!queue.empty()) {
			vertex minVertex = queue.top(); // 保证小顶堆出来的点一定是最小的。
			queue.pop();
			if (minVertex.id_ == e) { break; }
			if (shortest[minVertex.id_]) { continue; } // 之前更新过，是冗余备份
			shortest[minVertex.id_] = true;
			for (int i = 0; i < adj_[minVertex.id_].size(); ++i) { // 遍历节点连通的边
				edge cur_edge = adj_[minVertex.id_].at(i); // 取出当前连通的边
				int next_vid = cur_edge.eid_;
				if (minVertex.dist_ + cur_edge.w_ < vertexes[next_vid].dist_) {
					vertexes[next_vid].dist_ = minVertex.dist_ + cur_edge.w_;
					parent[next_vid] = minVertex.id_;
					queue.push(vertexes[next_vid]);
				}
			}
		}
		cout << s;
		print(s, e, parent);
	}

	void dijgraph::print(int s, int e,vector<int> parent) {
		if (s == e) return;
		print(s, parent[e], parent);
		cout << "->" << e ;
	}
}

using namespace graph;

int main(int argc, char* argv[])
{
	dijgraph g{8};
	g.addedge(0, 1, 1);
	g.addedge(1, 0, 1);
	g.addedge(0, 2, 12);
	g.addedge(2, 0, 12);
	g.addedge(1, 3, 3);
	g.addedge(3, 1, 3);
	g.addedge(1, 2, 9);
	g.addedge(2, 1, 9);
	g.addedge(3, 2, 4);
	g.addedge(2, 3, 4);
	g.addedge(3, 4, 13);
	g.addedge(4, 3, 13);
	g.addedge(2, 4, 5);
	g.addedge(4, 2, 5);
	g.addedge(4, 5, 4);
	g.addedge(5, 4, 4);
	g.addedge(3, 5, 15);
	g.addedge(5, 3, 15);
	g.dijkstra(0, 5);
}