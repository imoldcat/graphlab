#include <graphlab.hpp>
#include <string>

using namespace graphlab;
using namespace std;

struct web_page {
	string pagename;
	double pagerank;
	web_page():pagerank(0.0) {}
	explicit web_page(string name):pagename(name), pagerank(0.0) {}
	
	void save(oarchive& oarc) const {
		oarc << pagename << pagerank;
	}

	void load(iarchive& iarc) {
		iarc >> pagename >> pagerank;
	}
};

typedef distributed_graph<web_page, empty> graph_type;

bool line_parser(graph_type& graph, const string& filename, 
		const string& textline) {
	stringstream strm(textline);
	vertex_id_type vid;
	string pagename;
	strm >> vid;
	strm >> pagename;
	graph.add_vertex(vid, web_page(pagename));
	while (1) {
		vertex_id_type other_vid;
		strm >> other_vid;
		if (strm.fail()) break;
		graph.add_edge(vid, other_vid);
	}
	return true;
}

class pagerank_program : public ivertex_program<graph_type, double>, public IS_POD_TYPE {
	private:
		bool perform_scatter;
	public:
		edge_dir_type gather_edges(icontext_type& context, const vertex_type& vertex) const {
			return IN_EDGES;
		}

		double gather(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
			return edge.source().data().pagerank / edge.source().num_out_edges();
		}

		void apply(icontext_type& context, vertex_type& vertex, const gather_type& total) {
			double newval = total * 0.85 + 0.15;
			double prevval = vertex.data().pagerank;
			vertex.data().pagerank = newval;
			perform_scatter = (fabs(prevval - newval) > 1E-3);
		}

		edge_dir_type scatter_edges(icontext_type& context, const vertex_type& vertex) const {
			if (perform_scatter) return OUT_EDGES;
			else return NO_EDGES;
		}

		void scatter(icontext_type& context, const vertex_type& vertex, edge_type& edge) const {
			context.signal(edge.target());
		}
};

class graph_writer {
	public:
		string save_vertex(graph_type::vertex_type v) { 
			cout << v.data().pagename << "\t" << v.data().pagerank << "\n";	
			return ""; 
		}

		string save_edge(graph_type::edge_type e) { return ""; }
};


int main(int argc, char** argv) {
	graphlab::mpi_tools::init(argc, argv);
	cout << "here1" << endl; 
	graphlab::distributed_control dc;

	cout << "here2" << endl << flush;
	
	graph_type graph(dc);

	cout << "here3" << endl << flush;

	graph.load("graph.txt", line_parser);

	cout << "here4" << endl << flush;

	graph.finalize();
	
	omni_engine<pagerank_program> engine(dc, graph, "sync");
	engine.signal_all();
	engine.start();

	graph.save("output", graph_writer(), false, true, false);
	graphlab::mpi_tools::finalize();
}

