/***
 * geometry_constructor.hpp
 *
 *  Created on: Dec 18, 2015
 *      Author: nathanael
 *
 *  The GeometryConstructor contains both, the SidewalkFactory and the
 *  CrossingFactory. The main function is to create the sidewalks while
 *  iterating through the vehicle_node_map. While iterating the official
 *  OSM crossing are created.
 *  After constructing the sidewalks and crossings, the intersections
 *  between the constructed geometries and the pedestrian way of OSM are
 *  calculated.
 *
 */


#ifndef GEOMETRY_CONSTRUCTOR_HPP_
#define GEOMETRY_CONSTRUCTOR_HPP_

class GeometryConstructor {

    location_handler_type& location_handler;
    DataStorage& ds;
    GeomOperate go;
    google::sparse_hash_map<PedestrianRoad*, vector<PedestrianRoad*>>
            temp_pedestrian_map;
    google::sparse_hash_set<PedestrianRoad*>
            temp_pedestrian_set;
    google::sparse_hash_map<string, vector<Crossing*>>
            temp_crossing_map;
    google::sparse_hash_map<string, vector<Sidewalk*>>
            temp_sidewalk_map;
    char* my_null;
    GeometryFactory geos_factory;

    /***
     * A given geometry is split into a pair of geometries divided at a given
     * point.
     */
    pair<Geometry*, Geometry*> split_line(Geometry* geometry,
            Point* split_point) {

        LineString* origin_geometry = dynamic_cast<LineString*>(geometry);
        LineString* first_segment = nullptr;
        LineString* second_segment = nullptr;
        int num_points = geometry->getNumPoints();
        if (num_points == 2) {
            first_segment = go.set_point(origin_geometry, split_point, true);
            second_segment = go.set_point(origin_geometry, split_point, false);
        } else {
            for (int i = 0; i < num_points - 1; ++i) {
                if (go.point_is_between(split_point,
                        origin_geometry->getPointN(i),
                        origin_geometry->getPointN(i + 1))) {
                    first_segment = go.set_point(origin_geometry, split_point,
                            i + 1);
                    if ((i + 1) < (num_points - 1)) {
                        first_segment = go.cut_line(first_segment, i + 1, true);
                    }
                    second_segment = go.set_point(origin_geometry, split_point,
                            i);
                    if (i > 0) {
                        second_segment = go.cut_line(second_segment, i, false);
                    }
                    break;
                }
            }
        }
        if (first_segment) {
            geometry = first_segment;
        } else {
            cerr << "unexcepted error in split_line / GeometryConstructor"
                    << endl;
            exit(1);
        }
        if (!second_segment) {
            cerr << "unexcepted error in split_line / GeometryConstructor"
                    << endl;
            exit(1);
        }
        return pair<Geometry*, Geometry*>(first_segment, second_segment);
    }

    /***
     * Split both, crossing and OSM pedestrian, and create new objects.
     */
    void split_and_create(PedestrianRoad* pedestrian, Crossing* crossing,
            Point* intersection_point, int count_intersects) {

        Geometry* pedestrian_g = pedestrian->geometry;
        Geometry* crossing_g = crossing->geometry;
        pair<Geometry*, Geometry*> pedestrian_pair =
                split_line(pedestrian_g, intersection_point);
        pair<Geometry*, Geometry*> crossing_pair =
                split_line(crossing_g, intersection_point);
        PedestrianRoad* changed_pedestrian = new PedestrianRoad(
                pedestrian->get_index(), pedestrian, pedestrian_pair.first);
        PedestrianRoad* new_pedestrian = new PedestrianRoad(
                pedestrian->get_index() + count_intersects,
                pedestrian, pedestrian_pair.second);
        Crossing* changed_crossing = new Crossing(crossing, crossing_pair.first,
                crossing->get_index());
        Crossing* new_crossing = new Crossing(crossing, crossing_pair.second,
                crossing->get_index() + count_intersects);
        temp_pedestrian_map[pedestrian].push_back(changed_pedestrian);
        temp_pedestrian_set.insert(new_pedestrian);
        temp_crossing_map[changed_crossing->id].push_back(changed_crossing);
        temp_crossing_map["new"].push_back(new_crossing);
    }

