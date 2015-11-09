/*
 * tagcheck.hpp
 *
 *  Created on: Jul 6, 2015
 *      Author: nathanael
 */

#ifndef TAGCHECK_HPP_
#define TAGCHECK_HPP_

#include <osmium/osm/tag.hpp>
#include <osmium/tags/filter.hpp>
#include <stdlib.h>


using namespace std;

class TagCheck {

    /*static const char *get_highway_type(const char *raw_type) {
        if (!raw_type) {
            return nullptr;
        }
        if ((strcmp(raw_type, "river")) && (strcmp(raw_type, "stream")) 
                && (strcmp(raw_type, "drain")) && (strcmp(raw_type, "brook"))
                && (strcmp(raw_type, "canal")) && (strcmp(raw_type,"ditch"))
                && (strcmp(raw_type, "riverbank"))) {
            return strdup("other\0");
        } else {
            return strdup(raw_type);
        }
    }*/

    static const bool char_in_list(const char *pattern,
            vector<string> search_list) {
        string pattern_str(pattern);
        for (auto item : search_list) {
            if (item == pattern_str) {
                return true;
            }
        }
        return false;
    }
    
public:

    static bool is_highway(const osmium::OSMObject &osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        if (highway) {
            return true;
        }
    }

    static bool is_vehicle(const osmium::OSMObject &osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        string type_list[] = {
            "motorway", "trunk", "primary", "secondary", "tertiary",
            "unclassified", "road", "residential", "service",        
            "motorway_link", "trunk_link", "primary_link", "secondary_link",
            "tertiary_link", "bus_guideway"
        };
        vector<string> type_vector;
        for (string item : type_list) {
            type_vector.push_back(item);
        }
        return (char_in_list(highway, type_vector));
    }

    static bool is_pedestrian(const osmium::OSMObject &osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        string type_list[] = {
            "pedestrian", "footway", "steps", "path", "track", "living_street"
        };
        vector<string> type_vector;
        for (string item : type_list) {
            type_vector.push_back(item);
        }
        if (char_in_list(highway, type_vector)) {
            return true;
        } else {
            if (!strcmp(highway, "cycleway")) {
                const char* foot = osm_object.get_value_by_key("foot");
                if ((foot) && (!strcmp(foot, "yes"))) {
                    return true;
                }
            }
            return false;
        }
    }

    static bool is_polygon(const osmium::OSMObject &osm_object,
                                bool is_relation) {
        /*not ready yet*/
        if (is_relation) {
            const char* type = osm_object.tags().get_value_by_key("type");
            if (!type) {
                return false;
            }
            if (strcmp(type, "multipolygon")) {
                return false;
            }
        }
        const char* natural = osm_object.get_value_by_key("natural");
        if ((natural) && (!strcmp(natural, "water"))) {
            return true;
        }
        const char* waterway = osm_object.get_value_by_key("waterway");
        if (waterway) {
            return true;
        }
        const char* landuse = osm_object.get_value_by_key("landuse");
        if ((landuse) && (!strcmp(landuse, "reservoir"))) {
            return true;
        }
        if ((landuse) && (!strcmp(landuse, "basin"))) {
            return true;
        }
        return false;
    }

    static const char *get_highway_type(const osmium::OSMObject &osm_object) {
        return osm_object.get_value_by_key("highway");
    }

    /*static const char *get_polygon_type(const osmium::Area &area) {
        const char *type;
        const char *natural = area.get_value_by_key("natural");
        if ((natural) && (!strcmp(natural, "coastline"))) {
            type = "coastline";
        } else {
            type = get_waterway_type(area.get_value_by_key("waterway"));
            //if (!type) {
            //    type = area.get_value_by_key("water");
            //}
            if (!type) {
                type = area.get_value_by_key("landuse");
            }
            if (!type) type = "";
        }
        return type;
    }*/

    static const char *get_name(const osmium::OSMObject &osm_object) {
        const char *name = osm_object.get_value_by_key("name");
        if (!name) name = "";
        return name;
    }

    static int get_lanes(const osmium::OSMObject &osm_object) {
        const char *lanes_chr;
        lanes_chr = osm_object.get_value_by_key("lanes");
        if (lanes_chr) {
            int lanes = atoi(lanes_chr);
            return lanes;
        } else {
            return 0;
        }
    }

    static const bool is_crossing(const osmium::OSMObject &osm_object) {
        const char *footway = osm_object.get_value_by_key("footway");
        if ((footway) && (!strcmp(footway, "crossing"))) {
            return true;
        } else {
            return false;
        }
    }
    
    static const bool is_crossing_point(const osmium::OSMObject &osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        if (!strcmp(highway, "crossing")) {
            return true;
        } else {
            return false;
        }
    }

    
};

#endif /* TAGCHECK_HPP_ */
