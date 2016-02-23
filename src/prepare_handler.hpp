/***
 * prepare_handler.hpp
 *
 *  Created on: Dec 7, 2015
 *      Author: nathanael
 *
 * In the first pass of reading the OSM Data all crossing nodes are collected
 * and the pedestrian_node_map is created. The map is used to split the
 * pedestrian roads.
 * The crossing nodes are collected in the crossing_node_map to construct the
 * crossings later.
 *
 */

#ifndef PREPARE_HANDLER_HPP_
#define PREPARE_HANDLER_HPP_

class PrepareHandler : public handler::Handler {

    DataStorage& ds;
    location_handler_type& location_handler;
    google::sparse_hash_map<object_id_type,
            vector<object_id_type>> temp_node_map;

    /***
     * In the temp_node_map the way ids are collected for every node id.
     */
    void prepare_pedestrian_road(Way& way) {
        object_id_type way_id = way.id();
        for (auto node : way.nodes()) {
            temp_node_map[node.ref()].push_back(way_id);
        }
    }

public:

    explicit PrepareHandler(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {

	temp_node_map.set_deleted_key(-1);
    }

    /***
     * Every node is checked if it is a crossing node. In the crossing_node_map
     * the crossings and their type is collected.
     */
    void node(Node& node) {
        if (TagCheck::node_is_crossing(node)) {
            string type = TagCheck::get_crossing_type(node);
            CrossingPoint* crossing = new CrossingPoint(type);
            ds.crossing_node_map[node.id()] = crossing;
        }        
    }

    /***
     * All pedestrian roads are collected.
     */
    void way(Way& way) {
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                prepare_pedestrian_road(way);
            }
        }
    }

    /***
     * All pedestrian nodes are collected to split the way where more than one
     * pedestrian road connects each other.
     */
    void create_pedestrian_node_map() {
        for (auto entry : temp_node_map) {
            if (entry.second.size() > 1) {
                object_id_type node_id = entry.first;
                for (object_id_type way_id : entry.second) {
                    ds.pedestrian_node_map[way_id].push_back(node_id);
                }
            }
        }
    }
};

#endif /* PREPARE_HANDLER_HPP_ */

