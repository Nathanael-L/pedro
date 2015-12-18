
/***
 * pedro_point.hpp
 *
 *  Created on: Dec 19, 2015
 *      Author: nathanael
 *
 * Structure to remember the relevant points: crossing_point, 
 */

#ifndef PEDRO_POINT_HPP_
#define PEDRO_POINT_HPP_

struct PedroPoint {

    string type;

    void init_point(string type) {
        this->type = type;
    }
};


struct CrossingPoint : PedroPoint {

    CrossingPoint(string type) {
        this->init_point(type);
    }
};

#endif /* PEDRO_POINT_HPP_ */
