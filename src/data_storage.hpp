/***
 * Stores all important Data over the runtime and handle the database.
 */

#ifndef DATASTORAGE_HPP_
#define DATASTORAGE_HPP_

#include <math.h>
#include <geos/index/strtree/STRtree.h>
#include <geos/index/ItemVisitor.h>
//#include <geos/geom/prep/PreparedPolygon.h>
#include <gdal/ogrsf_frmts.h> 
#include <gdal/ogr_api.h>
#include <string>

/*typedef google::sparse_hash_map<object_id_type,
        vector<pair<object_id_type, VehicleRoad*>>> vehicle_node_map_type;
typedef google::sparse_hash_map<object_id_type,
        vector<pair<object_id_type, VehicleRoad*>>>::iterator
        vehicle_node_map_iterator_type;*/

struct VehicleMapValue {
    object_id_type node_id;
    VehicleRoad* vehicle_road;
    bool forward;

    VehicleMapValue(object_id_type node_id, VehicleRoad* vehicle_road,
            bool forward) {

        this->node_id = node_id;
        this->vehicle_road = vehicle_road;
        this->forward = forward;
    }
};

class DataStorage {

    OGRSpatialReference sparef_webmercator;
    OGRCoordinateTransformation *srs_tranformation;
    OGRDataSource *data_source;
    OGRLayer *layer_ways;
    OGRLayer *layer_vehicle;
    OGRLayer *layer_nodes;
    OGRLayer *layer_sidewalks;
    OGRLayer *layer_intersects;
    geom::OGRFactory<> ogr_factory;
    geom::GEOSFactory<> geos_factory;
    string output_database;
    string output_filename;
    //const char *SRS = "WGS84";
    location_handler_type &location_handler;
    GeomOperate go;
    int gid;

    void create_table(OGRLayer *&layer, const char *name,
            OGRwkbGeometryType geometry) {

        const char* options[] = {"OVERWRITE=YES", nullptr};
        layer = data_source->CreateLayer(name, &sparef_wgs84, geometry,
                const_cast<char**>(options));
        if (!layer) {
            cerr << "Layer " << name << " creation failed." << endl;
            exit(1);
        }
    }

    void create_field(OGRLayer *&layer, const char *name, OGRFieldType type,
            int width = 0) {

        OGRFieldDefn ogr_field(name, type);
        if (width > 0) {
            ogr_field.SetWidth(width);
        }
        if (layer->CreateField(&ogr_field) != OGRERR_NONE) {
            cerr << "Creating " << name << " field failed." << endl;
            exit(1);
        }
    }

    void init_db() {
        OGRRegisterAll();

        OGRSFDriver* driver;
        driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(
            "ESRI Shapefile");
            //"postgresql");
            

        if (!driver) {
            cerr << "ESRI Shapefile" << " driver not available." << endl;
            //cerr << "postgres" << " driver not available." << endl;
            exit(1);
        }

        CPLSetConfigOption("OGR_TRUNCATE", "YES");
        //CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "FALSE");
        string connection_string = output_filename;
        //string connection_string = "PG:dbname=" + output_filename;
        data_source = driver->CreateDataSource(connection_string.c_str());
        if (!data_source) {
            cerr << "Creation of output file failed." << endl;
            exit(1);
        }

        sparef_wgs84.SetWellKnownGeogCS("WGS84");
        //sparef_webmercator.importFromProj4("+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs");
        //srs_transformation = OGRCreateCoordinateTransformation(sparef_wgs84,
        //    sparef_webmercator);
        //if (!srs_transformation) {
            //cerr << "Transformation object creation failed." << endl;
            //exit(1);
        //}

        create_table(layer_ways, "ways", wkbLineString);
        create_field(layer_ways, "gid", OFTInteger); 
        create_field(layer_ways, "class_id", OFTInteger);
        create_field(layer_ways, "length", OFTReal);
        create_field(layer_ways, "name", OFTString, 40);
        create_field(layer_ways, "osm_id", OFTString, 14); 

        create_table(layer_vehicle, "vehicle", wkbLineString);
        create_field(layer_vehicle, "gid", OFTInteger, 10); 
        create_field(layer_vehicle, "class_id", OFTInteger, 10);
        create_field(layer_vehicle, "sidewalk", OFTString, 1);
        create_field(layer_vehicle, "type", OFTString, 20);
        create_field(layer_vehicle, "lanes", OFTInteger, 10);
        create_field(layer_vehicle, "length", OFTReal, 10);
        create_field(layer_vehicle, "name", OFTString, 40);
        create_field(layer_vehicle, "osm_id", OFTString, 14); 

        /*create_table(layer_nodes, "nodes", wkbPoint);
        create_field(layer_nodes, "osm_id", OFTString, 14);
        create_field(layer_nodes, "orientations", OFTString, 7);
        create_field(layer_nodes, "angle", OFTReal, 10);
        //test what 4th parameter does

        create_table(layer_intersects, "intersects", wkbPoint);
        create_field(layer_intersects, "roads", OFTString, 50);

        create_table(layer_sidewalks, "sidewalks", wkbMultiLineString);*/
    }

