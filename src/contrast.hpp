/***
 * contrast.hpp
 *
 *  Created on: Dec 10, 2015
 *      Author: nathanael
 */

#ifndef CONTRAST_HPP_
#define CONTRAST_HPP_

typedef pair<Geometry*, double> ortho_pair_type;
typedef google::sparse_hash_set<Sidewalk*> detect_set_type;

class Contrast {
    DataStorage& ds;
    GeomOperate go;
    GeometryFactory geometry_factory;

    //PARAMETERS
    const double segment_size = 0.01; // distance between orthogonal lines
    const double ortho_length = 0.015; // length of the orthogonal line
    const double contrast_factor = 0.8; // if 80% of the segments intersects with the sidewalk

    void insert_in_tree(ortho_pair_type* ortho_pair) {
        ds.ortho_tree.insert(ortho_pair->first->getEnvelopeInternal(), ortho_pair);
    }

    vector<Coordinate> segmentize(Coordinate start, Coordinate end,
            double fraction_length) {

        vector<Coordinate> splits;
        LineSegment segment(start, end);
        double length = go.haversine(start.x, start.y, end.x, end.y);
        double fraction = fraction_length / length;
        double position = 0;
        do {
            Coordinate new_coord;
            segment.pointAlong(position, new_coord);
            splits.push_back(new_coord);
            position += fraction;
        } while (position < 1);
        return splits;
    }

    /***
     * get distance between centroid of othogonal segment and sidewalk geometry
     */
    double get_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        Point* ortho_mid = ortho_pair->first->getCentroid();
        const Point* c_ortho_mid = const_cast<const Point*>(ortho_mid);//->getCoordinate();
        double distance = sidewalk->distance(c_ortho_mid);
        return distance;
    }

    /***
     * clue the shortest intersection distance to the orthogonal line
     */
    void set_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        double distance = get_distance(sidewalk, ortho_pair);
        if (distance < ortho_pair->second) {
            ortho_pair->second = distance;
        }
    }

    bool is_shortest_distance(Geometry* sidewalk, ortho_pair_type* ortho_pair) {
        double tolerance = 0.0000005; // ca. 5cm
        double current_distance = get_distance(sidewalk, ortho_pair);
        double shortest_distance = ortho_pair->second;
        if (abs(current_distance - shortest_distance) < tolerance) {
            return true;
        }
        return false;
    }

    double compare_to_length(int count_intersects, double length) {
        double count_segments = length / segment_size;
        /*int count_segments = static_cast<int>(floor(length / segment_size));
        if (count_segments == 0) {
            count_segments = 1;
        }*/
        double ratio = static_cast<double>(count_intersects) / count_segments;
        return ratio;
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
                    const Geometry* ortho_line = const_cast<const Geometry*>(
                            ortho_pair->first);
                    if (sidewalk->intersects(ortho_line)) {
                        if (is_shortest_distance(sidewalk, ortho_pair)) {
                            count_intersects++;
                        }
                    }
                }
                double ratio = compare_to_length(count_intersects,
                        sidewalk_road->length);
                if (ratio > contrast_factor) {
                    ds.sidewalk_set.erase(sidewalk_road);
                }
            }
        }
    }

    /***
     * Checks sidewalks intesecting orthogonals. For each sidewalk the number
     * of intersects are count. Is the number compared to the length of the
     * linestring higher than the constrast factor, the sidewalk is stored in
     * the detect map. The distance of the closest sidewalk to the centroid of
     * the orthogonal is clued to the orthogonal.
     * TODO only accept angles close to 90 degrees.
     */
    void find_possible_positives(detect_set_type& detect_set) {
        for (auto sidewalk_road : ds.sidewalk_set) {
            vector<void *> results;
            Geometry* sidewalk = sidewalk_road->geometry;
            ds.ortho_tree.query(sidewalk->getEnvelopeInternal(), results);
            if (results.size() > 1) {
                int count_intersects = 0;
                Geometry* ortho_line = nullptr;
                for (auto result : results) {
                    ortho_pair_type* ortho_pair = static_cast<ortho_pair_type*>(result);
                    const Geometry* ortho_line = const_cast<const Geometry*>(
                            ortho_pair->first);
                    if (sidewalk->intersects(ortho_line)) {
                        set_distance(sidewalk, ortho_pair);
                        count_intersects++;
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
        for (int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate start = coords->getAt(i);
            Coordinate end = coords->getAt(i + 1);
            Point* end_point = geometry_factory.createPoint(end);
            for (Coordinate coord : segmentize(start, end, segment_size)) {
                Point* seg_point = geometry_factory.createPoint(coord);
                double closest_intersection_distance = 1;
                LineString* ortho_line = go.orthogonal_line(seg_point,
                        end_point, ortho_length);
                ortho_pair_type* ortho_pair = new ortho_pair_type(
                        ortho_line, closest_intersection_distance);
                insert_in_tree(ortho_pair);
                //debug
                ds.insert_orthos(ortho_line);
            }
        }
    }

    void check_sidewalks() {
        detect_set_type detect_set;
        detect_set.set_deleted_key(nullptr);
        find_possible_positives(detect_set);
        find_closest_positives(detect_set);

        for (auto detected_sidewalk : detect_set) {
            double r = 0.7;
            ds.insert_intersect(detected_sidewalk->geometry,
                    detected_sidewalk->length, r);
        }
    }
};

#endif /* CONTRAST_HPP_ */