    /***
     * Iterate through multipoint and do split_and_create for each intersection
     * point.
     */
    void split_and_create(PedestrianRoad* pedestrian, Crossing* crossing,
            MultiPoint* multipoint, int count_intersects) {

        CoordinateSequence *coords;
        coords = multipoint->getCoordinates();
        for (int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate current = coords->getAt(i);
            Point* intersection_point = geos_factory.createPoint(current);
            split_and_create(pedestrian, crossing, intersection_point,
                    count_intersects);
            count_intersects++;
        }
    }

    /***
     * Split both, sidewalk and OSM pedestrian, and create new objects.
     */
    void split_and_create(PedestrianRoad* pedestrian, Sidewalk* sidewalk,
            Point* intersection_point, int count_intersects) {

        Geometry* pedestrian_g = pedestrian->geometry;
        Geometry* sidewalk_g = sidewalk->geometry;
        pair<Geometry*, Geometry*> pedestrian_pair =
                split_line(pedestrian_g, intersection_point);
        pair<Geometry*, Geometry*> sidewalk_pair =
                split_line(sidewalk_g, intersection_point);
        PedestrianRoad* changed_pedestrian = new PedestrianRoad(
                pedestrian->get_index(), pedestrian, pedestrian_pair.first);
        PedestrianRoad* new_pedestrian = new PedestrianRoad(
                pedestrian->get_index() + count_intersects,
                pedestrian, pedestrian_pair.second);
        Sidewalk* changed_sidewalk = new Sidewalk(sidewalk, sidewalk_pair.first,
                sidewalk->get_index());
        Sidewalk* new_sidewalk = new Sidewalk(sidewalk, sidewalk_pair.second,
                sidewalk->get_index() + count_intersects);
        //debug
        if (changed_pedestrian->osm_id == "23093185") {
            cout << "DEBUG1" << endl;
            cout << changed_pedestrian->geometry->toString() << endl;
            cout << new_pedestrian->geometry->toString() << endl;
        }
        if (new_pedestrian->osm_id == "23093185") {
            cout << "DEBUG2" << endl;
            cout << changed_sidewalk->geometry->toString() << endl;
            cout << new_sidewalk->geometry->toString() << endl;
        }

        temp_pedestrian_map[pedestrian].push_back(changed_pedestrian);
        temp_pedestrian_set.insert(new_pedestrian);
        temp_sidewalk_map[changed_sidewalk->id].push_back(changed_sidewalk);
        temp_sidewalk_map["new"].push_back(new_sidewalk);
    }

    /***
     * Iterate through multipoint and do split_and_create for each intersection
     * point.
     */
    void split_and_create(PedestrianRoad* pedestrian, Sidewalk* sidewalk,
            MultiPoint* multipoint, int count_intersects) {

        CoordinateSequence *coords;
        coords = multipoint->getCoordinates();
        for (int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate current = coords->getAt(i);
            Point* intersection_point = geos_factory.createPoint(current);
            split_and_create(pedestrian, sidewalk, intersection_point,
                    count_intersects);
            count_intersects++;
        }
    }

    /***
     * Store changes into DataStorage.
     */
    void insert_changes() {
        for (auto entry : temp_pedestrian_map) {
            if (entry.first) {
                if (ds.pedestrian_road_set.find(entry.first) !=
                        ds.pedestrian_road_set.end()) {
                    ds.pedestrian_road_set.erase(entry.first);
                    ds.pedestrian_road_set.insert(entry.second[0]);
                } else {
                    cerr << "unexcepted error in insert_changes / "
                            << "GeometryConstructor" << endl;
                    exit(1);
                }
            }
        }
        for (auto road : temp_pedestrian_set) {
                ds.pedestrian_road_set.insert(road);
        }
        for (auto entry : temp_sidewalk_map) {
            if (entry.first != "new") {
                ds.sidewalk_map[entry.first] = entry.second[0]; 
            } else {
                for (auto road : entry.second) {
                    ds.sidewalk_map[road->id] = road;
                }
            }
        }
    }

public:

    //OGRGeometry *test;

