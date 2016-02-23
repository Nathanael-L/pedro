/***
 * way_handler.hpp
 *
 *  Created on: Nov 9, 2015
 *      Author: nathanael
 */

#ifndef WAY_HANDLER_HPP_
#define WAY_HANDLER_HPP_

#include <iostream>
#include <osmium/geom/geos.hpp>
#include <geos/geom/MultiLineString.h>
#include <geos/geom/Point.h>
#include <osmium/handler.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>


class WayHandler : public handler::Handler {

    geom::OGRFactory<> ogr_factory;
    geom::GEOSFactory<> geos_factory;
    location_handler_type& location_handler;
    DataStorage& ds;
    GeomOperate go;
    Contrast contrast = Contrast(ds);
    const bool left = true;
    const bool right = false;

    const bool id_in_list(object_id_type osm_id,
            vector<object_id_type> search_list) {
        for (auto item : search_list) {
            if (item == osm_id) {
                return true;
            }
        }
        return false;
    }

    /***
     * PedestrianRoad are created for each way segment between crossings.
     * Whether a node is a crossing is looked up in the pedestrian_node_map.
     * The PedestrianRoad is stored to pedestrian_road_set.
     * The orthogonals for they contrast calculations are also created now.
     * TODO: some logical problems: e.g. at lindenmuseum crossing.
     */
    void handle_pedestrian_road(Way& way) {
        object_id_type way_id = way.id();
        auto map_entry = ds.pedestrian_node_map.find(way_id);
        if (map_entry != ds.pedestrian_node_map.end()) {
            size_t num_points;
            auto first_node = way.nodes().begin();
            auto last_node = way.nodes().begin() + 1;
            for (auto current_node = last_node;
                    (current_node < way.nodes().end()); current_node++) {
                
                object_id_type current_id = current_node->ref();
                if ((id_in_list(current_id, map_entry->second)) || 
                        (current_node == way.nodes().end() - 1)) {
                    geos_factory.linestring_start();
                    num_points = geos_factory.fill_linestring(first_node, last_node + 1);
                    LineString* linestring = geos_factory.linestring_finish(
                            num_points).release();
                    first_node = current_node;
                    PedestrianRoad* pedestrian_road = new PedestrianRoad(0, way, linestring);
                    ds.pedestrian_road_set.insert(pedestrian_road);
                    contrast.create_orthogonals(linestring);
                }
                last_node++;
            }
        } else {
            LineString* linestring = nullptr;
            linestring = geos_factory.create_linestring(way).release();
            PedestrianRoad* pedestrian_road = new PedestrianRoad(0, way,
                    linestring);
            ds.pedestrian_road_set.insert(pedestrian_road);
            contrast.create_orthogonals(linestring);
        }
    }

    void handle_vehicle_road(Way& way) {
        VehicleRoad *vehicle_road = new VehicleRoad(0, way);
        ds.vehicle_road_set.insert(vehicle_road);
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

    bool is_node_crossing(object_id_type node_id) {
        if (ds.crossing_node_map.find(node_id) ==
                ds.crossing_node_map.end()) {
            return false;
        }
        return true;
    }


    void iterate_over_nodes(Way& way, VehicleRoad* road) {
        object_id_type prev_node = 0;
        object_id_type current_node = 0;
        for (NodeRef node : way.nodes()) {
            current_node = node.ref();
            if (prev_node != 0) {
                if (!has_same_location(prev_node, current_node)) {
                    bool start_is_crossing = is_node_crossing(prev_node);
                    bool end_is_crossing = is_node_crossing(current_node);
                    string start_crossing_type = "";
                    string end_crossing_type = "";
                    if (start_is_crossing) {
                        start_crossing_type = ds.crossing_node_map[
                                prev_node]->type;
                    }
                    if (end_is_crossing) {
                        end_crossing_type = ds.crossing_node_map[
                                current_node]->type;
                    }
                    ds.insert_in_vehicle_node_map(prev_node, current_node, road,
                            start_is_crossing, start_crossing_type,
                            end_is_crossing, end_crossing_type);
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
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                handle_pedestrian_road(way);
            }
            if (TagCheck::is_vehicle(way) && (!TagCheck::is_tunnel(way)) && 
                (!TagCheck::is_bridge(way))) {
                handle_vehicle_road(way);
            }
        }
    }
};

#endif /* WAY_HANDLER_HPP_ */
