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

#include "errorsum.hpp"
#include "tagcheck.hpp"
#include "datastorage.hpp"
#include "wayhandler.hpp"
#include "geometricoperations.hpp"


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
    
    /*
    osmium::io::Reader reader(input_filename);
    WayHandler way_handler(ds, location_handler);
    osmium::apply(reader, location_handler, way_handler);
    reader.close();
    ds.insert_ways();
    ds.insert_vhcl();

    cout << "ready" << endl;
    */

    Coordinate delta = GeomOperate::inverse_haversine(48.1, 9.18, 20);
    cout << "start: " << "48.1, 9.18" << endl;
    cout << "dlon: " << delta.lon << " dlat: " << delta.lat << endl;
    Coordinate new_point;
    new_point = GeomOperate::vertical_point(48.1, 9.18, 48.4, 9.23, 20);
    double distance;
    distance = GeomOperate::haversine(48.1, 9.18, new_point.lon, new_point.lat);
    cout << "dlon: " << new_point.lon << " dlat: " << new_point.lat << endl;
    cout << "D: " << distance << endl;
    
}
