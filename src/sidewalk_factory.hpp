


#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateFilter.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/CoordinateArraySequenceFactory.h>

using geos::geom::Coordinate;
using geos::geom::Geometry;
using geos::geom::Point;
using geos::geom::GeometryFactory;
using geos::geom::CoordinateSequence;

class SidewalkFactory {

    DataStorage& ds;
    location_handler_type& location_handler;
    GeomOperate go;
    GeometryFactory geos_factory;
    const bool left = true;
    const bool right = false;
    //bool is_first_way = true;

public:

    OGRGeometry *test;

    explicit SidewalkFactory(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {
    }


    void construct_left_segment(object_id_type current,
            object_id_type neighbour) {

        Location current_location;
        Location neighbour_location;
        current_location = location_handler.get_node_location(current);
        neighbour_location = location_handler.get_node_location(neighbour);
        Geometry *left_sidewalk = nullptr;

        left_sidewalk = go.parallel_line(current_location, neighbour_location,
                0.003, right);
        //right_sidewalk = go.parallel_line(current_location, neighbour_location,
        //        0.003, left);
        //ds.insert_sidewalk(left_sidewalk);
        //ds.insert_sidewalk(right_sidewalk);
        ds.sidewalk_geometries.push_back(left_sidewalk);
    }


/* NOTE:
 * mit den richtigen geometrien arbeiten nicht mit osm ids. gibt performance und ich kann die geometrien verändern...
 *
 */

    void construct_convex_segment(object_id_type current,
            object_id_type prev_neighbour,
            object_id_type neighbour) {
        
        Location current_location;
        Location next_location;
        Location prev_location;
        Location vertical_point1;
        Location vertical_point2;
        Geometry *convex_segment;
        current_location = location_handler.get_node_location(current);
        next_location = location_handler.get_node_location(neighbour);
        prev_location = location_handler.get_node_location(prev_neighbour);
        double angle;
        angle = go.angle(prev_location, current_location, next_location);
        if (angle > 180) {
            vertical_point1 = go.vertical_point(current_location, prev_location,
                    0.003, false, left);
            vertical_point2 = go.vertical_point(current_location,
                    next_location, 0.003, false, right);
        } else {
            vertical_point1 = go.vertical_point(current_location, prev_location,
                    0.003, false, right);
            vertical_point2 = go.vertical_point(current_location, next_location,
                    0.003, false, left);
        }
        convex_segment = go.connect_locations(vertical_point1,
                vertical_point2);
        ds.sidewalk_geometries.push_back(convex_segment);
        //ds.insert_sidewalk(convex_segment);
        //ds.insert_node(current_location, current, ori.c_str(), angle);
    }

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

                    //cout << intersector->getGeometryType() << endl;
                }
            }
            /*
            if (segment->intersects(net)) {
                intersector = net->difference(segment);
                
                
                    //test = go.geos2ogr(intersector);
                    //cout << test->getGeometryName();
                    //ds.insert_sidewalk(test);
                

            }*/

        }
    }

    void generate_sidewalks() {
        object_id_type current;
        object_id_type neighbour;
        object_id_type prev_neighbour;

        for (auto node : ds.node_map) {
            if (node.first == 1763864221) {


            }
            current = node.first;
            int count_neighbours = node.second.size();
            for (int i = 0; i < (count_neighbours); ++i) {
                neighbour = node.second[i].first;
                int prev_index = (i - 1 + count_neighbours) % count_neighbours;
                prev_neighbour = node.second[prev_index].first; 
                construct_left_segment(current, neighbour);
                construct_convex_segment(current, prev_neighbour, neighbour); 
                
            }
        }
        ds.union_sidewalk_geometries();
        intersect_segments(ds.geos_sidewalk_net);
    
    }
};

