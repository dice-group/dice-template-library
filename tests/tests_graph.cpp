#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <dice/template-library/graph.hpp>

#include <algorithm>
#include <string>
#include <type_traits>

TEST_SUITE("Graph") {
	using namespace dice::template_library;

	// --- Test Data Structures ---
	struct NodeData {
		std::string name;
		int value{};

		bool operator==(NodeData const &other) const {
			return name == other.name && value == other.value;
		}
	};

	struct EdgeData {
		double weight = 0.0;
		bool is_optional = false;
	};

	// --- Static Type Assertions ---
	// A Graph should not be copyable, but it should be movable.
	static_assert(!std::is_copy_constructible_v<Graph<NodeData, EdgeData>>);
	static_assert(!std::is_copy_assignable_v<Graph<NodeData, EdgeData>>);
	static_assert(std::is_move_constructible_v<Graph<NodeData, EdgeData>>);
	static_assert(std::is_move_assignable_v<Graph<NodeData, EdgeData>>);


	// --- Test Cases ---

	TEST_CASE("Default Construction") {
		Graph<NodeData, EdgeData> g;
		CHECK(g.empty());
		CHECK_EQ(g.num_vertices(), 0);
		CHECK_EQ(g.num_edges(), 0);
	}

	TEST_CASE("Vertex and Edge Manipulation") {
		Graph<NodeData, EdgeData> g;

		// Add vertices
		auto v1 = g.add_vertex({"A", 10});
		auto v2 = g.add_vertex({"B", 20});

		CHECK_FALSE(g.empty());
		CHECK_EQ(g.num_vertices(), 2);

		// Add an edge
		auto e1 = g.add_edge(v1, v2, {1.5, false});
		CHECK_EQ(g.num_edges(), 1);

		// Access data via proxy
		CHECK_EQ(g[v1].data().name, "A");
		CHECK_EQ(g[v2].data().value, 20);
		CHECK_EQ(g[e1].data().weight, 1.5);

		// Modify data via proxy
		g[v1].data().value = 15;
		CHECK_EQ(g[v1].data().value, 15);

		// Check edge source and target
		CHECK_EQ(g.source(e1), v1);
		CHECK_EQ(g.target(e1), v2);
	}

	TEST_CASE("Iteration") {
		Graph<NodeData, EdgeData> g;
		auto v1 = g.add_vertex({"A", 10});
		auto v2 = g.add_vertex({"B", 20});
		auto v3 = g.add_vertex({"C", 30});
		g.add_edge(v1, v2, {1.2});
		g.add_edge(v1, v3, {2.3});

		SUBCASE("Vertices Range") {
			int count = 0;
			int total_value = 0;
			for (auto const &vertex : g.vertices()) {
				count++;
				total_value += vertex.data().value;
			}
			CHECK_EQ(count, 3);
			CHECK_EQ(total_value, 60);
		}

		SUBCASE("Edges Range") {
			int count = 0;
			double total_weight = 0.0;
			for (auto const &edge : g.edges()) {
				count++;
				total_weight += edge.data().weight;
			}
			CHECK_EQ(count, 2);
			CHECK_EQ(total_weight, doctest::Approx(3.5));
		}

		SUBCASE("Outgoing Edges and Neighbors") {
			int neighbor_count = 0;
			std::vector<std::string> neighbor_names;
			for (auto const &neighbor : g.neighbors(v1)) {
				neighbor_count++;
				neighbor_names.push_back(neighbor.data().name);
			}
			CHECK_EQ(neighbor_count, 2);
			// Sorting because neighbor order is not guaranteed
			std::ranges::sort(neighbor_names);
			CHECK_EQ(neighbor_names[0], "B");
			CHECK_EQ(neighbor_names[1], "C");

			int out_edge_count = 0;
			for (auto const &edge : g.out_edges(v1)) {
				out_edge_count++;
				CHECK_EQ(edge.source(), v1);
			}
			CHECK_EQ(out_edge_count, 2);
		}
	}

	TEST_CASE("Const Graph Access and Iteration") {
		Graph<NodeData, EdgeData> g_mut;
		auto v1 = g_mut.add_vertex({"const_A", 1});
		auto v2 = g_mut.add_vertex({"const_B", 2});
		g_mut.add_edge(v1, v2, {9.9});

		Graph<NodeData, EdgeData> const &g = g_mut;

		CHECK_EQ(g.num_vertices(), 2);
		CHECK_EQ(g[v1].data().name, "const_A");

		int count = 0;
		for (auto const &vertex : g.vertices()) {
			count++;
			// The following line should not compile if uncommented, which is correct
			// vertex.data().value = 5;
		}
		CHECK_EQ(count, 2);
	}

	TEST_CASE("Algorithms") {
		SUBCASE("Connected Components (Undirected)") {
			Graph<NodeData, EdgeData, boost::undirectedS> g;
			// Component 1
			auto vA = g.add_vertex({"A"});
			auto vB = g.add_vertex({"B"});
			g.add_edge(vA, vB);

			// Component 2
			auto vC = g.add_vertex({"C"});
			auto vD = g.add_vertex({"D"});
			auto vE = g.add_vertex({"E"});
			g.add_edge(vC, vD);
			g.add_edge(vD, vE);

			// Component 3 (isolated vertex)
			auto vF = g.add_vertex({"F"});

			auto components = g.connected_components();
			CHECK_EQ(components.size(), 3);

			// Sort by size to make checks deterministic
			std::sort(components.begin(), components.end(),
					  [](auto const &a, auto const &b) { return a.size() < b.size(); });

			CHECK_EQ(components[0].size(), 1);// F
			CHECK_EQ(components[1].size(), 2);// A, B
			CHECK_EQ(components[2].size(), 3);// C, D, E
		}

		SUBCASE("Strongly Connected Components (Directed)") {
			Graph<NodeData, EdgeData, boost::directedS> g;
			auto v1 = g.add_vertex({"1"});
			auto v2 = g.add_vertex({"2"});
			auto v3 = g.add_vertex({"3"});
			auto v4 = g.add_vertex({"4"});

			g.add_edge(v1, v2);
			g.add_edge(v2, v3);
			g.add_edge(v3, v1);// Cycle v1-v2-v3
			g.add_edge(v3, v4);// Edge out of the cycle

			auto components = g.strong_components();
			CHECK_EQ(components.size(), 2);

			// The cycle {v1, v2, v3} is one component, {v4} is the other.
			auto it_cycle = std::ranges::find_if(components,
												 [](auto const &c) { return c.size() == 3; });
			auto it_single = std::ranges::find_if(components,
												  [](auto const &c) { return c.size() == 1; });

			CHECK(it_cycle != components.end());
			CHECK(it_single != components.end());
			CHECK_EQ((*it_single)[0], v4);
		}
	}

	TEST_CASE("Utilities") {
		SUBCASE("create_subgraph") {
			Graph<NodeData, EdgeData> g;
			auto v1 = g.add_vertex({"A"});
			auto v2 = g.add_vertex({"B"});
			auto v3 = g.add_vertex({"C"});
			auto v4 = g.add_vertex({"D"});

			g.add_edge(v1, v2, {1.0});// Should be in subgraph
			g.add_edge(v2, v3, {2.0});// Should NOT be in subgraph
			g.add_edge(v1, v4, {3.0});// Should be in subgraph
			g.add_edge(v3, v4, {4.0});// Should NOT be in subgraph

			// Create a subgraph with only A, B, and D
			auto sub_handles = std::vector{v1, v2, v4};
			auto sub_g = g.create_subgraph(sub_handles);

			CHECK_EQ(sub_g.num_vertices(), 3);
			CHECK_EQ(sub_g.num_edges(), 2);// Only edges v1->v2 and v1->v4

			bool found_A = false;
			bool found_B = false;
			bool found_C = false;// Should not be found
			bool found_D = false;
			for (auto const &v : sub_g.vertices()) {
				if (v.data().name == "A") found_A = true;
				if (v.data().name == "B") found_B = true;
				if (v.data().name == "C") found_C = true;
				if (v.data().name == "D") found_D = true;
			}
			CHECK(found_A);
			CHECK(found_B);
			CHECK(found_D);
			CHECK_FALSE(found_C);
		}

		SUBCASE("to_graphviz") {
			Graph<NodeData, EdgeData> g;
			auto v1 = g.add_vertex({"A", 1});
			g.add_vertex({"B", 2});

			// Just check that it compiles and runs without error
			std::stringstream ss;
			auto vertex_labeler = [](auto const &v) { return v.data().name; };
			auto edge_labeler = [](auto const &e) { return std::to_string(e.data().weight); };

			CHECK_NOTHROW(g.to_graphviz(ss, vertex_labeler, edge_labeler));

			// A basic check on the output
			auto output = ss.str();
			CHECK(output.find("digraph G") != std::string::npos);
			CHECK(output.find("[label=\"A\"]") != std::string::npos);
			CHECK(output.find("[label=\"B\"]") != std::string::npos);
		}
	}
	TEST_CASE("Proxy Casting from Mutable to Const") {
		using MyGraph = Graph<NodeData, EdgeData>;
		MyGraph g;

		auto vA = g.add_vertex({"A"});
		auto eAB = g.add_edge(vA, g.add_vertex({"B"}));

		// Helper function that only accepts const proxies
		auto get_const_vertex_name = [](MyGraph::ConstVertex const &v) {
			return v.data().name;
		};

		SUBCASE("Implicit conversion in function call") {
			MyGraph::Vertex const mut_vertex = g.vertex(vA);

			// This call only compiles if the mutable proxy can be implicitly
			// cast to a const proxy.
			CHECK_EQ(get_const_vertex_name(mut_vertex), "A");
		}

		SUBCASE("Explicit conversion via assignment") {
			MyGraph::Vertex const mut_vertex = g[vA];
			MyGraph::Edge const mut_edge = g[eAB];

			// Assign a mutable proxy to a const one
			MyGraph::ConstVertex const const_vertex = mut_vertex;
			MyGraph::ConstEdge const const_edge = mut_edge;

			CHECK_EQ(const_vertex.handle(), mut_vertex.handle());
			CHECK_EQ(const_edge.handle(), mut_edge.handle());

			// This should not compile if uncommented, proving it's truly const
			// const_vertex.data().name = "C";
		}
	}
}