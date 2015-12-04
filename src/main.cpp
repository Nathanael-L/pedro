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
using geos::geom::Coordinate;
using geos::geom::Geometry;
using geos::geom::LineString;
using geos::geom::Point;
using geos::geom::GeometryFactory;
using geos::geom::CoordinateSequence;


typedef index::map::Dummy<unsigned_object_id_type,
        Location> index_neg_type;
typedef index::map::SparseMemArray<unsigned_object_id_type,
        Location> index_pos_type;
typedef handler::NodeLocationsForWays<index_pos_type, index_neg_type>
        location_handler_type;

#include "geometric_operations.hpp"
#include "tag_check.hpp"
#include "road.hpp"
#include "data_storage.hpp"
#include "way_handler.hpp"
#include "sidewalk_factory.hpp"


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
    GeomOperate go;
    SidewalkFactory sf(ds, location_handler);
    
    cerr << "start reading osm ..." << endl;
    io::Reader reader(input_filename);
    WayHandler way_handler(ds, location_handler);
    apply(reader, location_handler, way_handler);

    cerr << "insert osm footways in postgres ...";
    ds.insert_ways();
    cerr << "generate sidewalks ...";
    sf.generate_sidewalks();

    //ds.union_vehicle_geometries();
    //ds.union_pedestrian_geometries();
    cerr << "node_map size: " << ds.node_map.size() << endl;
    ds.insert_vehicle();
    ds.insert_sidewalks();

    reader.close();
    cerr << "ready" << endl;

    /*** TEST GEOM OPERATOR ***
    GeomOperate go;
    Location A, B, C;
    A.set_lon(10); A.set_lat(20);
    B.set_lon(18); B.set_lat(10);
    C.set_lon(30); C.set_lat(0);
    cout << "x = " << go.angle(A,B,C);
    ***/
}
