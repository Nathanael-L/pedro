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

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    geom::OGRFactory<> ogr_factory;
    const bool left = true;
    const bool right = false;
    bool is_first_way = true;

    void handle_pedestrian_road(Way& way) {
        PedestrianRoad *pedestrian_road = new PedestrianRoad(way);
        ds.pedestrian_road_set.insert(pedestrian_road);
        ds.pedestrian_geometries.push_back(pedestrian_road->geometry);
    }

    void handle_vehicle_road(Way& way) {
        VehicleRoad *vehicle_road = new VehicleRoad(way);
        ds.vehicle_road_set.insert(vehicle_road);
        ds.vehicle_geometries.push_back(vehicle_road->geometry);
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

    void iterate_over_nodes(Way& way, VehicleRoad* road) {
        object_id_type prev_node = 0;
        object_id_type current_node = 0;
        for (NodeRef node : way.nodes()) {
            current_node = node.ref();
            if (prev_node != 0) {
                if (!has_same_location(prev_node, current_node)) {
                    ds.insert_in_node_map(prev_node, current_node, road);
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

    explicit WayHandler(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    void way(Way& way) {
        if (is_first_way) {
            cerr << "... handle ways ..." << endl;
        }
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way)) {
                handle_vehicle_road(way);
            }
        }
        is_first_way = false;
    }
//
//    void construct_sidewalks(object_id_type current,
//            object_id_type neighbour) {
//
//        Location current_location;
//        Location neighbour_location;
//        current_location = location_handler.get_node_location(current);
//        neighbour_location = location_handler.get_node_location(neighbour);
//        geos::geom::Geometry *left_sidewalk = nullptr;
//
//        left_sidewalk = go.parallel_line(current_location, neighbour_location,
//                0.003, true, right);
//        //right_sidewalk = go.parallel_line(current_location, neighbour_location,
//        //        0.003, left);
//        //ds.insert_sidewalk(left_sidewalk);
//        //ds.insert_sidewalk(right_sidewalk);
//        ds.sidewalk_geometries.push_back(left_sidewalk);
//    }
//
//
///* NOTE:
// * mit den richtigen geometrien arbeiten nicht mit osm ids. gibt performance und ich kann die geometrien verändern...
// *
// */
//
//    void construct_convex_segment(object_id_type current,
//            object_id_type prev_neighbour,
//            object_id_type neighbour) {
//        
//        Location current_location;
//        Location next_location;
//        Location prev_location;
//        Location vertical_point1;
//        Location vertical_point2;
//        geos::geom::Geometry *convex_segment;
//        current_location = location_handler.get_node_location(current);
//        next_location = location_handler.get_node_location(neighbour);
//        prev_location = location_handler.get_node_location(prev_neighbour);
//        double angle;
//        angle = go.angle(prev_location, current_location, next_location);
//        if (angle > 180) {
//            vertical_point1 = go.vertical_point(current_location, prev_location,
//                    0.003, false, left);
//            vertical_point2 = go.vertical_point(current_location,
//                    next_location, 0.003, false, right);
//        } else {
//            vertical_point1 = go.vertical_point(current_location, prev_location,
//                    0.003, false, right);
//            vertical_point2 = go.vertical_point(current_location, next_location,
//                    0.003, false, left);
//        }
//            convex_segment = go.connect_locations(vertical_point1,
//                    vertical_point2);
//            ds.sidewalk_geometries.push_back(convex_segment);
//            //ds.insert_sidewalk(convex_segment);
//            string ori = to_string(static_cast<int>(ceil(go.orientation(current_location, prev_location)))) + to_string(static_cast<int>(ceil(go.orientation(current_location, next_location))));
//            //ds.insert_node(current_location, current, ori.c_str(), angle);
//        
//        
//        
//        /**/
//    }
//
//    void generate_sidewalks() {
//        object_id_type current;
//        object_id_type neighbour;
//        object_id_type prev_neighbour;
//
//        for (auto node : ds.node_map) {
//            if (node.first == 1763864221) {
//
//
//            }
//            current = node.first;
//            int count_neighbours = node.second.size();
//            for (int i = 0; i < (count_neighbours); ++i) {
//                neighbour = node.second[i].first;
//                int prev_index = (i - 1 + count_neighbours) % count_neighbours;
//                prev_neighbour = node.second[prev_index].first; 
//                
//                //if (!is_constructed(current, neighbour)) {
//                    construct_sidewalks(current, neighbour);
//                //}
//                construct_convex_segment(current, prev_neighbour, neighbour); 
//            }
//        }
//    }
};

#endif /* WAYHANDLER_HPP_ */
