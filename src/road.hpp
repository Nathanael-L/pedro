
/***
 * Structure to remember the pedestrian and vehicle used roads to create
 * sidewalks and crossings.
 *
 * length is in kilometers (pgRouting-style)
 *
 * 
 * Identifiers are:
 *  PedestrianRoad:     osm id|index[00..99]
 *  VehicleRoad:        osm id|index[000..999]
 *  Sidewalk:           from|to|left/right[0..1]|index[00..99]
 *    index is 00 as long as LineString is not splitted
 *
 * Sidewalk Characters are:
 *  'l' = left
 *  'r' = right
 *  'n' = none
 *
 */

#ifndef ROAD_HPP_
#define ROAD_HPP_

struct SidewalkID {
    int from;
    int to;
    int left;
    int index;

    SidewalkID(int from, int to, bool left, int index) {
        this->from = from;
        this->to = to;
        this->left = left;
        this->index = index;
    }
};


class Road {

    geom::GEOSFactory<> geos_factory;
    //virtual string get_id(...) const = 0;

protected:

    GeomOperate go;

    string pad_zeroes(int index, int num_char) {
        string with_zeroes = "";
        int factor = pow(10, (num_char - 1));
        while ((factor > 0) && ((index / factor) == 0)) {
            with_zeroes += "0";
            factor *= 0.1;
        }
        with_zeroes += to_string(index);
        return with_zeroes;    
    }

public: 

    string id;
    string name;
    string type;
    double length;
    Geometry *geometry;

    void init_road(string id, Way& way) {
        this->id = id;
        name = TagCheck::get_name(way);
        type = TagCheck::get_highway_type(way);
        try {
            geometry = nullptr;
            geometry = geos_factory.create_linestring(way,
                    geom::use_nodes::unique,
                    geom::direction::forward).release();
        } catch (...) {
            cerr << " GEOS ERROR at way: " << way.id() << endl;
        }

        length = go.get_length(geometry);
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
    }

    void init_road(string id, Way& way, Geometry* geometry) {
        this->id = id;
        name = TagCheck::get_name(way);
        type = TagCheck::get_highway_type(way);
        this->geometry = geometry;
        length = go.get_length(geometry);
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
    }

    OGRGeometry *get_ogr_geom() {
        return go.geos2ogr(geometry);
    }
};


class PedestrianRoad : public Road {

    virtual string get_id(int index, Way& way) {
        string id = to_string(way.id());
        id += pad_zeroes(index, 2);
        return id;
    }

public:

    string osm_id;

    PedestrianRoad(int index, Way& way) {
        this->id = get_id(index, way);
        init_road(id, way);
        osm_id = to_string(way.id());
    }

    PedestrianRoad(int index, Way& way, Geometry* geometry) {
        this->id = get_id(index, way);
        init_road(id, way, geometry);
        osm_id = to_string(way.id());
    }
};


class VehicleRoad : public Road {
    
    virtual string get_id(int index, Way& way) {
        string id = to_string(way.id());
        id += pad_zeroes(index, 3);
        return id;
    }

public:

    string osm_id;
    int lanes;
    char sidewalk;

    VehicleRoad(int index, Way& way) {
        init_road(id, way);
        osm_id = to_string(way.id());
        sidewalk = TagCheck::get_sidewalk(way);
        lanes = TagCheck::get_lanes(way);
    }
};


class Sidewalk : public Road {
    
    virtual string get_id(SidewalkID sid) {
        string id = "";
        id += pad_zeroes(sid.from, 6);
        id += pad_zeroes(sid.to, 6);
        id += (sid.left) ? "0" : "1";
        id += pad_zeroes(sid.index, 2);
        return id;
    }

public:

    string osm_id;

    Sidewalk(SidewalkID sid, string name, Geometry *geometry,
            string type, double length) {

        this->id = get_id(sid);
        this->name = name;
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->type = type;
        this->length = length;
        //this->osm_id = osm_id;
    }

    Sidewalk(SidewalkID sid, Geometry *geometry, VehicleRoad *vehicle_road) {
        this->id = get_id(sid);
        this->name = vehicle_road->name;
        this->geometry = geometry;
        this->type = vehicle_road->type;
        this->length = go.get_length(geometry);
        this->osm_id = vehicle_road->osm_id;
    }        

    string get_neighbour_id() {
        char this_side = id.at(12);
        const string other_side = (this_side == '0') ? "1" : "0";
        cout << "id before: " << id << endl;
        string neighbour_id = id.replace(12, 1, other_side);
        cout << "id before: " << id << endl;
        return neighbour_id;
    }
};


class Crossing : public Road {

    virtual string get_id() {
        return ""; //TODO
    }

public:
    
    string osm_id;

    Crossing(string id, string name, Geometry *geometry,
            string type, double length) {

        this->name = name;
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->type = type;
        this->length = length;
        this->osm_id = osm_id;
    }
};

#endif /* ROAD_HPP_ */
