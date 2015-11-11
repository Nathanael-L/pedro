/***
 *
 *
 *
 *
 */

#ifndef WAYHANDLER_HPP_
#define WAYHANDLER_HPP_

#include <iostream>
#include <osmium/geom/geos.hpp>
#include <geos/geom/MultiLineString.h>
#include <geos/geom/Point.h>
#include <osmium/handler.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>

typedef osmium::handler::NodeLocationsForWays<index_pos_type,
                                              index_neg_type>
        location_handler_type;

class WayHandler : public osmium::handler::Handler {

    DataStorage &ds;
    location_handler_type &location_handler;
    osmium::geom::OGRFactory<> ogr_factory;

    void handle_pedestrian_road(osmium::Way& way) {
        OGRLineString *linestring = nullptr;

        try {
            linestring = ogr_factory.create_linestring(way,
                    osmium::geom::use_nodes::unique,
                    osmium::geom::direction::forward).release();
        } catch (...) {
            cerr << "Error at way: " << way.id() << endl;
            cerr << "  Unexpected error" << endl;
            return;
        }

        string name = TagCheck::get_name(way);
        string type = TagCheck::get_highway_type(way);
        double length = linestring->get_Length();
        string osm_id = to_string(way.id());

        try {
            ds.store_pedestrian_road(name, linestring, type, length, osm_id);
        } catch (...) {
            cerr << "Inserting to set failed for way: "
            << way.id() << endl;
        }
        //delete linestring;
    }
//(string name, OGRGeometry *geometry, char sidewalk,
//            string type, int lanes, double length, string osm_id)
    void handle_vehicle_road(osmium::Way& way) {
        OGRLineString *linestring = nullptr;

        try {
            linestring = ogr_factory.create_linestring(way,
                    osmium::geom::use_nodes::unique,
                    osmium::geom::direction::forward).release();
        } catch (...) {
            cerr << "Error at way: " << way.id() << endl;
            cerr << "  Unexpected error" << endl;
            return;
        }

        string name = TagCheck::get_name(way);
        char sidewalk = TagCheck::get_sidewalk(way);
        string type = TagCheck::get_highway_type(way);
        int lanes = TagCheck::get_lanes(way);
        double length = linestring->get_Length();
        string osm_id = to_string(way.id());

        try {
            ds.store_vehicle_road(name, linestring, sidewalk, type, lanes,
                    length, osm_id);
        } catch (...) {
            cerr << "Inserting to set failed for way: "
            << way.id() << endl;
        }
        //delete linestring;
    }

    
public:

    explicit WayHandler(DataStorage &data_storage,
            location_handler_type &location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    void way(osmium::Way& way) {
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way)) {
                handle_vehicle_road(way);
            }

        }
    }
};

#endif /* WAYHANDLER_HPP_ */
