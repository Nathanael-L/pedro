


class SidewalkFactory {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    GeometryFactory geos_factory;
    const bool left = true;
    const bool right = false;

    string get_connection_string(object_id_type node1,
            object_id_type node2) {

        object_id_type small_id = min(node1, node2);
        object_id_type big_id = max(node1, node2);
        string connection_string;
        connection_string = to_string(small_id) + to_string(big_id);
        return connection_string;
    }
 
    bool is_constructed(string connection_string) {

        if (ds.finished_segments.find(connection_string)
	        == ds.finished_segments.end()) {
            return false;
        }
        return true;
    }

    void insert_segments(string connection_string, Sidewalk* segment1,
            Sidewalk* segment2) {

        ds.finished_segments[connection_string] =
                pair<Sidewalk*, Sidewalk*>(segment1, segment2);
    }

    bool is_convex(LineString* segment1, LineString* segment2,
            bool reverse_first, bool reverse_second) {

        LineString* from = nullptr;
        LineString* to = nullptr;
        from = (reverse_first ? dynamic_cast<LineString*>(segment1->reverse())
                : segment1),
        to = (reverse_second ? dynamic_cast<LineString*>(segment2->reverse())
                : segment2);
        double angle = go.angle(from, to);
        if (angle > 180) {
            return true;
        }
        return false;
    }

    bool exists(VehicleMapValue map_value, bool left) {
        switch (map_value.vehicle_road->sidewalk) {
            case 'b':
                return true;
            case 'n':
                return false;
            case 'l':
                if (left) {
                    return map_value.forward;
                } else {
                    return !map_value.forward;
                }
            case 'r':
                if (!left) {
                    return map_value.forward;
                } else {
                    return !map_value.forward;
                }
        }
        return false;
    }
   
    Sidewalk *construct_parallel_sidewalk(int i, int count_neighbours,
            pair<object_id_type, vector<VehicleMapValue>> node,
            vector<Sidewalk*>& segments, vector<bool>& reverse,
            bool left) {

        object_id_type current = node.first;
        object_id_type neighbour = node.second[i].node_id;
        //int prev_index = (i - 1 + count_neighbours) % count_neighbours;
        //object_id_type prev_neighbour = node.second[prev_index].node_id; 
        VehicleRoad* vehicle_road =  node.second[i].vehicle_road;
        LineString* segment = nullptr;
        Sidewalk* sidewalk = nullptr;
        string connection = get_connection_string(current, neighbour);
        if (!is_constructed(connection)) {
            segment = construct_segment(current, neighbour, left);
            sidewalk = new Sidewalk(segment, vehicle_road);
            ds.sidewalk_set.insert(sidewalk);
            reverse.push_back(false);
        } else {
            if (left) {
                sidewalk = ds.finished_segments[connection].second;
            } else {
                sidewalk = ds.finished_segments[connection].first;
            }
            reverse.push_back(true);
        }
        return sidewalk;
    }

public:

    OGRGeometry *test;

