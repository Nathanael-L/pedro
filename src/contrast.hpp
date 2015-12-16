/***
 * contrast.hpp
 *
 *  Created on: Dec 10, 2015
 *      Author: nathanael
 */

#ifndef CONTRAST_HPP_
#define CONTRAST_HPP_

class Contrast {
    DataStorage& ds;
    GeomOperate go;
    GeometryFactory geometry_factory;

    //PARAMETERS
    const double segment_size = 0.01; // distance between orthogonal lines
    const double ortho_length = 0.015; // length of the orthogonal line
    const double contrast_factor = 0.8; // if 80% of the segments intersects with the sidewalk

    void insert_in_tree(Geometry* geometry) {
        ds.ortho_tree.insert(geometry->getEnvelopeInternal(), geometry);
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

    double compare_to_length(int count_intersects, double length) {
        double count_segments = length / segment_size;
        /*int count_segments = static_cast<int>(floor(length / segment_size));
        if (count_segments == 0) {
            count_segments = 1;
        }*/
        double ratio = static_cast<double>(count_intersects) / count_segments;
        return ratio;
    }

public:

    explicit Contrast(DataStorage& data_storage) :
            ds(data_storage) {
    }
    
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
                LineString* ortho_line = go.orthogonal_line(seg_point,
                        end_point, ortho_length);
                insert_in_tree(ortho_line);
                ds.insert_orthos(ortho_line);
            }
        }
    }

    /***
     *
     * Look up for neighbour sidewalk in ds.finished_connections, if there is,
     * it compares the distance to the mid of the orthogonal.
     * Return true if input is the closer sidewalk or if no neighbour exists.
     * BUG: if just one sidewalk is on the farer side.
     *
     */
    int closer_sidewalk(Geometry* sidewalk, Geometry* neighbour,
            vector<void*>& orthos) {

        Geometry* ortho = static_cast<Geometry*>(orthos[0]); //could check more than one
        LineString* ortho_line = dynamic_cast<LineString*>(ortho);
        const Coordinate* ortho_mid = ortho_line->getCentroid()->getCoordinate();
        double ortho_lon = ortho_mid->x;
        Geometry* sidewalk_intersection = ortho->intersection(sidewalk);
        Geometry* neighbour_intersection = ortho->intersection(neighbour);
        double sidewalk_lon = dynamic_cast<Point*>(sidewalk_intersection)->getCoordinate()->x;
        double neighbour_lon = dynamic_cast<Point*>(neighbour_intersection)->getCoordinate()->x;
        double distance_sidewalk = abs(sidewalk_lon - ortho_lon);
        double distance_neighbour = abs(sidewalk_lon - ortho_lon);
        if (distance_sidewalk > distance_neighbour) {
            return 0;
        }
        return 1;
    }

    void check_sidewalks() {
        google::sparse_hash_map<string, vector<Sidewalk*>> detect_map;
        detect_map.set_deleted_key("");
        for (auto sidewalk_road : ds.sidewalk_set) {
            vector<void *> results;
            Geometry* sidewalk = sidewalk_road->geometry;
            ds.ortho_tree.query(sidewalk->getEnvelopeInternal(), results);
            if (results.size() > 1) {
                int count_intersects = 0;
                Geometry* ortho_line = nullptr;
                for (auto result : results) {
                    ortho_line = static_cast<Geometry*>(result);
                    if (sidewalk->intersects(const_cast<const
                            Geometry*>(ortho_line))) {
                        count_intersects++;
                    }
                }
                double ratio = compare_to_length(count_intersects,
                        sidewalk_road->length);
                if (ratio > contrast_factor) {
                    string osm_id = sidewalk_road->osm_id;
                    detect_map[osm_id].push_back(sidewalk_road);
                    if (detect_map[osm_id].size() > 1) {
                        Geometry* old_sidewalk = detect_map[osm_id][0]->geometry;
                        Geometry* new_sidewalk = detect_map[osm_id][1]->geometry;
                        // could BUG (only one ortholine is taken)
                        // altenative idea: store for every ortho the distance to intersection
                        // and check twice
                        detect_map[osm_id].erase(detect_map[osm_id].begin() +
                            closer_sidewalk(old_sidewalk, new_sidewalk, results));
                    }
                }
            }
        }
        for (auto map_entry : detect_map) {
            Sidewalk* detected = map_entry.second[0];
            double r = 0.7;
            ds.insert_intersect(detected->geometry, detected->length, r);
        }
    }
};

#endif /* CONTRAST_HPP_ */
