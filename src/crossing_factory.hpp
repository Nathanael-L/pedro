/***
 * crosing_factory.hpp
 *
 *  Created on: Dec 19, 2015
 *      Author: nathanael
 */


#ifndef CROSSING_FACTORY_HPP_
#define CROSSING_FACTORY_HPP_

class CrossingFactory {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    GeometryFactory geos_factory;
    const bool left = true;
    const bool right = false;

public:

    OGRGeometry *test;

    explicit CrossingFactory(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    void generate_crossings(vector<Sidewalk*>& segments,
            vector<bool> reverse, SidewalkFactory sidewalk_factory) {

        int count_segments = segments.size();
        if (count_segments == 4) {
            if ((segments[0]) && (segments[1])) {
                connect_ends(segments[0], segments[1],
                        reverse[0], reverse[1]);
            }
        } else {
            cout << "segments_size: " << count_segments << endl;
        }

    }
};

#endif /* CROSSING_FACTORY_HPP_ */
