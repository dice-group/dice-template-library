#ifndef DICE_TEMPLATE_LIBRARY_GRAPH_HPP
#define DICE_TEMPLATE_LIBRARY_GRAPH_HPP

#include <concepts>
#include <functional>
#include <ranges>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/property_map/property_map.hpp>

namespace dice::template_library {


	namespace detail {
		template<typename T>
		concept IsDirectedness = std::same_as<T, boost::directedS> ||
								 std::same_as<T, boost::undirectedS> ||
								 std::same_as<T, boost::bidirectionalS>;
	}// namespace detail

	template<typename VertexDataT, typename EdgeDataT, detail::IsDirectedness DirectednessT = boost::directedS>
	struct Graph;

	namespace detail {
		template<typename GraphType, typename HandleType>
		struct Proxy {
		private:
			constexpr static bool is_edge_proxy = std::same_as<HandleType, typename GraphType::EdgeHandle>;
			constexpr static bool is_vertex_proxy = std::same_as<HandleType, typename GraphType::VertexHandle>;
			constexpr static bool is_mut_graph = !std::is_const_v<GraphType>;

			template<typename, typename, IsDirectedness>
			friend struct dice::template_library::Graph;
			friend struct Proxy<std::remove_const_t<GraphType>, HandleType>;// for casting

			GraphType *graph_;
			HandleType handle_;

			Proxy(GraphType *graph, HandleType handle) : graph_(graph), handle_(handle) {}

		public:
			HandleType handle() const { return handle_; }
			auto &data()
				requires(is_mut_graph)
			{ return graph_->get_data(handle_); }
			auto const &data() const { return graph_->get_data(handle_); }
			auto source() const
				requires is_edge_proxy
			{ return graph_->source(handle_); }
			auto target() const
				requires is_edge_proxy
			{ return graph_->target(handle_); }
			auto out_edges() const
				requires is_vertex_proxy
			{ return graph_->out_edges(handle_); }
			auto neighbors() const
				requires is_vertex_proxy
			{ return graph_->neighbors(handle_); }

			operator Proxy<GraphType const, HandleType>() const
				requires(is_mut_graph)
			{
				return Proxy<GraphType const, HandleType>(graph_, handle_);
			}
		};
	}// namespace detail


	template<typename VertexDataT, typename EdgeDataT, detail::IsDirectedness DirectednessT>
	struct Graph {
	private:
		struct BoostVertexProps {
			VertexDataT data;
		};
		struct BoostEdgeProps {
			EdgeDataT data;
		};
		using BGLGraph = boost::adjacency_list<boost::vecS, boost::vecS, DirectednessT, BoostVertexProps, BoostEdgeProps>;

		template<typename, typename>
		friend struct detail::Proxy;

		VertexDataT &get_data(typename BGLGraph::vertex_descriptor v) { return graph_[v].data; }
		VertexDataT const &get_data(typename BGLGraph::vertex_descriptor v) const { return graph_[v].data; }
		EdgeDataT &get_data(typename BGLGraph::edge_descriptor e) { return graph_[e].data; }
		EdgeDataT const &get_data(typename BGLGraph::edge_descriptor e) const { return graph_[e].data; }

		BGLGraph graph_;

	public:
		using VertexData = VertexDataT;
		using EdgeData = EdgeDataT;
		using Directedness = DirectednessT;
		using VertexHandle = typename BGLGraph::vertex_descriptor;
		using EdgeHandle = typename BGLGraph::edge_descriptor;
		using Vertex = detail::Proxy<Graph, VertexHandle>;
		using Edge = detail::Proxy<Graph, EdgeHandle>;
		using ConstVertex = detail::Proxy<Graph const, VertexHandle>;
		using ConstEdge = detail::Proxy<Graph const, EdgeHandle>;

		Graph() = default;
		~Graph() = default;
		Graph(Graph &&other) noexcept = default;
		Graph &operator=(Graph &&other) noexcept = default;
		Graph(Graph const &other) = delete;
		Graph &operator=(Graph const &other) = delete;

		VertexHandle add_vertex(VertexData &&data = {}) {
			VertexHandle v = boost::add_vertex(graph_);
			graph_[v].data = std::move(data);
			return v;
		}

		EdgeHandle add_edge(VertexHandle source, VertexHandle target, EdgeData &&data = {}) {
			auto [edge, success] = boost::add_edge(source, target, graph_);
			graph_[edge].data = std::move(data);
			return edge;
		}

