/***
 * prepare_handler.hpp
 *
 *  Created on: Dec 7, 2015
 *      Author: nathanael
 */

#ifndef PREPARE_HANDLER_HPP_
#define PREPARE_HANDLER_HPP_




class PrepareHandler : public handler::Handler {

    DataStorage& ds;
    location_handler_type& location_handler;
    bool is_first_way = true;
    google::sparse_hash_map<object_id_type,
            vector<object_id_type>> temp_node_map;

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

    void way(Way& way) {
        if (is_first_way) {
            cerr << "... prepare ways ..." << endl;
        }
        if (TagCheck::is_highway(way)) {
            if (TagCheck::is_pedestrian(way)) {
                prepare_pedestrian_road(way);
            }
        }
        is_first_way = false;
    }

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

