/***
 * main.cpp
 *
 *  Created on: Nov 09, 2015
 *      Author: nathanael
 *
 *  pedro - Pedestrian Routing on OSM
 *  pedro follows a different way of creating a routing graph out of
 *  OpenStreetMap data. The main concept is to generate sidewalks and to
 *  consider human behaviour of crossing roads.
 *  
 *  The software is available under BSD License
 *  (http://www.linfo.org/bsdlicense.html)
 *  
 */

#include <iostream>
#include <getopt.h>
#include <iterator>
#include <vector>

#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/visitor.hpp>
#include <osmium/geom/factory.hpp>
#include <osmium/osm/location.hpp>
//#include <osmium/osm/relation.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/tags/filter.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
//#include <osmium/relations/collector.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/geom/ogr.hpp>
#include <osmium/geom/geos.hpp>
//#include <osmium/geom/wkt.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateFilter.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/CoordinateArraySequenceFactory.h>
#include <geos/geom/GeometryCollection.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineSegment.h>
#include <geos/index/strtree/STRtree.h>
#include <geos/io/WKBWriter.h>
#include <geos/io/WKBReader.h>
#include <geos/io/WKTReader.h>
#include <google/sparse_hash_set>
#include <google/sparse_hash_map>

using namespace std;
using namespace osmium;
using namespace geos::geom;
using namespace geos::index::strtree;

typedef index::map::Dummy<unsigned_object_id_type,
        Location> index_neg_type;
typedef index::map::SparseMemArray<unsigned_object_id_type,
        Location> index_pos_type;
typedef handler::NodeLocationsForWays<index_pos_type, index_neg_type>
        location_handler_type;

#include "geom_operate.hpp"
#include "tag_check.hpp"
#include "road.hpp"
#include "pedro_point.hpp"
#include "data_storage.hpp"
#include "contrast.hpp"
#include "prepare_handler.hpp"
#include "way_handler.hpp"
#include "sidewalk_factory.hpp"
#include "crossing_factory.hpp"
#include "geometry_constructor.hpp"


void print_help() {
    cout << "osmi INFILE [OUTFILE]\n\n"
         << "  INFILE        proper OSM-File (osm or pbf)\n"
         << "  OUTFILE       name of shapefile directory - must be empty\n"
         << "  -p            OUTFILE is name of postgis database\n"
         << "                - not default for performance reasons\n"
         << "                - it is recomanded to use shp2pgsql instead\n"
         << "  -h, --help           This help message\n"
         //<< "  -d, --debug          Enable debug output !NOT IN USE\n"
         << endl;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = { { "help", no_argument, 0, 'h' }, {
            "psql", no_argument, 0, 'p' }, {"debug", no_argument, 0, 'd' }, {
            0, 0, 0, 0 } };

    bool debug = false;
    bool psql = false;

    while (true) {
        int c = getopt_long(argc, argv, "dhp:", long_options, 0);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'h':
            print_help();
            exit(0);
        case 'p':
            psql = true;
            break;
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
    if (remaining_args == 2) {
        input_filename = argv[optind];
        output_filename = argv[optind + 1];
        cout << "in: " << input_filename << " out: " << output_filename << endl;
    } else if (remaining_args == 1) {
        input_filename = argv[optind];
        output_filename = "output";
        cout << "in: " << input_filename << " out: " << output_filename << endl;
    } else {
        print_help();
        exit(1);
    }

    index_pos_type index_pos;
    index_neg_type index_neg;
    location_handler_type location_handler(index_pos, index_neg);
    location_handler.ignore_errors();
    DataStorage ds(output_filename, location_handler, psql);
    GeomOperate go;
    GeometryConstructor geometry_constructor(ds, location_handler);
    CrossingFactory crossing_factory(ds, location_handler);
    
    if (debug) cerr << "start reading osm once ..." << endl;
    io::Reader reader1(input_filename);
    PrepareHandler prepare_handler(ds, location_handler);
    apply(reader1, location_handler, prepare_handler);

    if (debug) cerr << "insert osm footways ..." << endl;
    prepare_handler.create_pedestrian_node_map();
    reader1.close();

    if (debug) cerr << "start reading osm twice ..." << endl;
    io::Reader reader2(input_filename);
    WayHandler way_handler(ds, location_handler);
    apply(reader2, location_handler, way_handler);
    reader2.close();

    if (debug) cerr << "generate sidewalks and osm crossings ..."
        << endl;
    geometry_constructor.generate_sidewalks();

    if (debug) cerr << "calculate contrast ..." << endl;
    Contrast contrast = Contrast(ds);
    contrast.check_sidewalks();

    if (debug) cerr << "generate frequent crossing ..." << endl;
    crossing_factory.generate_frequent_crossings();
    
    if (debug) cerr << "fill in sidewalk and crossing tree ..." << endl;
    ds.fill_sidewalk_tree();
    ds.fill_crossing_tree();
    
    if (debug) cerr << "connect sidewalks and pedestrian ..." << endl;
    geometry_constructor.connect_sidewalks_and_pedesrians();

    if (debug) cerr << "vehicle_vehicle_node_map size: " << ds.vehicle_node_map.size() << endl;
    if (debug) cerr << "croosing_node_map size: " << ds.crossing_node_map.size() << endl;
    if (debug) cerr << "crossing_set size: " << ds.crossing_set.size() << endl;

    if (debug) cerr << "insert ways ..." << endl;
    ds.insert_ways();
    //ds.insert_vehicle();
    ds.insert_sidewalks();
    ds.insert_crossings();

    if (debug) cerr << "clean up ..." << endl;
    ds.clean_up();

    cerr << "ready!" << endl;

    /*** TEST GEOM OPERATOR ***
    GeomOperate go;
    Location A, B, C;
    A.set_lon(10); A.set_lat(20);
    B.set_lon(18); B.set_lat(10);
    C.set_lon(30); C.set_lat(0);
    cout << "x = " << go.angle(A,B,C);
    ***/
}
