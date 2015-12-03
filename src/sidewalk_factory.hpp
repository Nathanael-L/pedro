


#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateFilter.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/CoordinateArraySequenceFactory.h>

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
   
    void construct_parallel_sidewalks(int i, int count_neighbours,
            pair<object_id_type,
            vector<pair<object_id_type, VehicleRoad*>>> node,
            vector<Sidewalk*>& segments, vector<bool>& reverse) {

        object_id_type current = node.first;
        object_id_type neighbour = node.second[i].first;
        int prev_index = (i - 1 + count_neighbours) % count_neighbours;
        object_id_type prev_neighbour = node.second[prev_index].first; 
        VehicleRoad* vehicle_road =  node.second[i].second;
        LineString* left_segment = nullptr;
        LineString* right_segment = nullptr;
        Sidewalk* left_sidewalk = nullptr;
        Sidewalk* right_sidewalk = nullptr;
        string connection = get_connection_string(current, neighbour);
        if (!is_constructed(connection)) {
            left_segment = construct_segment(current, neighbour, left);
            right_segment = construct_segment(current, neighbour, right);
            left_sidewalk = new Sidewalk(left_segment, vehicle_road);
            right_sidewalk = new Sidewalk(right_segment, vehicle_road);
            insert_segments(connection, left_sidewalk, right_sidewalk);
            ds.sidewalk_set.insert(left_sidewalk);
            ds.sidewalk_set.insert(right_sidewalk);
            reverse.push_back(false);
            reverse.push_back(false);
        } else {
            left_sidewalk = ds.finished_segments[connection].second;
            right_sidewalk = ds.finished_segments[connection].first;
            reverse.push_back(true);
            reverse.push_back(true);
        }
        segments.push_back(left_sidewalk);
        segments.push_back(right_sidewalk);
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
        for (auto node : ds.node_map) {
            vector<Sidewalk*> segments;
            vector<bool> reverse;
            segments.clear();
            reverse.clear();
            int count_neighbours = node.second.size();
            for (int i = 0; i < count_neighbours; ++i) {
                construct_parallel_sidewalks(i, count_neighbours, node,
                        segments, reverse);
            }
                
            int count_segments = segments.size();
            if (count_segments > 2) {
                for (int i = 1; i < count_segments; i += 2) {
                    int next_index = (i + 1) % count_segments;
                    bool reverse_first = reverse[i];
                    bool reverse_second = reverse[next_index];
                    connect_segments(segments[i], segments[next_index],
                            reverse_first, reverse_second);
                }
            } else {
                connect_ends(segments[0], segments[1],
                            reverse[0], reverse[1]);
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