    void order_clockwise(object_id_type node_id) {
        Location node_location; 
        Location last_location; 
        int vector_size = vehicle_node_map[node_id].size();
        node_location = location_handler.get_node_location(node_id);
        last_location = location_handler.get_node_location(
                vehicle_node_map[node_id][vector_size - 1].node_id);

        double angle_last = go.orientation(node_location, last_location);
        for (int i = vector_size - 1; i > 0; --i) {
            object_id_type test_id = vehicle_node_map[node_id][i - 1].node_id;
            Location test_location;
            test_location = location_handler.get_node_location(test_id);
            double angle_test = go.orientation(node_location, test_location);
            if (node_id == 49440095) {
            }
            if (angle_last < angle_test) {
                swap(vehicle_node_map[node_id][i], vehicle_node_map[node_id][i - 1]);
                if (node_id == 49440095) {
                }
            } else {
                break;
            }
        }
    }
        
//    const string get_timestamp(Timestamp timestamp) {
//        string time_str = timestamp.to_iso();
//        time_str.replace(10, 1, " ");
//        time_str.replace(19, 1, "");
//        return time_str;
//    }
//
//    string width2string(float &width) {
//        int rounded_width = static_cast<int> (round(width * 10));
//        string width_str = to_string(rounded_width);
//        if (width_str.length() == 1) {
//            width_str.insert(width_str.begin(), '0');
//        }
//        width_str.insert(width_str.end() - 1, '.');
//        return width_str;
//    }
//
//    void destroy_polygons() {
//        for (auto polygon : prepared_polygon_set) {
//            delete polygon;
//        }
//        for (auto multipolygon : multipolygon_set) {
//            delete multipolygon;
//        }
//    }
//
public:

    OGRSpatialReference sparef_wgs84;
    google::sparse_hash_set<VehicleRoad*> vehicle_road_set;
    google::sparse_hash_set<PedestrianRoad*> pedestrian_road_set;
    google::sparse_hash_set<Sidewalk*> sidewalk_set;
    google::sparse_hash_map<object_id_type,
            vector<object_id_type>> pedestrian_node_map;
    google::sparse_hash_map<object_id_type,
            vector<VehicleMapValue>> vehicle_node_map;
    google::sparse_hash_map<string, pair<Sidewalk*,
            Sidewalk*>> finished_segments;
    vector<Geometry*> pedestrian_geometries;
    vector<Geometry*> vehicle_geometries;
    vector<Geometry*> sidewalk_geometries;
    Geometry *geos_pedestrian_net;
    Geometry *geos_vehicle_net;
    Geometry *geos_sidewalk_net;
    geos::index::strtree::STRtree sidewalk_tree;
    //geos::geom::GeometryFactory geos_factory;

    explicit DataStorage(string outfile,
            location_handler_type &location_handler) :
            output_filename(outfile),
            location_handler(location_handler) {

        init_db(); 
	pedestrian_node_map.set_deleted_key(-1);
	vehicle_node_map.set_deleted_key(-1);
	finished_segments.set_deleted_key("");
        vehicle_road_set.set_deleted_key(nullptr);
        pedestrian_road_set.set_deleted_key(nullptr);
        sidewalk_set.set_deleted_key(nullptr);
        gid = 0;
        //pedestrian_geometries = new vector<Geometry*>();
        //sidewalk_geometries = new vector<Geometry*>();
        //vehicle_geometries = new vector<Geometry*>();
    }

    ~DataStorage() {
        layer_ways->CommitTransaction();
        /*layer_nodes->CommitTransaction();*/
        layer_vehicle->CommitTransaction();
        /*layer_sidewalks->CommitTransaction();
        layer_intersects->CommitTransaction();*/

        OGRDataSource::DestroyDataSource(data_source);
        OGRCleanupAll();
    }


    /*PedestrianRoad *store_pedestrian_road(string name, Geometry *geometry,
        string type, double length, string osm_id) {

        PedestrianRoad *pedestrian_road = new PedestrianRoad(name, geometry,
                type, length, osm_id);
        pedestrian_road_set.insert(pedestrian_road);
	return pedestrian_road;
    }

    VehicleRoad *store_vehicle_road(string name, Geometry *geometry,
        char sidewalk, string type, int lanes, double length,
        string osm_id) {

        VehicleRoad *vehicle_road = new VehicleRoad(name, geometry, sidewalk,
                type, lanes, length, osm_id);
        vehicle_road_set.insert(vehicle_road);
        
	return vehicle_road;
    }*/

    vector<Coordinate> segmentize(Coordinate start, Coordinate end,
            double fraction_length) {

        vector<Coordinate> splits;
        LineSegment segment(start, end);
        double length = go.haversine(start.x, start.y, end.x, end.y);
        double fraction = fraction_length / length;
        double position = fraction;

        while (position < 1) {
            Coordinate new_coord;
            segment.pointAlong(position, new_coord);
            splits.push_back(new_coord);
            position += fraction;
        }
        return splits;
    }

