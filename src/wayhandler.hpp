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

typedef handler::NodeLocationsForWays<index_pos_type,
                                              index_neg_type>
        location_handler_type;

class WayHandler : public handler::Handler {

    DataStorage &ds;
    location_handler_type &location_handler;
    GeomOperate go;
    geom::OGRFactory<> ogr_factory;
    bool left = true;
    bool right = false;

    Road *get_road(Way &way, bool is_pedestrian) {
        OGRLineString *linestring = nullptr;
        try {
            linestring = ogr_factory.create_linestring(way,
                    geom::use_nodes::unique,
                    geom::direction::forward).release();
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

    void handle_pedestrian_road(Way &way) {
        Road *pedestrian_road;
        pedestrian_road = get_road(way, true);
    }

    void handle_vehicle_road(Way &way) {
        Road *vehicle_road;
        vehicle_road = get_road(way, false);
	iterate_over_nodes(way, vehicle_road);
    }
    
    bool has_same_location(object_id_type node1,
            object_id_type node2) {

        Location location1;
        Location location2;
        location1 = location_handler.get_node_location(node1);
        location2 = location_handler.get_node_location(node2);
        return ((location1.lon() == location2.lon()) &&
                (location1.lat() == location2.lat()));
    }


    void iterate_over_nodes(Way &way, Road *road) {
        object_id_type prev_node = 0;
        object_id_type current_node = 0;
        for (NodeRef node : way.nodes()) {
            current_node = node.ref();
            if (prev_node != 0) {
                if (!has_same_location(prev_node, current_node)) {
                    ds.insert_in_node_map(prev_node, node.ref(), road);
                }
            }
            prev_node = current_node;
        }
    }

    bool is_constructed(object_id_type node1,
            object_id_type node2) {

        object_id_type small_id = min(node1, node2);
        object_id_type big_id = max(node1, node2);
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

    void way(Way &way) {
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way)) {
                handle_vehicle_road(way);
            }
        }
    }

    void construct_sidewalks(object_id_type current,
            object_id_type neighbour) {

        Location current_location;
        Location neighbour_location;
        current_location = location_handler.get_node_location(current);
        neighbour_location = location_handler.get_node_location(neighbour);
        OGRGeometry *left_sidewalk = nullptr;
        OGRGeometry *right_sidewalk = nullptr;
        left_sidewalk = go.parallel_line(current_location, neighbour_location, 0.003, left);
        right_sidewalk = go.parallel_line(current_location, neighbour_location, 0.003, right);
        ds.insert_sidewalk(left_sidewalk);
        ds.insert_sidewalk(right_sidewalk);
    }

    void construct_convex_segment(object_id_type current,
            object_id_type prev_neighbour,
            object_id_type neighbour) {
        
        Location current_location;
        Location neighbour_location;
        Location prev_location;
        Location vertical_point1;
        Location vertical_point2;
        OGRGeometry *convex_segment;
        current_location = location_handler.get_node_location(current);
        neighbour_location = location_handler.get_node_location(neighbour);
        prev_location = location_handler.get_node_location(prev_neighbour);
        double angle;
        angle = go.angle(prev_location, current_location, neighbour_location);
        if (angle > 180) {
            vertical_point1 = go.vertical_point(current_location, prev_location, 0.003, right);
            vertical_point2 = go.vertical_point(current_location, neighbour_location, 0.003, left);
        } else {
            vertical_point1 = go.vertical_point(current_location, prev_location, 0.003, left);
            vertical_point2 = go.vertical_point(current_location, neighbour_location, 0.003, right);
        }
        convex_segment = go.connect_locations(vertical_point1, vertical_point2);
        ds.insert_sidewalk(convex_segment);
    }

