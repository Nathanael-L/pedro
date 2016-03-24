/***
 * crosing_factory.hpp
 *
 *  Created on: Dec 19, 2015
 *      Author: nathanael
 *
 *  There are two types of crossing creations. One is the crossing
 *  indicated by a node within the VehicleRoad. They are created
 *  inside the GeometryConstructor. The other are connections between
 *  every sidewalk side, created within the main function. They get
 *  another weight and can be excluded by the routing algorithm.
 *  Depending on the road type they are marked as a risk crossing
 *  //TODO: set crossing type, test pqgrouting and set weight (distance * 2)
 *
 */


#ifndef CROSSING_FACTORY_HPP_
#define CROSSING_FACTORY_HPP_

class CrossingFactory {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    GeometryFactory geos_factory;
    google::sparse_hash_set<Sidewalk*> new_sidewalk_set;
    const bool left = true;
    const bool right = false;
    const double segment_size = 0.050;

    /***
     * Create new corssing object of given start and end point, with an ID and
     * a length. Insert object into crossing_set.
     */
    void insert_crossing(Point* start, Point* end, Sidewalk* sidewalk,
            string type, string osm_type) {

        Geometry* geometry = nullptr;
        geometry = go.connect_points(start, end);
        CrossingID cid(sidewalk->id, true, 1);
        double length = go.get_length(geometry);
        Crossing* crossing = nullptr;
        crossing = new Crossing(cid, sidewalk->name, geometry, type,
                osm_type, length);
        ds.crossing_set.insert(crossing);
    }

    /* Figure out the start and end point of the crossing depending on the
     * creation direction. Call insert_crossing.
     */
    void create_osm_crossing(Sidewalk*& sidewalk1, Sidewalk*& sidewalk2,
                bool reverse_first, bool reverse_second, string osm_type) {
        
        LineString* segment1 = dynamic_cast<LineString*>(sidewalk1->geometry);
        LineString* segment2 = dynamic_cast<LineString*>(sidewalk2->geometry);
        Point* start_point;
        Point* end_point;
        if (reverse_first) {
            start_point = segment1->getEndPoint();
        } else {
            start_point = segment1->getStartPoint();
        }
        if (reverse_second) {
            end_point = segment2->getEndPoint();
        } else {
            end_point = segment2->getStartPoint();
        }
        insert_crossing(start_point, end_point, sidewalk1, "osm-crossing", osm_type);
    }
    
    /***
     * split_sidewalk takes a Sidewalk object and cut it to the given point.
     * From the split_point till the end a new Sidewalk is created with a new
     * ID. The new Sildewalk is the return value.
     */
    Sidewalk* split_sidewalk(Sidewalk*& sidewalk, Point* split_point, int index) {
        LineString* origin_geometry = dynamic_cast<LineString*>(sidewalk->geometry);
        LineString* first_segment = nullptr;
        LineString* second_segment = nullptr;
        Sidewalk* new_sidewalk = nullptr;
        int num_points = sidewalk->geometry->getNumPoints();
        if (num_points == 2) {
            first_segment = go.set_point(origin_geometry, split_point, true);
            second_segment = go.set_point(origin_geometry, split_point, false);
        } else {
            for (int i = 0; i < num_points - 1; ++i) {
                if (go.point_is_between(split_point,
                        origin_geometry->getPointN(i),
                        origin_geometry->getPointN(i + 1))) {
                    first_segment = go.set_point(origin_geometry, split_point, i + 1);
                    if ((i + 1) < (num_points - 1)) {
                        first_segment = go.cut_line(first_segment, i + 1, true);
                    }
                    second_segment = go.set_point(origin_geometry, split_point, i);
                    if (i > 0) {
                        second_segment = go.cut_line(second_segment, i, false);
                    }
                    break;
                }
            }
        }
        if (first_segment) {
            sidewalk->geometry = first_segment;
        } else {
            cerr << "unexcepted error in split_sidewalk / CrossingFactory"
                << endl;
            exit(1);
        }
        if (second_segment) {
            new_sidewalk = new Sidewalk(sidewalk, second_segment, index);
            new_sidewalk_set.insert(new_sidewalk);
        } else {
            cerr << "unexcepted error in split_sidewalk / CrossingFactory"
                << endl;
            exit(1);
        }
        return new_sidewalk;
    }
    
