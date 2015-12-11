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
        cout << "length" << length << endl;
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
        cout << length << " / " << segment_size << endl;
        double count_segments = length / segment_size;
        /*int count_segments = static_cast<int>(floor(length / segment_size));
        if (count_segments == 0) {
            count_segments = 1;
        }*/
        double ratio = static_cast<double>(count_intersects) / count_segments;
        cout << "ratio: " << ratio << endl;
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

    void check_sidewalks() {
        cout << "count sidewalks: " << ds.sidewalk_set.size() << endl;
        for (auto sidewalk_road : ds.sidewalk_set) {
            vector<void *> results;
            Geometry* sidewalk = sidewalk_road->geometry;
            ds.ortho_tree.query(sidewalk->getEnvelopeInternal(), results);
            if (results.size() > 0) {
                int count_intersects = 0;
                for (auto result : results) {
                    Geometry* ortho_line = static_cast<Geometry*>(result);
                    if (sidewalk->intersects(const_cast<const
                            Geometry*>(ortho_line))) {
                        count_intersects++;
                    }
                }
                double ratio = compare_to_length(count_intersects,
                        sidewalk_road->length);
                if (ratio > contrast_factor) {
                    ds.insert_intersect(sidewalk, sidewalk_road->length, ratio);
                }
            }
        }
    }

};

#endif /* CONTRAST_HPP_ */
