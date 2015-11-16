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
#include <google/sparse_hash_set>
#include <google/sparse_hash_map>

using namespace std;

typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type,
        osmium::Location> index_neg_type;
typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
        osmium::Location> index_pos_type;
typedef osmium::handler::NodeLocationsForWays<index_pos_type, index_neg_type>
        location_handler_type;
typedef geos::geom::LineString linestring_type;

#include "geometricoperations.hpp"
#include "errorsum.hpp"
#include "tagcheck.hpp"
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

    DataStorage ds(output_filename);
    index_pos_type index_pos;
    index_neg_type index_neg;
    location_handler_type location_handler(index_pos, index_neg);
    location_handler.ignore_errors();
    
    osmium::io::Reader reader(input_filename);
    WayHandler way_handler(ds, location_handler);
    osmium::apply(reader, location_handler, way_handler);
    reader.close();

    /*
    for (auto node : ds.node_map) {
	cout << node.first << ": " << endl;
	for (auto link : node.second) {
	    cout << "  " << link.first << " - " << link.second->name << endl;
	}
    }
    */

    ds.insert_ways();
    way_handler.generate_sidewalks();
    cout << "node_map size: " << ds.node_map.size() << endl;
    cout << "finished_connections size: " << ds.finished_connections.size() << endl;
    ds.insert_vhcl();

    cout << "ready" << endl;

    /*** TEST GEOM OPERATOR ***
    GeomOperate geom_operate;
    osmium::Location A, B;
    A.set_lon(68.2); A.set_lat(9.18);
    B.set_lon(68.0); B.set_lat(9.2);
    osmium::Location delta = geom_operate.inverse_haversine(A.lon(), A.lat(), 20);
    //cout << "dlon: " << delta.lon << " dlat: " << delta.lat << endl;
    osmium::Location new_point;
    new_point = geom_operate.vertical_point(B.lon(), B.lat(), A.lon(), A.lat(), 20);
    double distance;
    distance = geom_operate.haversine(A.lon(), A.lat(), new_point.lon(), new_point.lat());
    cout << "LINESTRING (" << A.lat() << " " << A.lon() << ", " << B.lat() << " " << B.lon() << ")" << endl;
    cout << "LINESTRING (" << B.lat() << " " << B.lon() << ", " << new_point.lat() << " " << new_point.lon() << ")" << endl;
    cout << "D: " << distance << endl;
    ***/
}