    /***
     * Segmentize a sidewalk geometry and store the split points into the given
     * vector.
     */
    void segmentize_sidewalk(Sidewalk*& sidewalk, vector<Coordinate>&
            split_points) {

        LineString* linestring = dynamic_cast<LineString*>(
                sidewalk->geometry);
        CoordinateSequence *coords;
        coords = linestring->getCoordinates();
        for (unsigned int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate start = coords->getAt(i);
            Coordinate end = coords->getAt(i + 1);
            if (go.haversine(start, end) < segment_size) {
                continue;
            }
            //Point* end_point = geos_factory.createPoint(end);
            split_points = go.segmentize(start, end, segment_size);
            int index = 2;
            for (Coordinate coord : split_points) {
                Point* seg_point = geos_factory.createPoint(coord);
                sidewalk = split_sidewalk(sidewalk, seg_point, index);   
                index++;
            }
        }
    }

public:

    explicit CrossingFactory(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {

        new_sidewalk_set.set_deleted_key(nullptr);
    }

    /***
     * Create OSM crossing of two parallel sidewalks, called by
     * GeometryConstructor.
     */
    void generate_osm_crossing(vector<Sidewalk*>& segments,
            vector<bool>& reverse, string type) {

        int count_segments = segments.size();
        if (count_segments == 4) {
            if ((segments[0]) && (segments[1])) {
                create_osm_crossing(segments[0], segments[1], reverse[0],
                        reverse[1], type);
            }
        }
    }

    /***
     * Each road can be crossed beside official crossings. To realize the
     * crossing behavior each sidewalk is segmentized by segment_size.
     * Crossing of larger roads is marked as a risk crossing.
     */
    void generate_frequent_crossings() {
        google::sparse_hash_set<string> segmentized_sidewalks;
        segmentized_sidewalks.set_deleted_key("");
        vector<Coordinate> sidewalk_splits;
        vector<Coordinate> neighbour_splits;
        for (auto map_entry : ds.sidewalk_map) {
            string sidewalk_id = map_entry.first;
            Sidewalk* sidewalk = map_entry.second;
            if (segmentized_sidewalks.find(sidewalk_id) !=
                    segmentized_sidewalks.end()) {
                continue;
            }
            string neighbour_id = sidewalk->get_neighbour_id();
            auto neighbour_pair = ds.sidewalk_map.find(neighbour_id);
            if (neighbour_pair == ds.sidewalk_map.end()) {
                continue;
            }
            Sidewalk* neighbour = neighbour_pair->second;
            sidewalk_splits.clear();
            neighbour_splits.clear();
            segmentize_sidewalk(sidewalk, sidewalk_splits);
            segmentize_sidewalk(neighbour, neighbour_splits);
            int sidewalk_split_size = sidewalk_splits.size();
            int neighbour_split_size = neighbour_splits.size();
            if ((sidewalk_split_size != 0) && (neighbour_split_size != 0)) {
                int min_size = min(sidewalk_split_size, neighbour_split_size);
                for (int i = 0; i < min_size; ++i) {
                    Point* start_point = geos_factory.createPoint(
                            sidewalk_splits[i]);
                    Point* end_point = geos_factory.createPoint(
                            neighbour_splits[i]);
                    string crossing_type =
                            TagCheck::get_frequent_crossing_type(
                            sidewalk->at_osm_type);
                    insert_crossing(start_point, end_point, sidewalk,
                            crossing_type, "");
                }
            }
            segmentized_sidewalks.insert(sidewalk_id);
            segmentized_sidewalks.insert(neighbour_id);
        }
        for (Sidewalk* new_sidewalk : new_sidewalk_set) {
            ds.sidewalk_map[new_sidewalk->id] = new_sidewalk;
        }
    }
};

#endif /* CROSSING_FACTORY_HPP_ */
