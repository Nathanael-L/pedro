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

    /*static const char* get_highway_type(const char* raw_type) {
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

    static bool char_in_list(const char* pattern,
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

    static bool is_highway(const osmium::OSMObject& osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        if (highway) {
            return true;
        }
        return false;
    }

    static bool is_vehicle(const osmium::OSMObject& osm_object) {
        if (is_polygon(osm_object)) {
            return false;
        }
        const char* highway = osm_object.get_value_by_key("highway");
        /* ALL 
        string type_list[] = {
            "motorway", "trunk", "primary", "secondary", "tertiary",
            "unclassified", "road", "residential", "service",        
            "motorway_link", "trunk_link", "primary_link", "secondary_link",
            "tertiary_link", "bus_guideway", "unclassified"
        };
        */
        string type_list[] = {
            // "motorway",
            // "trunk",
            "primary",
            "secondary",
            "tertiary",
            "unclassified",
            // "road",
            "residential",
            "service",        
            // "motorway_link",
            // "trunk_link",
            "primary_link",
            "secondary_link",
            "tertiary_link",
            "bus_guideway",
            "unclassified"
        };
        vector<string> type_vector;
        for (string item : type_list) {
            type_vector.push_back(item);
        }
        return (char_in_list(highway, type_vector));
    }

    static bool is_pedestrian(const osmium::OSMObject& osm_object) {
        if (is_polygon(osm_object)) {
            return false;
        }
        const char* highway = osm_object.get_value_by_key("highway");
        string type_list[] = {
            "pedestrian",
            "footway",
            "steps",
            "path",
            "track",
            "living_street"
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

    static bool is_tunnel(const osmium::OSMObject& osm_object) {
        const char* tunnel = osm_object.get_value_by_key("tunnel");
        if ((tunnel) && (!strcmp(tunnel, "yes"))) {
            return true;
        } else {
            return false;
        }
    }

    static bool is_bridge(const osmium::OSMObject& osm_object) {
        const char* bridge = osm_object.get_value_by_key("bridge");
        if ((bridge) && (!strcmp(bridge, "yes"))) {
            return true;
        } else {
            return false;
        }
    }

    static bool is_polygon(const osmium::OSMObject& osm_object) {
        //more polygons?
        const char* area = osm_object.get_value_by_key("area");
        if (!area) {
            return false;
        }
        if (strcmp(area, "yes")) {
            return true;
        }
        return false;
    }

    static bool is_crossing(const osmium::OSMObject& osm_object) {
        const char* footway = osm_object.get_value_by_key("footway");
        if ((footway) && (!strcmp(footway, "crossing"))) {
            return true;
        } else {
            return false;
        }
    }

    static bool node_is_crossing(const osmium::OSMObject& osm_object) {
        const char* highway = osm_object.get_value_by_key("highway");
        if ((highway) && (!strcmp(highway, "crossing"))) {
            return true;
        } else {
            return false;
        }
    }

    static const char* get_name(const osmium::OSMObject& osm_object) {
        const char* name = osm_object.get_value_by_key("name");
        if (!name) name = "";
        return name;
    }

    static const char* get_highway_type(const osmium::OSMObject& osm_object) {
        return osm_object.get_value_by_key("highway");
    }

    static char get_sidewalk_type(const osmium::OSMObject& osm_object) {
        const char* sidewalk = osm_object.get_value_by_key("sidewalk");
        char sidewalk_chr = 'b';
        if (sidewalk) {
            if ((!strcmp(sidewalk, "none")) || (!strcmp(sidewalk, "no"))) {
                sidewalk_chr = 'n';
            } else if (!strcmp(sidewalk, "right")) {
                sidewalk_chr = 'r';
            } else if (!strcmp(sidewalk, "left")) {
                sidewalk_chr = 'l';
            }
        }
        return sidewalk_chr;
    }

    static const char* get_crossing_type(const OSMObject& osm_object) {
        const char* crossing = osm_object.get_value_by_key("crossing");
        if (crossing) {
            return crossing;
        }
        return "";
    }
    
    static int get_lanes(const osmium::OSMObject& osm_object) {
        const char* lanes_chr;
        lanes_chr = osm_object.get_value_by_key("lanes");
        if (lanes_chr) {
            int lanes = atoi(lanes_chr);
            return lanes;
        } else {
            return 0;
        }
    }

    static string get_frequent_crossing_type(string osm_type/*, int lanes*/) {
        string risk_list[] = {
            "primary",
            "primary_link",
        };
        vector<string> risk_vector;
        for (string item : risk_list) {
            risk_vector.push_back(item);
        }
        /* NOT DONE: Sidewalks do not know their lanes.
        if ((lanes > 1) || (char_in_list("osm_type"))) {
            return "risk-crossing";
        }
        */
        if (char_in_list(osm_type.c_str(), risk_vector)) {
            return "risk-crossing";
        }
        return "frequent-crossing";
    }
};

#endif /* TAGCHECK_HPP_ */
