/***
 * geom_operate.hpp
 * former: geometricoperations.hpp
 *
 *  Created on: Nov 11, 2015
 *      Author: nathanael
 */

#ifndef GEOM_OPERATE_HPP_
#define GEOM_OPERATE_HPP_

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <geos/geom/GeometryFactory.h>

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
    const int SQRT2 = 1.4142;
    const double TO_RAD = (3.1415926536 / 180);
    const double TO_DEG = (180 / 3.1415926536);
    OGRSpatialReference sparef_wgs84;
    OGRGeometryFactory ogr_factory;
    GeometryFactory geos_factory;
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

    double haversine(Point* point1, Point* point2) {
        
        double distance;
        distance = haversine(point1->getX(), point1->getY(), point2->getX(),
                point2->getY());
        return distance;
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
        if (lat1 > lat2) {
            if (lon1 < lon2) {
                /* 2. Quadrant */
                orientation = 180 - orientation;
            } else {
                /* 3. Quadrant */
                orientation += 180;
            }
        } else {
            if (lon1 > lon2) {
                /* 4. Quadrant */
                orientation = 360 - orientation;
            } else {
                /* 1. Quadrant */
                /* nothing     */
            }
        }
        return orientation;
    }

        
    double orientation(Point* point1, Point* point2) {
        return orientation(point1->getX(), point1->getY(), point2->getX(),
                point2->getY());
    }

    double orientation(Location location1, Location location2) {
        return orientation(location1.lon(), location1.lat(), location2.lon(),
                location2.lat());
    }

    double angle(Location first, Location middle, Location second) {
        double orientation1 = orientation(middle, first);
        double orientation2 = orientation(middle, second);
        double alpha = orientation2 - orientation1;
        if (alpha < 0 ) {
            alpha += 360;
        }
        return alpha;
    }

    double angle(LineString* from, LineString* to) {
        double lon_from_start = from->getStartPoint()->getX();
        double lat_from_start = from->getStartPoint()->getY();
        double lon_from_end = from->getEndPoint()->getX();
        double lat_from_end = from->getEndPoint()->getY();
        double lon_to_start = to->getStartPoint()->getX();
        double lat_to_start = to->getStartPoint()->getY();
        double lon_to_end = to->getEndPoint()->getX();
        double lat_to_end = to->getEndPoint()->getY();

        double orientation1 = orientation(lon_from_start, lat_from_start,
                lon_from_end, lat_from_end);
        double orientation2 = orientation(lon_to_start, lat_to_start,
                lon_to_end, lat_to_end);
        double alpha = orientation2 - orientation1;
        if (alpha < 0 ) {
            alpha += 360;
        }
        return alpha;
    }

    Point* vertical_point(double lon1, double lat1, double lon2,
            double lat2, double distance, bool left = true) {

        int angle = 90;
        /*if (extend) {
            distance *= SQRT2;
            angle = 45;
        }*/
        LonLat delta = inverse_haversine(lon1, lat1, distance);
        double reverse_orientation;
        reverse_orientation = orientation(lon1, lat1, lon2, lat2);
        if (left) {
            reverse_orientation -= angle;
        } else {
            reverse_orientation += angle;
        }
        double new_lon = lon1 + sin(reverse_orientation * TO_RAD) * delta.lon;
        double new_lat = lat1 + cos(reverse_orientation * TO_RAD) * delta.lat;
        const Coordinate coord = Coordinate(new_lon, new_lat);
        Point* point = geos_factory.createPoint(coord);
        return point;
    }

    Location vertical_location(double lon1, double lat1, double lon2,
            double lat2, double distance, bool left = true) {

        int angle = 90;
        /*if (extend) {
            distance *= SQRT2;
            angle = 45;
        }*/
        LonLat delta = inverse_haversine(lon1, lat1, distance);
        double reverse_orientation;
        reverse_orientation = orientation(lon1, lat1, lon2, lat2);
        if (left) {
            reverse_orientation -= angle;
        } else {
            reverse_orientation += angle;
        }
        Location point;
        double new_lon = lon1 + sin(reverse_orientation * TO_RAD) * delta.lon;
        double new_lat = lat1 + cos(reverse_orientation * TO_RAD) * delta.lat;
        point.set_lon(new_lon);
        point.set_lat(new_lat);
        return point;
    }

    Point* vertical_point(Point* start_point, Point* end_point,
            double distance, bool left = true) {

        Point* point;
        point = vertical_point(start_point->getX(), start_point->getY(),
                end_point->getX(), end_point->getY(), distance, left);
	return point;
    }

    Location vertical_location(Location start_location, Location end_location,
            double distance, bool left = true) {

        Location point;
        point = vertical_location(start_location.lon(), start_location.lat(),
                end_location.lon(), end_location.lat(), distance, left);
	return point;
    }

    LineString* parallel_line(Location location1,
            Location location2, double distance, bool left = true) {

        LineString* geos_line = nullptr;
        Location start;
        Location end;
        start = vertical_location(location1, location2, distance, left);
        end = vertical_location(location2, location1, distance, !left);
        geos_line = connect_locations(start, end);
        return geos_line;
    }

    LineString* orthogonal_line(Point* point1, Point* point2, double distance) {
        LineString* ortho_line = nullptr;
        Point* left_point = nullptr;
        Point* right_point = nullptr;
        left_point = vertical_point(point1, point2, distance, true);
        right_point = vertical_point(point1, point2, distance, false);
        ortho_line = connect_points(left_point, right_point);        
        return ortho_line;
    }

    LineString* connect_points(Point* point1, Point* point2) {
        
        const Coordinate* coordinate1;
        const Coordinate* coordinate2;
        coordinate1 = point1->getCoordinate();
        coordinate2 = point2->getCoordinate();
        vector<Coordinate>* coord_v = new vector<Coordinate>();
        coord_v->push_back(*coordinate1);
        coord_v->push_back(*coordinate2);
        const CoordinateArraySequence coords = CoordinateArraySequence(coord_v);
        return geos_factory.createLineString(coords);
    }

    LineString* connect_locations(Location location1, Location location2) {
        Geometry* geos_line = nullptr;
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
        return dynamic_cast<LineString*>(geos_line);
    }

    OGRGeometry* ogr_connect_locations(Location location1, Location location2) {
        OGRGeometry* ogr_line = nullptr;
        string target_wkt = "LINESTRING (";
        target_wkt += to_string(location1.lon()) + " ";
        target_wkt += to_string(location1.lat()) + ", ";
        target_wkt += to_string(location2.lon()) + " ";
        target_wkt += to_string(location2.lat()) + ")";
        char* wkt;
        wkt = strdup(target_wkt.c_str());
        if (ogr_factory.createFromWkt(&wkt, &sparef_wgs84, &ogr_line) != OGRERR_NONE) {
            cerr << "Failed to connect coordinates.";
        }
        return ogr_line;
    }

    OGRGeometry* ogr_parallel_line(Location location1,
            Location location2, double distance, bool left = true) {

        OGRGeometry* ogr_line = nullptr;
        Location start;
        Location end;
        start = vertical_location(location1, location2, distance, left);
        end = vertical_location(location2, location1, distance, !left);
        ogr_line = ogr_connect_locations(start, end);
        return ogr_line;
    }

    /* unused */
    Geometry* union_geometries(vector<Geometry*>
                geom_vector) { 
        
        geos::geom::GeometryCollection* geom_collection = nullptr;
        Geometry* result = nullptr;
        try {
            geom_collection = geos_factory.createGeometryCollection(
                    &geom_vector);
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
        //delete geom_collection;
        return result;
    }

    Point* mean(Point* point1, Point* point2) {

        double lon = ((point1->getX() + point2->getX()) / 2);
        double lat = ((point1->getY() + point2->getY()) / 2);
        Coordinate mean_coords(lon, lat);
        Point* mean = geos_factory.createPoint(mean_coords);
        return mean;
    }

    /*Geometry* ogr2geos(OGRGeometry* ogr_geom)
    {
        Geometry* geos_geom;

        istream is;
        string wkb = is.str();
        if (ogr_geom->exportToWkb(wkbXDR, (unsigned char* ) wkb.c_str())
                != OGRERR_NONE ) {
            geos_geom = nullptr;
            assert(false);
        }
        geos::io::WKBReader wkbReader;
        
        //wkbWriter.setOutputDimension(g->getCoordinateDimension());
        geos_geom = wkbReader.read(is);
        return geos_geom;
    }*/

    double get_length(Geometry *geometry) {
        double length = 0;
        if (geometry->getGeometryType() == "LineString") {
            LineString* linestring = dynamic_cast<LineString*>(geometry);
            CoordinateSequence *coords;
            coords = linestring->getCoordinates();
            for (int i = 0; i < (coords->getSize() - 1); i++) {
                Coordinate start = coords->getAt(i);
                Coordinate end = coords->getAt(i + 1);
                length += haversine(start.x, start.y, end.x, end.y);
            }
        } else {
            cerr << "error while get_length, geometry not LineString but "
                    << geometry->getGeometryType() << endl;
            exit(1);
        }
        return length;
    }

    OGRGeometry* geos2ogr(const Geometry* g)
    {
        OGRGeometry* ogr_geom;

        geos::io::WKBWriter wkbWriter;
        wkbWriter.setOutputDimension(g->getCoordinateDimension());
        ostringstream ss;
        wkbWriter.write(*g, ss);
        string wkb = ss.str();
        if (OGRGeometryFactory::createFromWkb((unsigned char* ) wkb.c_str(),
                                              nullptr, &ogr_geom, wkb.size())
            != OGRERR_NONE ) {
            ogr_geom = nullptr;
            assert(false);
        }
        return ogr_geom;
    }
};

#endif /* GEOM_OPERATE_HPP_ */




    /** angle using law of cosinus
    double angle(Location left, Location middle, Location right) {
        double a = haversine(left, middle);
        double b = haversine(right, middle);
        double c = haversine(left, right);
        double gamma = acos((a * a + b * b - c * c) / (2 * a * b));
        return gamma * TO_DEG;
    }
    */

