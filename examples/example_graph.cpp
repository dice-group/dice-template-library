#include <iostream>
#include <string>
#include <vector>

#include <dice/template-library/graph.hpp>

namespace dtl = dice::template_library;

// 1. Define custom data for vertices and edges
struct Node {
	std::string name;
};
struct Link {};// Edge data is not needed for this example

int main() {
	using ExGraph = dtl::Graph<Node, Link, boost::undirectedS>;
	ExGraph graph;

	//    Component 1: A-C
	auto const hA = graph.add_vertex({"A"});
	auto const hB = graph.add_vertex({"B"});
	auto const hC = graph.add_vertex({"C"});
	graph.add_edge(hA, hB);
	graph.add_edge(hB, hC);
	graph.add_edge(hC, hA);

	//    Component 2: D-E pair
	auto const hD = graph.add_vertex({"D"});
	auto const hE = graph.add_vertex({"E"});
	graph.add_edge(hD, hE);

	//    Component 3: Isolated node F
	graph.add_vertex({"F"});

	auto const components = graph.connected_components();

	for (auto const &vertex_handle_group : components) {
		ExGraph const subgraph = graph.create_subgraph(vertex_handle_group);

		std::stringstream dot_content;

		auto vertex_labeler = [](ExGraph::ConstVertex const &v) {
			return v.data().name;
		};
		auto edge_labeler = [](ExGraph::ConstEdge const &) {
			return std::string{""};// No edge labels needed
		};

		// Use to_graphviz, passing the stringstream as the ostream
		subgraph.to_graphviz(dot_content, vertex_labeler, edge_labeler);

		// Print the captured content to the console
		std::cout << dot_content.str() << "\n\n";
	}
}