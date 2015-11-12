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

    const int EARTH_RADIUS = 6371;
    const double TO_RAD = (3.1415926536 / 180);
    const double TO_DEG = (180 / 3.1415926536);
    OGRSpatialReference sparef_wgs84;
    OGRGeometryFactory ogr_factory;

public:
    
    GeomOperate() {
        sparef_wgs84.SetWellKnownGeogCS("WGS84");
    }

    //from http://rosettacode.org/wiki/Haversine_formula#C

    double haversine(double lon1, double lat1, double lon2,
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

    osmium::Location inverse_haversine(double lon, double lat,
            double distance) {

        double dlon;
        double dlat;
        dlon = distance / EARTH_RADIUS;
        dlat = asin(sin(dlon) / cos(lon * TO_RAD));
        osmium::Location max_location;
        max_location.set_lon(dlon * TO_DEG);
        max_location.set_lat(dlat * TO_DEG);
        return max_location;
    }

    double angle(double lon1, double lat1, double lon2,
            double lat2) {

        double dlon = lon2 - lon1;
        double dlat = lat2 - lat1;
        double angle = atan(dlon / dlat) * TO_DEG;
        if (dlat < 0) {
            angle += 180;
        } else if (dlon < 0) {
            angle += 360;
        }
        return angle;
    }

    osmium::Location vertical_point(double lon1, double lat1, double lon2,
            double lat2, double distance, bool clockwise = true) {

        osmium::Location delta;
        delta = inverse_haversine(lon1, lat1, distance);
        double reverse_angle;
        reverse_angle = angle(lon1, lat1, lon2, lat2);
        if (clockwise) {
            reverse_angle += 90;
        } else {
            reverse_angle -= 90;
        }
        osmium::Location point;
        point.set_lon(lon1 + sin(reverse_angle * TO_RAD) * delta.lon());
        point.set_lat(lat1 + cos(reverse_angle * TO_RAD) * delta.lat());
        return point;
    }

    OGRGeometry *parallel_line(OGRLineString *source_line,
        double distance, bool left = true) {

        OGRGeometry *target_line;
        OGRPoint *source_start;
        OGRPoint *source_end;
        osmium::Location target_start;
        osmium::Location target_end;
        source_line->StartPoint(source_start);
        source_line->EndPoint(source_end);
        double lon1 = source_start->getY();
        double lat1 = source_start->getX();
        double lon2 = source_end->getY();
        double lat2 = source_end->getX();
        target_start = vertical_point(lon1, lat1, lon2, lat2, distance, left);
        target_end = vertical_point(lon1, lat1, lon2, lat2, distance, !left);
        string target_wkt = "LINESTRING (";
        target_wkt += to_string(target_start.lon()) + " ";
        target_wkt += to_string(target_start.lat()) + " ";
        target_wkt += to_string(target_end.lon()) + " ";
        target_wkt += to_string(target_end.lat()) + " ";
        char *wkt;
        wkt = strdup(target_wkt.c_str());
        //ERROR ABFANGEN
        ogr_factory.createFromWkt(&wkt, &sparef_wgs84, &target_line);
        return target_line;

        
    }
};

#endif /* GEOMETRICOPERATIONS_HPP_ */
