/***
 * sidewalk_factory.hpp
 *
 *  Created on: Nov 24, 2015
 *      Author: nathanael
 *  
 *  The SidewalkFactory creates the sidewalks for every VehicleRoad
 *  connection stored in the vehicle_node_map.
 *
 */


#ifndef SIDEWALK_FACTORY_HPP_
#define SIDEWALK_FACTORY_HPP_

class SidewalkFactory {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    GeometryFactory geos_factory;
    const bool left = true;
    const bool right = false;

    /***
     * Concatenating two OSM ID to identicate a connection.
     */
    string get_connection_string(object_id_type node1,
            object_id_type node2) {

        object_id_type small_id = min(node1, node2);
        object_id_type big_id = max(node1, node2);
        string connection_string;
        connection_string = to_string(small_id) + to_string(big_id);
        return connection_string;
    }
 
    /***
     * Test if connection is allready constructed.
     */
    bool is_constructed(string connection_string) {

        if (ds.finished_segments.find(connection_string)
	        == ds.finished_segments.end()) {
            return false;
        }
        return true;
    }

    /***
     * Segments are temporaly stored in a map.
     */
    void insert_segments(string connection_string, Sidewalk* segment1,
            Sidewalk* segment2) {

        ds.finished_segments[connection_string] =
                pair<Sidewalk*, Sidewalk*>(segment1, segment2);
    }

    /***
     * Test if sidewalk segment is on th covex side of the curve using the
     * angle. Because of the precision of double, excact 180 degrees are
     * hardly possible.
     *
     */
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

    /***
     * Test if the sidewalk exists on the assumption.
     */
    bool sidewalk_exists(VehicleMapValue map_value, bool left) {
        switch (map_value.vehicle_road->sidewalk) {
            case 'b':
                return true;
            case 'n':
                return false;
            case 'l':
                if (left) {
                    return map_value.is_foreward;
                } else {
                    return !map_value.is_foreward;
                }
            case 'r':
                if (!left) {
                    return map_value.is_foreward;
                } else {
                    return !map_value.is_foreward;
                }
        }
        return false;
    }
   
    /***
     * Create sidewalk at one side between two VehicleRoad nodes.
     */
    Sidewalk *construct_parallel_sidewalk(int i, int count_neighbours,
            object_id_type node_id, vector<VehicleMapValue> neighbours,
            vector<Sidewalk*>& segments, vector<bool>& reverse,
            bool left) {

        object_id_type current_id = node_id;
        object_id_type neighbour_id = neighbours[i].node_id;
        VehicleRoad* vehicle_road =  neighbours[i].vehicle_road;
        LineString* segment = nullptr;
        Sidewalk* sidewalk = nullptr;
        string connection = get_connection_string(current_id, neighbour_id);
        if (!is_constructed(connection)) {
            segment = construct_segment(current_id, neighbour_id, left);
            SidewalkID sid(neighbours[i].from, 
                    neighbours[i].to, left, 1);
            sidewalk = new Sidewalk(sid, segment, vehicle_road);
            ds.sidewalk_map[sidewalk->id] = sidewalk;
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

    /***
     * Construct LineString parallel to two OSM locations.
     */
    LineString* construct_segment(object_id_type current_id,
            object_id_type neighbour_id, bool left) {

        Location current_location;
        Location neighbour_location;
        current_location = location_handler.get_node_location(current_id);
        neighbour_location = location_handler.get_node_location(neighbour_id);
        LineString *segment = nullptr;
        segment = go.parallel_line(current_location, neighbour_location,
                0.0045, left);
        return segment;
    }

    /***
     * Created tho connection between two sidewalk segments.
     * There are 3 cases:
     *   case 1:
     *   The convex side: the connection between both is added to one sidewalk.
     *   case 2:
     *   The concave side and the segment are intersecting: the intersecting
     *   point is set as the end of both geometries.
     *   case 3:
     *   The concave side without intersecting: the mean of both end is set as
     *   the end of both geometries.
     */
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
            segment1 = go.insert_point(segment1, connector, reverse_first);
        } else if (segment1->intersects(segment2)) {
            Geometry* intersector;
            intersector = segment1->intersection(segment2);
            if (intersector->getGeometryType() == "Point") {
                segment1 = go.set_point(segment1, intersector, reverse_first);   
                segment2 = go.set_point(segment2, intersector, reverse_second);
            }
        } else {
            Point* mean;
            mean = ((reverse_first ? segment1->getEndPoint() :
                    segment1->getStartPoint()),
                    (reverse_second ? segment2->getEndPoint() :
                    segment2->getStartPoint()));
            segment1 = go.set_point(segment1, mean, reverse_first);
            segment2 = go.set_point(segment2, mean, reverse_second);
        }
        sidewalk1->geometry = segment1;
        sidewalk2->geometry = segment2;
    }

    /***
     * At the end of a sidewalk pair the connection line is add to one of the
     * sidewalks (segment1).
     */
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
        segment1 = go.insert_point(segment1, connector, (reverse_first ? 2 : 0));
        sidewalk1->geometry = segment1;
    }
    
public:

    OGRGeometry *test;

    explicit SidewalkFactory(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }

    /***
     * From a node in the vehicle_node_map the sidewalks are created to every
     * neighbour node.
     */
    void generate_parallel_segments(object_id_type node_id,
            vector<VehicleMapValue> neighbours, vector<Sidewalk*>& segments,
            vector<bool>& reverse) {

        int count_neighbours = neighbours.size();
        for (int i = 0; i < count_neighbours; ++i) {
            object_id_type neighbour_id = neighbours[i].node_id;
            string connection = get_connection_string(node_id, neighbour_id);
            Sidewalk* left_sidewalk = nullptr;
            Sidewalk* right_sidewalk = nullptr;
            if (sidewalk_exists(neighbours[i], left)) {
                left_sidewalk = construct_parallel_sidewalk(i, count_neighbours,
                        node_id, neighbours, segments, reverse, left);
            } else {
                reverse.push_back(false);
            }
            if (sidewalk_exists(neighbours[i], right)) {
                right_sidewalk = construct_parallel_sidewalk(i, count_neighbours,
                        node_id, neighbours, segments, reverse, right);
            } else {
                reverse.push_back(false);
            }
            segments.push_back(left_sidewalk);
            segments.push_back(right_sidewalk);
            insert_segments(connection, left_sidewalk, right_sidewalk);
        }
    }
                
    /***
     * If there are more than two segments created, the segments are connected
     * using connect_segments. Otherwise it is the end of a road and the
     * segments are connected using connect_ends.
     */
    void generate_connections(vector<Sidewalk*>& segments,
            vector<bool>& reverse) {

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
};

#endif /* SIDEWALK_FACTORY_HPP_ */