    explicit GeometryConstructor(DataStorage& data_storage,
            location_handler_type& location_handler) :
            ds(data_storage), location_handler(location_handler) {

        PedestrianRoad* null_road;
        temp_pedestrian_map.set_deleted_key(null_road);
        temp_sidewalk_map.set_deleted_key("");
        my_null = "0";
    }

    /***
     * Iterate through vehicle_node_map and create sidewalk geometries.
     * Use the intern SidewalkFactory and CrossingFactory.
     */
    void generate_sidewalks() {
        SidewalkFactory* sidewalk_factory = new SidewalkFactory(ds,
                location_handler);
        CrossingFactory* crossing_factory = new CrossingFactory(ds,
                location_handler);
        vector<Sidewalk*> segments;
        vector<bool> reverse;
        for (auto node : ds.vehicle_node_map) {
            segments.clear();
            reverse.clear();
            sidewalk_factory->generate_parallel_segments(node.first,
                    node.second, segments, reverse);
            sidewalk_factory->generate_connections(segments, reverse);
            if (node.second[0].is_crossing) {
                string crossing_type = node.second[0].crossing_type;
                crossing_factory->generate_osm_crossing(segments, reverse,
                crossing_type);
            }
        }
    }

    /***
     * Connect the original OSM pedestrian ways with the constructed
     * geometries. Iterate over pedestrian ways and query the sidewalk_tree.
     * The results are intersected with the way and split_and_create is called
     * for every positive intersection.
     */
    void connect_sidewalks_and_pedesrians() {
        for (PedestrianRoad* pedestrian : ds.pedestrian_road_set) {
            Geometry* pedestrian_g = pedestrian->geometry;
            /***ENLARGING LINESTRING NOT IMPLEMENTED JET***
            cout << "before: " << pedestrian_g->toString() << endl;
            pedestrian_g = go.enlarge_line(pedestrian_g, 0.001);
            cout << "after: " << pedestrian_g->toString() << endl;
            ***/
            vector<void *> results_sidewalk;
            ds.sidewalk_tree.query(pedestrian_g->getEnvelopeInternal(), results_sidewalk);
            if (results_sidewalk.size() > 1) {
                int count_intersects = 0;
                Sidewalk* sidewalk = nullptr;
                for (auto result : results_sidewalk) {
                    sidewalk = static_cast<Sidewalk*>(result);
                    Geometry* sidewalk_g = sidewalk->geometry;
                    if (pedestrian_g->intersects(const_cast<const Geometry*>(sidewalk_g))) {
                        count_intersects++;
                        Geometry* intersection = pedestrian_g->intersection(
                                const_cast<const Geometry*>(sidewalk_g));
                        if (intersection->getGeometryType() == "MultiPoint") {
                            MultiPoint* multipoint = dynamic_cast<MultiPoint*>(
                                    intersection);
                            split_and_create(pedestrian, sidewalk, multipoint,
                                    count_intersects);
                        } else {
                            Point* intersection_point = dynamic_cast<Point*>(intersection);
                            split_and_create(pedestrian, sidewalk,
                                    intersection_point, count_intersects);
                        }
                    }
                }
            }
            vector<void *> results_crossing;
            ds.crossing_tree.query(pedestrian_g->getEnvelopeInternal(), results_crossing);
            if (results_crossing.size() > 1) {
                int count_intersects = 0;
                Crossing* crossing = nullptr;
                for (auto result : results_crossing) {
                    crossing = static_cast<Crossing*>(result);
                    Geometry* crossing_g = crossing->geometry;
                    if (pedestrian_g->intersects(const_cast<const Geometry*>(crossing_g))) {
                        count_intersects++;
                        Geometry* intersection = pedestrian_g->intersection(
                                const_cast<const Geometry*>(crossing_g));
                        if (intersection->getGeometryType() == "MultiPoint") {
                            MultiPoint* multipoint = dynamic_cast<MultiPoint*>(
                                    intersection);
                            split_and_create(pedestrian, crossing, multipoint,
                                    count_intersects);
                        } else {
                            Point* intersection_point = dynamic_cast<Point*>(intersection);
                            split_and_create(pedestrian, crossing,
                                    intersection_point, count_intersects);
                        }
                    }
                }
            }
        }
        insert_changes();
    }
};

#endif /* GEOMETRY_CONSTRUCTOR_HPP_ */
