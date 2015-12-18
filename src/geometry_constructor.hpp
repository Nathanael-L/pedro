/***
 * geometry_constructor.hpp
 *
 *  Created on: Dec 18, 2015
 *      Author: nathanael
 */


#ifndef GEOMETRY_CONSTRUCTOR_HPP_
#define GEOMETRY_CONSTRUCTOR_HPP_

class GeometryConstructor {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    //GeometryFactory geos_factory;
    //const bool left = true;
    //const bool right = false;

public:

    OGRGeometry *test;

    explicit GeometryConstructor(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {

    }

    void generate_sidewalks() {
        SidewalkFactory* sidewalk_factory = new SidewalkFactory(ds, location_handler);
        CrossingFactory* crossing_factory = new CrossingFactory(ds, location_handler);
        vector<Sidewalk*> segments;
        vector<bool> reverse;
        for (auto node : ds.vehicle_node_map) {
            segments.clear();
            reverse.clear();
            int count_neighbours = node.second.size();
            sidewalk_factory->generate_parallel_segments(node.first,
                    node.second, segments, reverse);
            sidewalk_factory->generate_connections(segments, reverse);
        }
    }
};

#endif /* GEOMETRY_CONSTRUCTOR_HPP_ */