    void generate_sidewalks() {
        object_id_type current;
        object_id_type neighbour;
        object_id_type prev_neighbour;

        for (auto node : ds.node_map) {
            current = node.first;
            int count_neighbours = node.second.size();
            for (int i = 0; i < (count_neighbours); i++) {
                neighbour = node.second[i].first;
                if (!is_constructed(current, neighbour)) {
                    construct_sidewalks(current, neighbour);
                } else {
                if ((i > 0) && (i < (count_neighbours))) {
                    prev_neighbour = node.second[i-1].first; 
                    construct_convex_segment(current, prev_neighbour, neighbour); 
                }
            }
        }
    }

            


//	    if (number_of_neighbours == 1) {
//                next_id = node.second[0].first;
//                next_location = location_handler.get_node_location(next_id);
//                sidewalk_left = go.vertical_point(current_location, next_location, 0.003, true);
//                sidewalk_right = go.vertical_point(current_location, next_location, 0.003, false);
//                ds.insert_node(sidewalk_left, current_id);
//                ds.insert_node(sidewalk_right, current_id);
//            }
//	    if (number_of_neighbours == 2) {
//		//for (auto neighbour_node : no.second) {
//		prev_id = node.second[0].first;
//		next_id = node.second[1].first;
//                prev_location = location_handler.get_node_location(prev_id);
//                next_location = location_handler.get_node_location(next_id);
//                Location sidewalk_left_1;
//                Location sidewalk_left_2;
//                Location sidewalk_right_1;
//                Location sidewalk_right_2;
//                sidewalk_left_1 = go.vertical_point(current_location, prev_location, 0.003, true);
//                sidewalk_left_2 = go.vertical_point(current_location, next_location, 0.003, false);
//                sidewalk_right_1 = go.vertical_point(current_location, prev_location, 0.003, false);
//                sidewalk_right_2 = go.vertical_point(current_location, next_location, 0.003, true);
//                double d_prev = go.haversine(prev_location, sidewalk_left_1);
//                double d_next = go.haversine(prev_location, sidewalk_left_2);
//                if (d_prev > d_next) {  // winkel < 180°
//                    sidewalk_left = go.mean(sidewalk_left_1, sidewalk_left_2);
//                    ds.insert_node(sidewalk_left, current_id);
//                    ds.insert_node(sidewalk_right_1, current_id);
//                    ds.insert_node(sidewalk_right_2, current_id);
//                } else {
//                    sidewalk_right = go.mean(sidewalk_right_1, sidewalk_right_2);
//                    ds.insert_node(sidewalk_left_1, current_id);
//                    ds.insert_node(sidewalk_left_2, current_id);
//                    ds.insert_node(sidewalk_right, current_id);
//                }
//            }
//	    if (number_of_neighbours > 2) {
//                for (int i = 0; i < (number_of_neighbours); i++) {
//		    prev_id = node.second[i].first;
//		    next_id = node.second[(i + 1) % number_of_neighbours].first;
//                    prev_location = location_handler.get_node_location(prev_id);
//                    next_location = location_handler.get_node_location(next_id);
//                    Location sidewalk_left_1;
//                    Location sidewalk_left_2;
//                    sidewalk_left_1 = go.vertical_point(current_location, prev_location, 0.003, true);
//                    sidewalk_left_2 = go.vertical_point(current_location, next_location, 0.003, false);
//                    double d_prev = go.haversine(prev_location, sidewalk_left_1);
//                    double d_next = go.haversine(prev_location, sidewalk_left_2);
//                    if (d_prev > d_next) {  // winkel < 180°
//                        sidewalk_left = go.mean(sidewalk_left_1, sidewalk_left_2);
//                        ds.insert_node(sidewalk_left, current_id);
//                    } else {
//                        ds.insert_node(sidewalk_left_1, current_id);
//                        ds.insert_node(sidewalk_left_2, current_id);
//                    }
//                }
//	    }
//	}
    }

};
                /*if (is_connection_finished(current_id, next_id)) {
                    continue;
                }*/

#endif /* WAYHANDLER_HPP_ */
