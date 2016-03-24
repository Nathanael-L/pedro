/***
 * contrast.hpp
 *
 *  Created on: Dec 10, 2015
 *      Author: nathanael
 *
 *  In Contrast the OSM Ways are compared to the constructed sidewalks.
 *  If both have a similar geometrie, the assumtion is, that the
 *  constructed geometry is wrong.
 *
 */

#ifndef CONTRAST_HPP_
#define CONTRAST_HPP_

typedef pair<Geometry*, double> ortho_pair_type;
typedef google::sparse_hash_set<Sidewalk*> detect_set_type;

class Contrast {
    DataStorage& ds;
    GeomOperate go;
    GeometryFactory geos_factory;

    //PARAMETERS
    const double segment_size = 0.01;        // distance between orthogonal
                                             // lines
    const double ortho_length = 0.015;       // length of the orthogonal
                                             // line
    const double contrast_factor = 0.7;      // if 80% of the segments
                                             // intersects
                                             // with the sidewalk
    const double orientation_tolerance = 15; // tolerance in degrees, for the
                                             // orientation of sidewalk and
                                             // pedestrian road
    const double min_length = 0.05;          // minimal length of used
                                             // PedestrianRoad

    /***
     * Insert the pair of orthogonals into a tree structure to improve the
     * query time.
     */
    void insert_in_tree(ortho_pair_type* ortho_pair) {
        ds.ortho_tree.insert(ortho_pair->first->getEnvelopeInternal(),
                ortho_pair);
    }

    /***
     * Get distance between centroid of othogonal segment and sidewalk geometry.
     */
    double get_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        Point* ortho_mid = ortho_pair->first->getCentroid();
        const Point* c_ortho_mid = const_cast<const Point*>(ortho_mid);
        double distance = sidewalk->distance(c_ortho_mid);
        return distance;
    }

    /***
     * Clue the shortest intersection distance to the orthogonal line.
     */
    void set_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        double distance = get_distance(sidewalk, ortho_pair);
        if (distance < ortho_pair->second) {
            ortho_pair->second = distance;
        }
    }

    /***
     * Compare the current distance to the stored distance with the orthogonal.
     */
    bool is_shortest_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        double tolerance = 0.0000005; // ca. 5cm
        double current_distance = get_distance(sidewalk, ortho_pair);
        double shortest_distance = ortho_pair->second;
        if (abs(current_distance - shortest_distance) < tolerance) {
            return true;
        }
        return false;
    }

    /***
     * Compare the amount of intersections with the length of the segment.
     */
    double compare_to_length(int count_intersects, double length) {
        double count_segments = length / segment_size;
        double ratio = static_cast<double>(count_intersects) / count_segments;
        return ratio;
    }

    /***
     * Test if the orientation of the sidewalk and the orthogonals are close to
     * 90 degrees (orientation_toleance)
     */
    bool similar_orientation(Geometry* sidewalk, Geometry* ortho_line) {
        double angle = go.angle(dynamic_cast<LineString*>(sidewalk),
                dynamic_cast<LineString*>(ortho_line));
        if (angle > 180) {
            angle -= 180;
        }
        if ((angle > 90 - orientation_tolerance) &&
                (angle < 90 + orientation_tolerance)) {
            return true;
        }
        return false;
    }

    /***
     * Checks sidewalks intesecting orthogonals with similar orientation
     * (= 90 degree +- orientation_toleance). For each sidewalk the number
     * of intersects are count. Is the number compared to the length of the
     * linestring higher than the constrast factor, the sidewalk is stored in
     * the detect map. The distance of the closest sidewalk to the centroid of
     * the orthogonal is clued to the orthogonal.
     */
    void find_possible_positives(detect_set_type& detect_set) {
        for (auto map_entry : ds.sidewalk_map) {
            Sidewalk* sidewalk_road = map_entry.second;
            vector<void *> results;
            Geometry* sidewalk = sidewalk_road->geometry;
            ds.ortho_tree.query(sidewalk->getEnvelopeInternal(), results);
            if (results.size() > 1) {
                int count_intersects = 0;
                Geometry* ortho_line = nullptr;
                for (auto result : results) {
                    ortho_pair_type* ortho_pair =
                            static_cast<ortho_pair_type*>(result);
                    ortho_line = ortho_pair->first;
                    if (sidewalk->intersects(
                            const_cast<const Geometry*>(ortho_line))) {
                        if (similar_orientation(sidewalk, ortho_line)) {
                            set_distance(sidewalk, ortho_pair);
                            count_intersects++;
                        }
                    }
                }
                double ratio = compare_to_length(count_intersects,
                        sidewalk_road->length);
                if (ratio > contrast_factor) {
                    string sidewalk_id = sidewalk_road->id;
                    detect_set.insert(sidewalk_road);
                }
            }
        }
    }

    /***
     * Does the same as find_possible_positives, but iterates over the
     * detect_set and the distance to the orthogonal is the shortest.
     */
    void find_closest_positives(detect_set_type& detect_set) {
        for (auto sidewalk_road : detect_set) {
            vector<void *> results;
            Geometry* sidewalk = sidewalk_road->geometry;
            ds.ortho_tree.query(sidewalk->getEnvelopeInternal(), results);
            if (results.size() > 1) {
                int count_intersects = 0;
                Geometry* ortho_line = nullptr;
                for (auto result : results) {
                    ortho_pair_type* ortho_pair = static_cast<ortho_pair_type*>(result);
                    ortho_line = ortho_pair->first;
                    if (sidewalk->intersects(const_cast<const Geometry*>(ortho_line))) {
                        if (similar_orientation(sidewalk, ortho_line)) {
                            if (is_shortest_distance(sidewalk, ortho_pair)) {
                                count_intersects++;
                            }
                        }
                    }
                }
                double ratio = compare_to_length(count_intersects,
                        sidewalk_road->length);
                if (ratio > contrast_factor) {
                    string sidewalk_id = sidewalk_road->id;
                    ds.sidewalk_map.erase(sidewalk_id);
                }
            }
        }
    }

