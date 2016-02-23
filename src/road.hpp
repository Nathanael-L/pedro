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
 *  Sidewalk:           form|to|left/right[0..1]|index[00..99]
 *    index is 00 as long as LineString is not splitted
 *  Crossing:           form|to|left/right[0..1]|index[00..99]
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

struct CrossingID {
    string sidewalk_id;
    bool osm_crossing;
    int index;

    CrossingID(string sidewalk_id, bool osm_crossing, int index) {
        this->sidewalk_id = sidewalk_id;
        this->osm_crossing = osm_crossing;
        this->index = index;
    }
};

class PedroRoad {

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
    Geometry* geometry;

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
	    cerr << "bad reference fr geometry" << endl;
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
	    cerr << "bad reference fr geometry" << endl;
	    exit(1);
	}
    }

    OGRGeometry* get_ogr_geom() {
        return go.geos2ogr(geometry);
    }
};


class PedestrianRoad : public PedroRoad {

    virtual string get_id(int index, Way& way) {
        string id = to_string(way.id());
        id += pad_zeroes(index, 2);
        return id;
    }

    virtual string get_id(int index, string old_id) {
        int len_old = old_id.length();
        string id = old_id.substr(0, len_old - 3);
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

    PedestrianRoad(int index, PedestrianRoad* road, Geometry* geometry) {
        this->id = get_id(index, road->id);
        this->name = road->name;
        this->type = road->type;
        this->geometry = geometry;
        this->length = go.get_length(geometry);
        this->osm_id = road->osm_id;
    }
    
    int get_index() {
        int len = id.length();
        int index = stoi(id.substr(len - 3, 2));
        return index;
    }
};


class VehicleRoad : public PedroRoad {
    
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
        sidewalk = TagCheck::get_sidewalk_type(way);
        lanes = TagCheck::get_lanes(way);
    }
};


class Sidewalk : public PedroRoad {
    
    virtual string get_id(SidewalkID sid) {
        string id = "";
        id += pad_zeroes(sid.from, 6);
        id += pad_zeroes(sid.to, 6);
        id += (sid.left) ? "0" : "1";
        id += pad_zeroes(sid.index, 2);
        return id;
    }

    string increment_id(string origin_id) {
        string new_id;
        int index = stoi(origin_id.substr(13, 2));
        index++;
        new_id = change_index(origin_id, index);
        return new_id;
    }

    string change_index(string origin_id, int index) {
        string new_id;
        new_id = origin_id.substr(0, 13) + pad_zeroes(index, 2);
        return new_id;
    }

public:

    string osm_id;
    string at_osm_type;

    Sidewalk(SidewalkID sid, string name, Geometry* geometry,
            string type, string at_osm_type, double length) {

        this->id = get_id(sid);
        this->name = name;
	if (!geometry) {
	    cerr << "bad reference fr geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->type = type;
        this->at_osm_type = at_osm_type;
        this->length = length;
        //this->osm_id = osm_id;
    }

    Sidewalk(SidewalkID sid, Geometry* geometry, VehicleRoad* vehicle_road) {
        this->id = get_id(sid);
        this->name = vehicle_road->name;
        this->geometry = geometry;
        this->type = "sidewalk";
        this->at_osm_type = vehicle_road->type;
        this->length = go.get_length(geometry);
        this->osm_id = vehicle_road->osm_id;
    }        

    Sidewalk(Sidewalk* origin_sidewalk, Geometry* geometry, int new_id = -1) {
        if (new_id == -1) {
            this->id = increment_id(origin_sidewalk->id);
        } else {
            this->id = change_index(origin_sidewalk->id, new_id);
        }
        this->name = origin_sidewalk->name;
        this->geometry = geometry;
        this->type = origin_sidewalk->type;
        this->at_osm_type = origin_sidewalk->at_osm_type;
        this->length = go.get_length(geometry);
        this->osm_id = origin_sidewalk->osm_id;
    }

    string get_neighbour_id() {
        char this_side = id.at(12);
        const string other_side = (this_side == '0') ? "1" : "0";
        string neighbour_id = id.substr(0, 12) + other_side + id.substr(13, 2);
        return neighbour_id;
    }

    int get_index() {
        int index = stoi(id.substr(13, 2));
        return index;
    }
};


class Crossing : public PedroRoad {

    virtual string get_id(CrossingID cid) {
        string id = "";
        id += cid.sidewalk_id.substr(0, cid.sidewalk_id.size() - 3);
        id += (cid.osm_crossing) ? "2" : "3";
        id += pad_zeroes(cid.index, 2);
        return id;
    }

    string increment_id(string origin_id) {
        string new_id;
        int index = stoi(origin_id.substr(13, 2));
        index++;
        new_id = change_index(origin_id, index);
        return new_id;
    }

    string change_index(string origin_id, int index) {
        string new_id;
        new_id = origin_id.substr(0, 13) + pad_zeroes(index, 2);
        return new_id;
    }

public:
    
    //string osm_id;
    string osm_type;

    Crossing(CrossingID cid, string name, Geometry* geometry,
            string type, string osm_type, double length) {

        this->id = get_id(cid);
        this->name = name;
	if (!geometry) {
	    cerr << "bad reference fr geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->type = type;
        this->osm_type = osm_type;
        this->length = length;
        //this->osm_id = osm_id;
    }

    Crossing(Crossing* origin_crossing, Geometry* geometry, int new_id = -1) {
        if (new_id == -1) {
            this->id = increment_id(origin_crossing->id);
        } else {
            this->id = change_index(origin_crossing->id, new_id);
        }
        this->name = origin_crossing->name;
        this->geometry = geometry;
        this->type = origin_crossing->type;
        //this->at_osm_type = origin_crossing->at_osm_type;
        this->length = go.get_length(geometry);
        //this->osm_id = origin_crossing->osm_id;
        this->osm_type = origin_crossing->osm_type;
    }

    int get_index() {
        int index = stoi(id.substr(13, 2));
        return index;
    }
};

#endif /* ROAD_HPP_ */