		Vertex vertex(VertexHandle v) { return {this, v}; }
		ConstVertex vertex(VertexHandle v) const { return {this, v}; }
		Edge edge(EdgeHandle e) { return {this, e}; }
		ConstEdge edge(EdgeHandle e) const { return {this, e}; }
		Vertex operator[](VertexHandle v) { return vertex(v); }
		ConstVertex operator[](VertexHandle v) const { return vertex(v); }
		Edge operator[](EdgeHandle e) { return edge(e); }
		ConstEdge operator[](EdgeHandle e) const { return edge(e); }

		auto vertices() {
			auto [begin, end] = boost::vertices(graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](VertexHandle v) { return vertex(v); });
		}
		auto vertices() const {
			auto [begin, end] = boost::vertices(graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](VertexHandle v) { return vertex(v); });
		}
		auto edges() {
			auto [begin, end] = boost::edges(graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](EdgeHandle e) { return edge(e); });
		}
		auto edges() const {
			auto [begin, end] = boost::edges(graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](EdgeHandle e) { return edge(e); });
		}
		auto out_edges(VertexHandle v) {
			auto [begin, end] = boost::out_edges(v, graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](EdgeHandle e) { return edge(e); });
		}
		auto out_edges(VertexHandle v) const {
			auto [begin, end] = boost::out_edges(v, graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](EdgeHandle e) { return edge(e); });
		}
		auto neighbors(VertexHandle v) {
			auto [begin, end] = boost::adjacent_vertices(v, graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](VertexHandle v_adj) { return vertex(v_adj); });
		}
		auto neighbors(VertexHandle v) const {
			auto [begin, end] = boost::adjacent_vertices(v, graph_);
			return std::ranges::subrange(begin, end) | std::views::transform([this](VertexHandle v_adj) { return vertex(v_adj); });
		}

		[[nodiscard]] size_t num_vertices() const { return boost::num_vertices(graph_); }
		[[nodiscard]] size_t num_edges() const { return boost::num_edges(graph_); }
		[[nodiscard]] bool empty() const { return num_vertices() == 0; }
		VertexHandle source(EdgeHandle e) const { return boost::source(e, graph_); }
		VertexHandle target(EdgeHandle e) const { return boost::target(e, graph_); }

		std::vector<std::vector<VertexHandle>> connected_components() const {
			if (empty()) return {};
			std::vector<int> component_map(num_vertices());
			int num_components = boost::connected_components(graph_, component_map.data());
			std::vector<std::vector<VertexHandle>> components(num_components);
			for (auto const &v : vertices()) {
				components[component_map[v.handle()]].push_back(v.handle());
			}
			return components;
		}

		std::vector<std::vector<VertexHandle>> strong_components() const {
			static_assert(std::is_same_v<Directedness, boost::directedS>, "Strong components only applicable to directed graphs.");
			if (empty())
				return {};
			std::vector<int> component_map(num_vertices());
			auto const index_map = boost::get(boost::vertex_index, graph_);
			auto const num_components = boost::strong_components(graph_, boost::make_iterator_property_map(component_map.begin(), index_map));
			std::vector<std::vector<VertexHandle>> components(num_components);
			for (auto const &v : vertices()) {
				components[component_map[v.handle()]].push_back(v.handle());
			}
			return components;
		}

		Graph create_subgraph(std::vector<VertexHandle> const &handles_to_keep) const {
			Graph sub;

			std::unordered_map<VertexHandle, VertexHandle> old_to_new;
			for (auto const &old_v : handles_to_keep) {
				VertexHandle new_v = sub.add_vertex(VertexData(this->get_data(old_v)));
				old_to_new[old_v] = new_v;
			}
			for (auto const &old_v : handles_to_keep) {
				for (auto const &edge : this->out_edges(old_v)) {
					VertexHandle old_target = edge.target();
					if (old_to_new.contains(old_target)) {
						sub.add_edge(old_to_new[old_v], old_to_new[old_target], EdgeData(edge.data()));
					}
				}
			}
			return sub;
		}

		template<typename VertexLabeler, typename EdgeLabeler>
			requires std::invocable<VertexLabeler, ConstVertex const &> &&
					 std::invocable<EdgeLabeler, ConstEdge const &>
		void to_graphviz(std::ostream &os, VertexLabeler vertex_labeler, EdgeLabeler edge_labeler) const {
			boost::write_graphviz(os, graph_, [this, &vertex_labeler](auto &out, auto v_handle) { out << "[label=\"" << vertex_labeler(this->vertex(v_handle)) << "\"]"; }, [this, &edge_labeler](auto &out, auto e_handle) { out << "[label=\"" << edge_labeler(this->edge(e_handle)) << "\"]"; });
		}
	};

}// namespace dice::template_library

#endif