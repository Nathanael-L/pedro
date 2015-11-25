#include <iostream>
#include <getopt.h>
#include <iterator>
#include <vector>

#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/visitor.hpp>
#include <osmium/geom/factory.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/tags/filter.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/relations/collector.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/geom/ogr.hpp>
#include <osmium/geom/geos.hpp>
#include <osmium/geom/wkt.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <geos/geom/GeometryCollection.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/index/strtree/STRtree.h>
#include <geos/io/WKBWriter.h>
#include <geos/io/WKBReader.h>
#include <geos/io/WKTReader.h>
#include <google/sparse_hash_set>
#include <google/sparse_hash_map>

using namespace std;
using namespace osmium;

typedef index::map::Dummy<unsigned_object_id_type,
        Location> index_neg_type;
typedef index::map::SparseMemArray<unsigned_object_id_type,
        Location> index_pos_type;
typedef handler::NodeLocationsForWays<index_pos_type, index_neg_type>
        location_handler_type;
typedef geos::geom::LineString linestring_type;

#include "geometricoperations.hpp"
#include "tagcheck.hpp"
#include "road.hpp"
#include "datastorage.hpp"
#include "wayhandler.hpp"


void print_help() {
    cout << "osmi [OPTIONS] INFILE OUTFILE\n\n"
         << "  -h, --help           This help message\n"
         //<< "  -d, --debug          Enable debug output !NOT IN USE\n"
         << endl;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = { { "help", no_argument, 0, 'h' }, {
            "debug", no_argument, 0, 'd' }, { 0, 0, 0, 0 } };

    bool debug = false;

    while (true) {
        int c = getopt_long(argc, argv, "hd:", long_options, 0);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            print_help();
            exit(0);
        case 'd':
            debug = true;
            break;
        default:
            exit(1);
        }
    }

    string input_filename;
    string output_filename;
    int remaining_args = argc - optind;
    if ((remaining_args < 2) || (remaining_args > 4)) {
        cerr << "Usage: " << argv[0] << " [OPTIONS] INFILE OUTFILE" << endl;
        cerr << remaining_args;
        exit(1);
    } else if (remaining_args == 2) {
        input_filename = argv[optind];
        output_filename = argv[optind + 1];
        cout << "in: " << input_filename << " out: " << output_filename << endl;
    } else {
        input_filename = "-";
    }

    index_pos_type index_pos;
    index_neg_type index_neg;
    location_handler_type location_handler(index_pos, index_neg);
    location_handler.ignore_errors();
    DataStorage ds(output_filename, location_handler);
    
    cerr << "start reading osm ..." << endl;
    io::Reader reader(input_filename);
    WayHandler way_handler(ds, location_handler);
    apply(reader, location_handler, way_handler);
    reader.close();
    cerr << "!" << endl;

    /*
    for (auto node : ds.node_map) {
	cout << node.first << ": " << endl;
	for (auto link : node.second) {
	    cout << "  " << link.first << " - " << link.second->name << endl;
	}
    }
    */

    cerr << "insert osm footways in postgres ...";
    //ds.insert_ways();
    cerr << "!" << endl;
    cerr << "generate sidewalks ...";
    way_handler.generate_sidewalks();

    cerr << "!" << endl;
    ds.union_sidewalk_geometries();
    cerr << "node_map size: " << ds.node_map.size() << endl;
    cerr << "finished_connections size: " << ds.finished_connections.size() << endl;
    //ds.insert_vehicle();
    GeomOperate go;
    OGRGeometry *ogr_sidewalk_net = nullptr;
    ogr_sidewalk_net = go.geos2ogr(ds.geos_sidewalk_net);
    ds.insert_sidewalk(ogr_sidewalk_net);

    cerr << "ready" << endl;

    /*** TEST GEOM OPERATOR ***
    GeomOperate go;
    Location A, B, C;
    for (double i=4; i<9; ++i) {
        A.set_lon(i * 11 + 0.007 * i); A.set_lat(9.0 + 0.008 * (i-1));
        B.set_lon(i * 11 + 0.01 * (i - 2)); B.set_lat(9.0 + 0.0067 * i);
        C = go.vertical_point(B.lon(), B.lat(), A.lon(), A.lat(), 1);
        cout << "LINESTRING (" << A.lat() << " " << A.lon() << ", " << B.lat()
                << " " << B.lon() << ")" << endl;
        cout << "LINESTRING (" << B.lat() << " " << B.lon() << ", " << C.lat()
                << " " << C.lon() << ")" << endl;
    }
    ***/
}
