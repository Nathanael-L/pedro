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
    GeomOperate go;
    osmium::geom::OGRFactory<> ogr_factory;

    Road *get_road(osmium::Way &way, bool is_pedestrian) {
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

    void handle_pedestrian_road(osmium::Way &way) {
        Road *pedestrian_road;
        pedestrian_road = get_road(way, true);
    }

    void handle_vehicle_road(osmium::Way &way) {
        Road *vehicle_road;
        vehicle_road = get_road(way, false);
	iterate_over_nodes(way, vehicle_road);
    }
    
    bool has_same_location(osmium::object_id_type node1,
            osmium::object_id_type node2) {

        osmium::Location location1;
        osmium::Location location2;
        location1 = location_handler.get_node_location(node1);
        location2 = location_handler.get_node_location(node2);
        return ((location1.lon() == location2.lon()) &&
                (location1.lat() == location2.lat()));
    }


    void iterate_over_nodes(osmium::Way &way, Road *road) {
        osmium::object_id_type prev_node = 0;
        osmium::object_id_type current_node = 0;
        for (osmium::NodeRef node : way.nodes()) {
            current_node = node.ref();
            if (prev_node != 0) {
                if (!has_same_location(prev_node, current_node)) {
                    ds.insert_in_node_map(prev_node, node.ref(), road);
                }
            }
            prev_node = current_node;
        }
    }

    bool is_connection_finished(osmium::object_id_type node1,
            osmium::object_id_type node2) {

        osmium::object_id_type small_id = min(node1, node2);
        osmium::object_id_type big_id = max(node1, node2);
        string connection_string;
        connection_string = to_string(small_id) + to_string(big_id);
        if (ds.finished_connections.find(connection_string)
	        == ds.finished_connections.end()) {
	    ds.finished_connections.insert(connection_string);
            return false;
        }
        return true;
    }
    
public:

    explicit WayHandler(DataStorage &data_storage,
            location_handler_type &location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    void way(osmium::Way &way) {
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way)) {
                handle_vehicle_road(way);
            }
        }
    }

    void generate_sidewalks() {
        osmium::object_id_type current_id;
        osmium::object_id_type prev_id;
        osmium::object_id_type next_id;
        osmium::Location current_location;
        osmium::Location prev_location;
        osmium::Location next_location;
        osmium::Location sidewalk_left;
        osmium::Location sidewalk_right;

	for (auto node : ds.node_map) {
            current_id = node.first;
            current_location = location_handler.get_node_location(current_id);
            int number_of_neighbours = node.second.size();
	    if (number_of_neighbours == 1) {
                next_id = node.second[0].first;
                next_location = location_handler.get_node_location(next_id);
                sidewalk_left = go.vertical_point(current_location, next_location, 0.003, true);
                sidewalk_right = go.vertical_point(current_location, next_location, 0.003, false);
                ds.insert_node(sidewalk_left, current_id);
                ds.insert_node(sidewalk_right, current_id);
            }
	    if (number_of_neighbours == 2) {
		//for (auto neighbour_node : no.second) {
		prev_id = node.second[0].first;
		next_id = node.second[1].first;
                prev_location = location_handler.get_node_location(prev_id);
                next_location = location_handler.get_node_location(next_id);
                osmium::Location sidewalk_left_1;
                osmium::Location sidewalk_left_2;
                osmium::Location sidewalk_right_1;
                osmium::Location sidewalk_right_2;
                sidewalk_left_1 = go.vertical_point(current_location, prev_location, 0.003, true);
                sidewalk_left_2 = go.vertical_point(current_location, next_location, 0.003, false);
                sidewalk_right_1 = go.vertical_point(current_location, prev_location, 0.003, false);
                sidewalk_right_2 = go.vertical_point(current_location, next_location, 0.003, true);
                double d_prev = go.haversine(prev_location, sidewalk_left_1);
                double d_next = go.haversine(prev_location, sidewalk_left_2);
                if (d_prev > d_next) {  // winkel < 180°
                    sidewalk_left = go.mean(sidewalk_left_1, sidewalk_left_2);
                    ds.insert_node(sidewalk_left, current_id);
                    ds.insert_node(sidewalk_right_1, current_id);
                    ds.insert_node(sidewalk_right_2, current_id);
                } else {
                    sidewalk_right = go.mean(sidewalk_right_1, sidewalk_right_2);
                    ds.insert_node(sidewalk_left_1, current_id);
                    ds.insert_node(sidewalk_left_2, current_id);
                    ds.insert_node(sidewalk_right, current_id);
                }
            }
	    if (number_of_neighbours > 2) {
                for (int i = 0; i < (number_of_neighbours); i++) {
		    prev_id = node.second[i].first;
		    next_id = node.second[(i + 1) % number_of_neighbours].first;
                    prev_location = location_handler.get_node_location(prev_id);
                    next_location = location_handler.get_node_location(next_id);
                    osmium::Location sidewalk_left_1;
                    osmium::Location sidewalk_left_2;
                    sidewalk_left_1 = go.vertical_point(current_location, prev_location, 0.003, true);
                    sidewalk_left_2 = go.vertical_point(current_location, next_location, 0.003, false);
                    double d_prev = go.haversine(prev_location, sidewalk_left_1);
                    double d_next = go.haversine(prev_location, sidewalk_left_2);
                    if (d_prev > d_next) {  // winkel < 180°
                        sidewalk_left = go.mean(sidewalk_left_1, sidewalk_left_2);
                        ds.insert_node(sidewalk_left, current_id);
                    } else {
                        ds.insert_node(sidewalk_left_1, current_id);
                        ds.insert_node(sidewalk_left_2, current_id);
                    }
                }
	    }
	}
    }

};
                /*if (is_connection_finished(current_id, next_id)) {
                    continue;
                }*/

#endif /* WAYHANDLER_HPP_ */
