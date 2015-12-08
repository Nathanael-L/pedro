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


class WayHandler : public handler::Handler {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    geom::OGRFactory<> ogr_factory;
    geom::GEOSFactory<> geos_factory;
    const bool left = true;
    const bool right = false;
    bool is_first_way = true;

    const bool id_in_list(object_id_type osm_id,
            vector<object_id_type> search_list) {
        for (auto item : search_list) {
            if (item == osm_id) {
                return true;
            }
        }
        return false;
    }
    
    void handle_pedestrian_road(Way& way) {
        object_id_type way_id = way.id();
        auto map_entry = ds.pedestrian_node_map.find(way_id);
        if (map_entry != ds.pedestrian_node_map.end()) {
            auto first_node = way.nodes().begin();
            auto last_node = way.nodes().begin() + 1;
            for (auto node = last_node; (node < way.nodes().end()); node++) {
                object_id_type node_id = node->ref();
                if ((id_in_list(node_id, map_entry->second)) || 
                        (last_node == way.nodes().end() - 1)) {
                    geos_factory.linestring_start();
                    size_t num_points = geos_factory.fill_linestring(
                            first_node, last_node + 1);
                    LineString* linestring = geos_factory.linestring_finish(
                            num_points).release();
                    first_node = node;
                    PedestrianRoad* pedestrian_road = new PedestrianRoad(way, linestring);
                    ds.pedestrian_road_set.insert(pedestrian_road);
                    ds.pedestrian_geometries.push_back(pedestrian_road->geometry);
                }
                last_node++;
            }
        } else {
            PedestrianRoad* pedestrian_road = new PedestrianRoad(way);
            ds.pedestrian_road_set.insert(pedestrian_road);
            ds.pedestrian_geometries.push_back(pedestrian_road->geometry);
        }
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
                    ds.insert_in_vehicle_node_map(prev_node, current_node, road);
                }
            }
            prev_node = current_node;
        }
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
};

#endif /* WAYHANDLER_HPP_ */
