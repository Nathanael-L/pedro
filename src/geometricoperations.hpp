/*
 * geometricoperations.hpp
 *
 *  Created on: Nov 11, 2015
 *      Author: nathanael
 */

#ifndef GEOMETRICOPERATIONS_HPP_
#define GEOMETRICOPERATIONS_HPP_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

using namespace std;

struct Coordinate {
    double lon;
    double lat;
};

class GeomOperate {

    static constexpr const int EARTH_RADIUS = 6371;
    static constexpr const double TO_RAD = (3.1415926536 / 180);
    static constexpr const double TO_DEG = (180 / 3.1415926536);

public:
    
    //from http://rosettacode.org/wiki/Haversine_formula#C


    static double haversine(double lon1, double lat1, double lon2,
            double lat2) {

        double dx, dy, dz;
        lat1 -= lat2;
        lat1 *= TO_RAD;
        lon1 *= TO_RAD;
        lon2 *= TO_RAD;

        dz = sin(lon1) - sin(lon2);
        dx = cos(lat1) * cos(lon1) - cos(lon2);
        dy = sin(lat1) * cos(lon1);
        return asin(sqrt(dx * dx + dy * dy + dz * dz) / 2) * 2 * EARTH_RADIUS;
    }

    //inverse haversine from http://stackoverflow.com/questions/3182260/python-geocode-filtering-by-distance

    static Coordinate inverse_haversine(double lon, double lat,
            double distance) {

        double dlon;
        double dlat;
        dlon = distance / EARTH_RADIUS;
        dlat = asin(sin(dlon) / cos(lon * TO_RAD));
        Coordinate result;
        result.lon = dlon * TO_DEG;
        result.lat = dlat * TO_DEG;
        return result;
    }

    static double angle(double lon1, double lat1, double lon2,
            double lat2) {

        double dlon = lon2 - lon1;
        double dlat = lat2 - lat1;
        double angle = tan(dlon * TO_RAD / dlat * TO_RAD);
        return angle * TO_DEG;
    }

    static Coordinate vertical_point(double lon1, double lat1, double lon2,
            double lat2, double distance) {

        Coordinate delta;
        delta = inverse_haversine(lon1, lat1, distance);
        double reverse_angle;
        cout << "angle: " << angle(lon1, lat1, lon2, lat2) << endl;
        reverse_angle = (angle(lon1, lat1, lon2, lat2) + 90);
        cout << "reverse_angle: " << reverse_angle << endl;
        Coordinate point;
        point.lon = lon1 + sin(reverse_angle * TO_RAD) * delta.lon;
        point.lat = lat1 + cos(reverse_angle * TO_RAD) * delta.lat;
        return point;
    }
};

#endif /* GEOMETRICOPERATIONS_HPP_ */