public:

    explicit Contrast(DataStorage& data_storage) :
            ds(data_storage) {
    }
    
    /***
     * Segmentizes the PedestrianRoad and creates orthogonals each
     * segment_size.
     */
    void create_orthogonals(Geometry* geometry) {
        LineString* linestring = dynamic_cast<LineString*>(geometry);
        CoordinateSequence *coords;
        coords = linestring->getCoordinates();
        for (unsigned int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate start = coords->getAt(i);
            Coordinate end = coords->getAt(i + 1);
            Point* end_point = geos_factory.createPoint(end);
            if (go.haversine(start, end) < min_length) {
                continue;
            }
            for (Coordinate coord : go.segmentize(start, end, segment_size)) {
                Point* seg_point = geos_factory.createPoint(coord);
                double closest_intersection_distance = 1;
                LineString* ortho_line = go.orthogonal_line(seg_point,
                        end_point, ortho_length);
                ortho_pair_type* ortho_pair = new ortho_pair_type(
                        ortho_line, closest_intersection_distance);
                insert_in_tree(ortho_pair);
                //debug
                //ds.insert_orthos(ortho_line);
            }
        }
    }

    /***
     * Run constrast algorithmn. First find all possible positives - all
     * intersections of the orthogonals. The second step is to find only
     * the sidewalks with the closest intersections and erase them.
     */ 
    void check_sidewalks() {
        detect_set_type detect_set;
        detect_set.set_deleted_key(nullptr);
        find_possible_positives(detect_set);
        find_closest_positives(detect_set);

        for (auto detected_sidewalk : detect_set) {
            double r = 0.7;
            //debug
            ds.insert_intersect(detected_sidewalk->geometry,
                    detected_sidewalk->length, r);
                    
        }
    }
};

#endif /* CONTRAST_HPP_ */
