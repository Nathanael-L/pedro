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

struct LonLat {
    double lon;
    double lat;

    LonLat(double longitude, double latitude) {
        lon = longitude;
        lat = latitude;
    }
};

class GeomOperate {

    const int EARTH_RADIUS = 6371;
    const double TO_RAD = (3.1415926536 / 180);
    const double TO_DEG = (180 / 3.1415926536);
    OGRSpatialReference sparef_wgs84;
    OGRGeometryFactory ogr_factory;
    geos::geom::GeometryFactory geos_factory;
    geos::io::WKTReader geos_wkt_reader;
    GEOSContextHandle_t hGEOSCtxt;


public:
    
    GeomOperate() {
        sparef_wgs84.SetWellKnownGeogCS("WGS84");
        hGEOSCtxt = OGRGeometry::createGEOSContext();
        geos_wkt_reader = geos::io::WKTReader(geos_factory);
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

    LonLat inverse_haversine(double lon, double lat,
            double distance) {

        double dlon;
        double dlat;
        dlat = distance / EARTH_RADIUS;
        dlon = asin(sin(dlat) / cos(lat * TO_RAD));
        LonLat max_lonlat(dlon * TO_DEG, dlat * TO_DEG);
        return max_lonlat;
    }

    double difference(double d1, double d2) {
        return (max(d1, d2) - min (d1, d2));
    }

    double orientation(double lon1, double lat1, double lon2,
            double lat2) {

        double dlon = difference(lon1, lon2);
        double dlat = difference(lat1, lat2);
        double orientation = atan(dlon / (dlat/* sin(lat1 * TO_RAD)*/)) * TO_DEG;
        if (lat1 < lat2) {
            if (lon1 > lon2) {
                /* 2. Quadrant */
                orientation = 180 - orientation;
            } else {
                /* 3. Quadrant */
                orientation += 180;
            }
        } else {
            if (lon1 < lon2) {
                /* 4. Quadrant */
                orientation = 360 - orientation;
            } else {
                /* 1. Quadrant */
                /* nothing     */
            }
        }
        return orientation;
    }

    double orientation(Location location1, Location location2) {
        return orientation(location1.lon(), location1.lat(), location2.lon(),
                location2.lat());
    }

    double angle(Location first, Location middle, Location second) {
        double orientation1 = orientation(middle, second);
        double orientation2 = orientation(middle, first);
        double alpha = orientation1 - orientation2;
        if (alpha < 0 ) {
            alpha += 360;
        }
        return alpha;
    }

    Location vertical_point(double lon1, double lat1, double lon2,
            double lat2, double distance, bool left = true) {

        LonLat delta = inverse_haversine(lon1, lat1, distance);
        double reverse_orientation;
        reverse_orientation = orientation(lon1, lat1, lon2, lat2);
        if (left) {
            reverse_orientation -= 90;
        } else {
            reverse_orientation += 90;
        }
        Location point;
        double new_lon = lon1 + sin(reverse_orientation * TO_RAD) * delta.lon;
        double new_lat = lat1 + cos(reverse_orientation * TO_RAD) * delta.lat;
        point.set_lon(new_lon);
        point.set_lat(new_lat);
        return point;
    }

    Location vertical_point(Location start_location,
            Location end_location, double distance,
            bool left = true) {

        Location point;
        point = vertical_point(start_location.lon(), start_location.lat(),
                end_location.lon(), end_location.lat(), distance,
                left);
	    return point;
    }

    geos::geom::Geometry *connect_locations(Location location1, Location location2) {
        geos::geom::Geometry *geos_line = nullptr;
        string target_wkt = "LINESTRING (";
        target_wkt += to_string(location1.lon()) + " ";
        target_wkt += to_string(location1.lat()) + ", ";
        target_wkt += to_string(location2.lon()) + " ";
        target_wkt += to_string(location2.lat()) + ")";
        const string wkt = target_wkt;
        geos_line = geos_wkt_reader.read(wkt);
        
        if (!geos_line) {
            cerr << "Failed to create from wkt.";
            exit(1);
        }
        return geos_line;
    }

    geos::geom::Geometry *parallel_line(Location location1,
            Location location2, double distance, bool left = true) {

        geos::geom::Geometry *geos_line = nullptr;
        Location start;
        Location end;
        start = vertical_point(location1, location2, distance, left);
        end = vertical_point(location2, location1, distance, !left);
        geos_line = connect_locations(start, end);
        return geos_line;
    }

    OGRGeometry *ogr_connect_locations(Location location1, Location location2) {
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

    OGRGeometry *ogr_parallel_line(Location location1,
            Location location2, double distance, bool left = true) {

        OGRGeometry *ogr_line = nullptr;
        Location start;
        Location end;
        start = vertical_point(location1, location2, distance, left);
        end = vertical_point(location2, location1, distance, !left);
        ogr_line = ogr_connect_locations(start, end);
        return ogr_line;
    }

    geos::geom::Geometry *union_geometries(vector<geos::geom::Geometry*>
                *geom_vector) { 
        
        geos::geom::GeometryCollection *geom_collection = nullptr;
        geos::geom::Geometry *result = nullptr;
        try {
            geom_collection = geos_factory.createGeometryCollection(
                    geom_vector);
        } catch (...) {
            cerr << "Failed to create geometry collection." << endl;
            exit(1);
        }
        try {
            result = geom_collection->Union().release();
        } catch (...) {
            cerr << "Failed to union linestrings at relation: " << endl;
            delete geom_collection;
            exit(1);
        }
        delete geom_collection;
        return result;
    }

    Location mean(Location location1,
            Location location2) {

        Location location;
        location.set_lon((location1.lon() + location2.lon()) / 2);
        location.set_lat((location1.lat() + location2.lat()) / 2);
        return location;
    }

    OGRGeometry *ogr2geos(OGRGeometry *ogr_geom)
    {
        geos::geom::Geometry *geos_geom;

        ostream *os;
        string wkb = os.str();
        if (ogr_geom->exportToWkb(wkbXDR, (unsigned char *) wkb.c_str())
                != OGRERR_NONE ) {
            geos_geom = nullptr;
            assert(false);
        }
        geos::io::WKBReader wkbReader;
        
        //wkbWriter.setOutputDimension(g->getCoordinateDimension());
        geos_geom = wkbReader.read(os);
        return geos_geom;
    }

    OGRGeometry *geos2ogr(const geos::geom::Geometry *g)
    {
        OGRGeometry *ogr_geom;

        geos::io::WKBWriter wkbWriter;
        wkbWriter.setOutputDimension(g->getCoordinateDimension());
        ostringstream ss;
        wkbWriter.write(*g, ss);
        string wkb = ss.str();
        if (OGRGeometryFactory::createFromWkb((unsigned char *) wkb.c_str(),
                                              nullptr, &ogr_geom, wkb.size())
            != OGRERR_NONE ) {
            ogr_geom = nullptr;
            assert(false);
        }
        return ogr_geom;
    }
};

#endif /* GEOMETRICOPERATIONS_HPP_ */




    /** angle using law of cosinus
    double angle(Location left, Location middle, Location right) {
        double a = haversine(left, middle);
        double b = haversine(right, middle);
        double c = haversine(left, right);
        double gamma = acos((a * a + b * b - c * c) / (2 * a * b));
        return gamma * TO_DEG;
    }
    */

