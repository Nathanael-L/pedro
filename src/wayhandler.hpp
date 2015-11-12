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

    Road *get_road(osmium::Way& way, bool is_pedestrian) {
        OGRLineString *linestring = nullptr;
        try {
            linestring = ogr_factory.create_linestring(way,
                    osmium::geom::use_nodes::unique,
                    osmium::geom::direction::forward).release();
        } catch (...) {
            cerr << "Error at way: " << way.id() << endl;
            cerr << "  Unexpected error" << endl;
            return nullptr;
        }

        string name = TagCheck::get_name(way);
        string type = TagCheck::get_highway_type(way);
        double length = linestring->get_Length();
        string osm_id = to_string(way.id());
        Road *road;
        if (is_pedestrian) {
            try {
                road = ds.store_pedestrian_road(name, linestring, type, length,
                        osm_id);
            } catch (...) {
                cerr << "Inserting to set failed for way: "
                << way.id() << endl;
            }
        } else {
            char sidewalk = TagCheck::get_sidewalk(way);
            int lanes = TagCheck::get_lanes(way);
            try {
                road = ds.store_vehicle_road(name, linestring, sidewalk, type,
                        lanes, length, osm_id);
            } catch (...) {
                cerr << "Inserting to set failed for way: "
                << way.id() << endl;
            }
        }
        return road;
        //delete linestring;
    }

    void handle_pedestrian_road(osmium::Way& way) {
        Road *pedestrian_road;
        pedestrian_road = get_road(way, true);
    }

    void handle_vehicle_road(osmium::Way& way) {
        Road *vehicle_road;
        vehicle_road = get_road(way, false);
    }
    
    void iterate_over_nodes(osmium::Way& way) {
        cout << "B" << endl;
        osmium::object_id_type prev_node = 0;
        for (osmium::NodeRef node : way.nodes()) {
            if (prev_node != 0) {
                cout << prev_node << endl;
                cout << node.ref() << endl;
            }
            prev_node = node.ref();
        }
        cout << "B" << endl;
    }

    
public:

    explicit WayHandler(DataStorage &data_storage,
            location_handler_type &location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    void way(osmium::Way& way) {
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                //handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way)) {
                //handle_vehicle_road(way);
        iterate_over_nodes(way);
            }

        }
    }
};

#endif /* WAYHANDLER_HPP_ */