    void test_segmetize(Geometry* geometry) {
        cout << "test" << endl;
        GeometryFactory factory;
        LineString* linestring = dynamic_cast<LineString*>(geometry);
        CoordinateSequence *coords;
        coords = linestring->getCoordinates();
        for (int i = 0; i < (coords->getSize() - 1); i++) {
            Coordinate start = coords->getAt(i);
            Coordinate end = coords->getAt(i + 1);
            Point* end_point = factory.createPoint(end);

            for (Coordinate coord : segmentize(start, end, 0.015)) {
                Point* seg_point = factory.createPoint(coord);
                LineString* ortho_line = go.orthogonal_line(seg_point, end_point, 0.030);
                cout << ortho_line->toString() << endl;
            }
            
        }

        //const Coordinate *new_coordinate;
        //new_coordinate = dynamic_cast<Point*>(point)->getCoordinate();
        //int position = (reverse ? coords->getSize() : 0);
        //coords->add(position, *new_coordinate, true);

    }

    void insert_ways() {
        for (PedestrianRoad *road : pedestrian_road_set) {
            gid++;
            OGRFeature *feature;
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());
            test_segmetize(road->geometry);

            if (feature->SetGeometry(road->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            feature->SetField("gid", gid);
            feature->SetField("class_id", 1);
            feature->SetField("length", road->length);
            feature->SetField("name", road->name.c_str());
            feature->SetField("osm_id", road->osm_id.c_str());

            if (layer_ways->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }
    }

    void insert_vehicle() {
        for (VehicleRoad *road : vehicle_road_set) {
            gid++;
            OGRFeature *feature;
            feature = OGRFeature::CreateFeature(layer_vehicle->GetLayerDefn());

            if (feature->SetGeometry(road->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            feature->SetField("gid", gid);
            feature->SetField("class_id", 1);
            feature->SetField("sidewalk", road->sidewalk);
            feature->SetField("type", road->type.c_str());
            feature->SetField("lanes", road->lanes);
            feature->SetField("length", road->length);
            feature->SetField("name", road->name.c_str());
            feature->SetField("osm_id", road->osm_id.c_str());

            if (layer_vehicle->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }
    }

    void insert_intersect(Location location, const char *roadnames) {
        OGRFeature *feature;
        feature = OGRFeature::CreateFeature(layer_intersects->GetLayerDefn());
        
        OGRPoint *point = ogr_factory.create_point(location).release();
        if (feature->SetGeometry(point) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for intersects: ";
        }
        feature->SetField("roads", roadnames);

        if (layer_intersects->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
    }

    void insert_node(Location location, object_id_type osm_id,
        const char *ori, double angle) {
        OGRFeature *feature;
        feature = OGRFeature::CreateFeature(layer_nodes->GetLayerDefn());
        
        OGRPoint *point = ogr_factory.create_point(location).release();
        if (feature->SetGeometry(point) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for way: ";
            cerr << osm_id << endl;
        }
        feature->SetField("osm_id", to_string(osm_id).c_str());
        feature->SetField("orientations", ori);
        feature->SetField("angle", angle);

        if (layer_nodes->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
    }

    void union_pedestrian_geometries() {
        geos_pedestrian_net = go.union_geometries(pedestrian_geometries);
    }

    void union_vehicle_geometries() {
        geos_vehicle_net = go.union_geometries(vehicle_geometries);
    }

    void union_sidewalk_geometries() {
        geos_sidewalk_net = go.union_geometries(sidewalk_geometries);
    }

    void insert_sidewalk(OGRGeometry* sidewalk) {
        OGRFeature* feature;
        feature = OGRFeature::CreateFeature(layer_sidewalks->GetLayerDefn());
        OGRLineString* linestring = nullptr;
        if (sidewalk->getGeometryName() == "LineString") {
            linestring = dynamic_cast<OGRLineString*>(sidewalk);
        }
        if (feature->SetGeometry(linestring) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for sidewalk.";
        }
        if (layer_sidewalks->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
    }

    void insert_sidewalks() {

        for (Sidewalk *road : sidewalk_set) {
            gid++;
            OGRFeature *feature;
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());

            if (feature->SetGeometry(road->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for sidewalk: ";
            }

            feature->SetField("gid", gid);
            feature->SetField("class_id", 1);
            feature->SetField("type", road->type.c_str());
            feature->SetField("length", road->length);
            feature->SetField("name", road->name.c_str());
            feature->SetField("osm_id", road->osm_id.c_str());

            if (layer_ways->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }
    }

    void insert_in_vehicle_node_map(object_id_type start_node,
            object_id_type end_node,
            VehicleRoad* road) {
	
        VehicleMapValue forward = VehicleMapValue(end_node, road, true);
        VehicleMapValue backward = VehicleMapValue(start_node, road, false);
        vehicle_node_map[start_node].push_back(forward);
        vehicle_node_map[end_node].push_back(backward);
        if (vehicle_node_map[end_node].size() > 1) {
            order_clockwise(end_node);
        }
        if (vehicle_node_map[start_node].size() > 1) {
            order_clockwise(start_node);
        }
    }
};

#endif /* DATASTORAGE_HPP_ */
