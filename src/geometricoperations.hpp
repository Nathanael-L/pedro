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
#include <geos/geom/GeometryFactory.h>
//#include <geos/geom>

using namespace std;

class GeomOperate {

    const int EARTH_RADIUS = 6371;
    const double TO_RAD = (3.1415926536 / 180);
    const double TO_DEG = (180 / 3.1415926536);
    OGRSpatialReference sparef_wgs84;
    OGRGeometryFactory ogr_factory;
    geos::geom::GeometryFactory geos_factory;
    GEOSContextHandle_t hGEOSCtxt;


public:
    
    GeomOperate() {
        sparef_wgs84.SetWellKnownGeogCS("WGS84");
        hGEOSCtxt = OGRGeometry::createGEOSContext();
    }

    //from http://rosettacode.org/wiki/Haversine_formula#C

    double haversine(double lon1, double lat1, double lon2, double lat2) {

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

    double haversine(Location location1, Location location2) {
        
        double distance;
        distance = haversine(location1.lon(), location1.lat(), location2.lon(),
                location2.lat());
        return distance;
    }

    //inverse haversine from http://stackoverflow.com/questions/3182260/python-geocode-filtering-by-distance

    Location inverse_haversine(double lon, double lat,
            double distance) {

        double dlon;
        double dlat;
        dlon = distance / EARTH_RADIUS;
        dlat = asin(sin(dlon) / cos(lon * TO_RAD));
        Location max_location;
        max_location.set_lon(dlon * TO_DEG);
        max_location.set_lat(dlat * TO_DEG);
        return max_location;
    }

    double orientation(double lon1, double lat1, double lon2,
            double lat2) {

        double dlon = lon2 - lon1;
        double dlat = lat2 - lat1;
        double orientation = atan(dlon / dlat) * TO_DEG;
        if (dlat < 0) {
            orientation += 180;
        } else if (dlon < 0) {
            orientation += 360;
        }
        return orientation;
    }

    double orientation(Location location1, Location location2) {
        return orientation(location1.lon(), location1.lat(), location2.lon(),
                location2.lat());
    }

    double angle(Location left, Location middle, Location right) {
        double gamma = orientation(middle, left) - orientation(middle, right);
        if (gamma < 0) {
            gamma += 360;
        }
        cout << "gamma: " << gamma << endl;
        return gamma;
    }

    /** angle using law of cosinus
    double angle(Location left, Location middle, Location right) {
        double a = haversine(left, middle);
        double b = haversine(right, middle);
        double c = haversine(left, right);
        double gamma = acos((a * a + b * b - c * c) / (2 * a * b));
        return gamma * TO_DEG;
    }
    */

    Location vertical_point(double lon1, double lat1, double lon2,
            double lat2, double distance, bool clockwise = true) {

        Location delta;
        delta = inverse_haversine(lon1, lat1, distance);
        double reverse_orientation;
        reverse_orientation = orientation(lon1, lat1, lon2, lat2);
        if (clockwise) {
            reverse_orientation += 90;
        } else {
            reverse_orientation -= 90;
        }
        Location point;
        point.set_lon(lon1 + sin(reverse_orientation * TO_RAD) * delta.lon());
        point.set_lat(lat1 + cos(reverse_orientation * TO_RAD) * delta.lat());
        return point;
    }

    Location vertical_point(Location start_location,
            Location end_location, double distance,
            bool clockwise = true) {

        Location point;
        point = vertical_point(start_location.lon(), start_location.lat(),
                end_location.lon(), end_location.lat(), distance,
                clockwise);
	    return point;
    }

    OGRGeometry *connect_locations(Location location1, Location location2) {
        OGRGeometry *ogr_line = nullptr;
        string target_wkt = "LINESTRING (";
        target_wkt += to_string(location1.lon()) + " ";
        target_wkt += to_string(location1.lat()) + ", ";
        target_wkt += to_string(location2.lon()) + " ";
        target_wkt += to_string(location2.lat()) + ")";
        char *wkt;
        wkt = strdup(target_wkt.c_str());
        if (ogr_factory.createFromWkt(&wkt, &sparef_wgs84, &ogr_line) != OGRERR_NONE) {
            cerr << "Failed to connect coordinates.";
        }
        return ogr_line;
    }

    OGRGeometry *parallel_line(Location location1,
            Location location2, double distance, bool left = true) {

        OGRGeometry *ogr_line = nullptr;
        Location start;
        Location end;
        start = vertical_point(location1, location2, distance, left);
        end = vertical_point(location2, location1, distance, !left);
        ogr_line = connect_locations(start, end);
        return ogr_line;
    }


    Location mean(Location location1,
            Location location2) {

        Location location;
        location.set_lon((location1.lon() + location2.lon()) / 2);
        location.set_lat((location1.lat() + location2.lat()) / 2);
        return location;
    }

};

#endif /* GEOMETRICOPERATIONS_HPP_ */