    explicit SidewalkFactory(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    LineString* construct_segment(object_id_type current,
            object_id_type neighbour, bool left) {

        Location current_location;
        Location neighbour_location;
        current_location = location_handler.get_node_location(current);
        neighbour_location = location_handler.get_node_location(neighbour);
        LineString *segment = nullptr;

        segment = go.parallel_line(current_location, neighbour_location,
                0.003, left);
        //right_sidewalk = go.parallel_line(current_location, neighbour_location,
        //        0.003, left);
        return segment;
    }


    LineString *insert_point(LineString*& segment, Geometry* point,
            bool reverse) {

        CoordinateSequence *coords;
        coords = segment->getCoordinates();
        const Coordinate *new_coordinate;
        new_coordinate = dynamic_cast<Point*>(point)->getCoordinate();
        int position = (reverse ? coords->getSize() : 0);
        coords->add(position, *new_coordinate, true);
        return geos_factory.createLineString(coords);
    }

    LineString *set_point(LineString*& segment, Geometry* point, bool reverse) {
        CoordinateSequence *coords;
        coords = segment->getCoordinates();
        const Coordinate *new_coordinate;
        new_coordinate = dynamic_cast<Point*>(point)->getCoordinate();
        int position = (reverse ? coords->getSize() - 1 : 0);
        coords->setAt(*new_coordinate, position);
        return geos_factory.createLineString(coords);
    }

    void connect_segments(Sidewalk*& sidewalk1, Sidewalk*& sidewalk2,
            bool reverse_first, bool reverse_second) {

        LineString* segment1 = dynamic_cast<LineString*>(sidewalk1->geometry);
        LineString* segment2 = dynamic_cast<LineString*>(sidewalk2->geometry);
        if (is_convex(segment1, segment2, reverse_first,
                reverse_second)) {
            Point* connector;
            if (reverse_second) {
                connector = segment2->getEndPoint();
            } else {
                connector = segment2->getStartPoint();
            }
            segment1 = insert_point(segment1, connector, reverse_first);
            //cout << "3, 2:" << endl << segment1->toString() << endl << segment2->toString() << endl;
        } else if (segment1->intersects(segment2)) {
            Geometry* intersector;
            intersector = segment1->intersection(segment2);
            if (intersector->getGeometryType() == "Point") {
                segment1 = set_point(segment1, intersector, reverse_first);   
                segment2 = set_point(segment2, intersector, reverse_second);
            }
            //cout << "3, 3:" << endl << segment1->toString() << endl << segment2->toString() << endl;

        } else {
            Point* mean;
            mean = ((reverse_first ? segment1->getEndPoint() :
                    segment1->getStartPoint()),
                    (reverse_second ? segment2->getEndPoint() :
                    segment2->getStartPoint()));
            segment1 = set_point(segment1, mean, reverse_first);
            segment2 = set_point(segment2, mean, reverse_second);
            //cout << "2, 2:" << endl << segment1->toString() << endl << segment2->toString() << endl;
        }
        sidewalk1->geometry = segment1;
        sidewalk2->geometry = segment2;
    }

    void connect_ends(Sidewalk*& sidewalk1, Sidewalk*& sidewalk2,
            bool reverse_first, bool reverse_second) {
    
        LineString* segment1 = dynamic_cast<LineString*>(sidewalk1->geometry);
        LineString* segment2 = dynamic_cast<LineString*>(sidewalk2->geometry);
        Point* connector;
        if (reverse_second) {
            connector = segment2->getEndPoint();
        } else {
            connector = segment2->getStartPoint();
        }
        segment1 = insert_point(segment1, connector, (reverse_first ? 2 : 0));
        sidewalk1->geometry = segment1;
    }
    

    void generate_sidewalks() {
        for (auto node : ds.vehicle_node_map) {
            vector<Sidewalk*> segments;
            vector<bool> reverse;
            segments.clear();
            reverse.clear();
            int count_neighbours = node.second.size();
            for (int i = 0; i < count_neighbours; ++i) {
                object_id_type current = node.first;
                object_id_type neighbour = node.second[i].node_id;
                string connection = get_connection_string(current, neighbour);
                Sidewalk* left_sidewalk = nullptr;
                Sidewalk* right_sidewalk = nullptr;
                if (exists(node.second[i], left)) {
                    left_sidewalk = construct_parallel_sidewalk(i, count_neighbours, node,
                            segments, reverse, left);
                } else {
                    reverse.push_back(false);
                }
                if (exists(node.second[i], right)) {
                    right_sidewalk = construct_parallel_sidewalk(i, count_neighbours, node,
                            segments, reverse, right);
                } else {
                    reverse.push_back(false);
                }
                segments.push_back(left_sidewalk);
                segments.push_back(right_sidewalk);
                insert_segments(connection, left_sidewalk, right_sidewalk);
            }
                
            int count_segments = segments.size();
            if (count_segments > 2) {
                for (int i = 1; i < count_segments; i += 2) {
                    int next_index = (i + 1) % count_segments;
                    if ((segments[i]) && (segments[next_index])) {
                        bool reverse_first = reverse[i];
                        bool reverse_second = reverse[next_index];
                        connect_segments(segments[i], segments[next_index],
                                reverse_first, reverse_second);
                    }
                }
            } else {
                if ((segments[0]) && (segments[1])) {
                    connect_ends(segments[0], segments[1],
                            reverse[0], reverse[1]);
                }
            }
        }
        //ds.union_sidewalk_geometries();
        //intersect_segments(ds.geos_sidewalk_net);
    }
};
/*
    void intersect_segments(const Geometry *net) {
        Geometry *intersector;
        geos::geom::CoordinateArraySequenceFactory coord_factory;
        for (auto segment1 : ds.sidewalk_geometries) {
            for (auto segment2 : ds.sidewalk_geometries) {
                if (segment1->intersects(segment2)) {
                    intersector = segment1->intersection(segment2);
                    Geometry *new_line1, *new_line2;
                    CoordinateSequence *coords1;
                    coords1 = segment1->getCoordinates();
                    if (intersector->getGeometryType() == "Point") {
                        const Coordinate *new_coordinate;
                        new_coordinate = dynamic_cast<Point*>(intersector)->getCoordinate();
                        coords1->setAt(*new_coordinate, 1);
                    }
                }
            }
        }
    }
*/
