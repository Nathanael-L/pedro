/***
 * data_storage.hpp
 *
 *  Created on: Nov 9, 2015
 *      Author: nathanael
 *
 * Stores all important Data over the runtime and handle the database.
 *
 */

#ifndef DATASTORAGE_HPP_
#define DATASTORAGE_HPP_

#include <math.h>
#include <geos/index/strtree/STRtree.h>
#include <geos/index/ItemVisitor.h>
#include <gdal/ogrsf_frmts.h> 
#include <gdal/ogr_api.h>
#include <string>

/***
 * In the vehicle_node_map all roads are stored to create the sidewalks.
 */
struct VehicleMapValue {
    int from;
    int to;
    object_id_type node_id;
    VehicleRoad* vehicle_road;
    bool is_foreward;
    bool is_crossing;
    string crossing_type;

    VehicleMapValue(object_id_type node_id, int from, int to,
            VehicleRoad* vehicle_road, bool is_foreward, bool is_crossing,
            string crossing_type = "") {

        this->from = from;
        this->to = to;
        this->node_id = node_id;
        this->vehicle_road = vehicle_road;
        this->is_foreward = is_foreward;
        this->is_crossing = is_crossing;
        this->crossing_type = crossing_type;
    }
};

class DataStorage {

    string output_database;
    string output_filename;
    location_handler_type &location_handler;
    GeomOperate go;
    OGRDataSource* data_source;
    OGRLayer* layer_ways;
    OGRLayer* layer_intersects;
    //OGRLayer* layer_vehicle;
    //OGRLayer* layer_nodes;
    //OGRLayer* layer_sidewalks;
    //OGRLayer* layer_crossings;
    //OGRLayer* layer_orthos;

    geom::OGRFactory<> ogr_factory;
    geom::GEOSFactory<> geos_factory;
    GeometryFactory geometry_factory;

    const char* SRS = "WGS84";
    long gid;
    int link_counter;
    bool psql;

    /***
     * Crate OGR table.
     */
    void create_table(OGRLayer*& layer, const char* name,
            OGRwkbGeometryType geometry) {

        const char* options[] = {"OVERWRITE=YES", nullptr};
        layer = data_source->CreateLayer(name, &sparef_wgs84, geometry,
                const_cast<char**>(options));
        if (!layer) {
            cerr << "Layer " << name << " creation failed." << endl;
            exit(1);
        }
    }

    /***
     * Crate OGR column.
     */
    void create_field(OGRLayer* &layer, const char* name, OGRFieldType type,
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

    /***
     * create OGR layers - database structure
     * default is shapefile if psql (-p) is not set
     */
    void init_db(bool psql) {
        OGRRegisterAll();

        OGRSFDriver* driver;
        string driver_name = "ESRI Shapefile";
        if (psql) driver_name = "postgresql";
        driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(
                driver_name.c_str());

        if (!driver) {
            if (psql) {
                cerr << "ESRI Shapefile" << " driver not available." << endl;
            } else {
                cerr << "postgres" << " driver not available." << endl;
            }
            exit(1);
        }

        CPLSetConfigOption("OGR_TRUNCATE", "YES");
        string connection_string = output_filename;
        if (psql) string connection_string = "PG:dbname=" + output_filename;
        data_source = driver->CreateDataSource(connection_string.c_str());
        if (!data_source) {
            cerr << "Creation of output file failed." << endl;
            exit(1);
        }

        sparef_wgs84.SetWellKnownGeogCS(SRS);

        create_table(layer_ways, "ways", wkbLineString);
        //create_field(layer_ways, "gid", OFTInteger); 
        create_field(layer_ways, "class_id", OFTInteger);
        create_field(layer_ways, "type", OFTString, 20);
        create_field(layer_ways, "osm_type", OFTString, 14);
        create_field(layer_ways, "length", OFTReal);
        create_field(layer_ways, "name", OFTString, 40);
        create_field(layer_ways, "osm_id", OFTString, 14); 

        /*create_table(layer_vehicle, "vehicle", wkbLineString);
        create_field(layer_vehicle, "gid", OFTInteger, 10); 
        create_field(layer_vehicle, "class_id", OFTInteger, 10);
        create_field(layer_vehicle, "sidewalk", OFTString, 1);
        create_field(layer_vehicle, "type", OFTString, 20);
        create_field(layer_vehicle, "lanes", OFTInteger, 10);
        create_field(layer_vehicle, "length", OFTReal, 10);
        create_field(layer_vehicle, "name", OFTString, 40);
        create_field(layer_vehicle, "osm_id", OFTString, 14); 

        create_table(layer_nodes, "nodes", wkbPoint);
        create_field(layer_nodes, "osm_id", OFTString, 14);
        create_field(layer_nodes, "orientations", OFTString, 7);
        create_field(layer_nodes, "angle", OFTReal, 10);
*/
        create_table(layer_intersects, "intersects", wkbLineString);
        create_field(layer_intersects, "length", OFTReal);
        create_field(layer_intersects, "ratio", OFTReal);
/*
        create_table(layer_sidewalks, "sidewalks", wkbLineString);
        create_field(layer_sidewalks, "id", OFTString, 16);

        create_table(layer_crossings, "crossings", wkbLineString);
        create_field(layer_crossings, "type", OFTString, 10);

        create_table(layer_orthos, "orthos", wkbLineString);*/
    }

    /***
     * for the geometric creations of the sidewalks a clockwise order is
     * neccessary.
     */
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
            if (angle_last < angle_test) {
                swap(vehicle_node_map[node_id][i], vehicle_node_map[node_id][i - 1]);
            } else {
                break;
            }
        }
    }
        
public:

    OGRSpatialReference sparef_wgs84;
    google::sparse_hash_set<VehicleRoad*> vehicle_road_set;
    google::sparse_hash_set<PedestrianRoad*> pedestrian_road_set;
    google::sparse_hash_map<string, Sidewalk*> sidewalk_map;
    google::sparse_hash_set<Crossing*> crossing_set;
    google::sparse_hash_map<object_id_type,
            vector<object_id_type>> pedestrian_node_map;
    google::sparse_hash_map<object_id_type,
            vector<VehicleMapValue>> vehicle_node_map;
    google::sparse_hash_map<object_id_type, CrossingPoint*>
            crossing_node_map;
    google::sparse_hash_map<string, pair<Sidewalk*,
            Sidewalk*>> finished_segments;

    geos::index::strtree::STRtree ortho_tree;
    geos::index::strtree::STRtree sidewalk_tree;
    geos::index::strtree::STRtree crossing_tree;
    const bool is_foreward = true;
    const bool is_backward = false;

    explicit DataStorage(string outfile,
            location_handler_type &location_handler, bool psql) :
            output_filename(outfile),
            location_handler(location_handler),
            psql(psql) {

        init_db(psql); 
	pedestrian_node_map.set_deleted_key(-1);
	vehicle_node_map.set_deleted_key(-1);
	finished_segments.set_deleted_key("");
        vehicle_road_set.set_deleted_key(nullptr);
        pedestrian_road_set.set_deleted_key(nullptr);
        sidewalk_map.set_deleted_key("");
        crossing_set.set_deleted_key(nullptr);
        //gid = 0;
        link_counter = 0;
    }

    /***
     * creates the database tables when DataStorage object is destoyed.
     */
    ~DataStorage() {
        layer_ways->CommitTransaction();
        /*layer_nodes->CommitTransaction();*/
        //layer_vehicle->CommitTransaction();
        //layer_sidewalks->CommitTransaction();
        //layer_crossings->CommitTransaction();
        layer_intersects->CommitTransaction();
        //layer_orthos->CommitTransaction();

        OGRDataSource::DestroyDataSource(data_source);
        OGRCleanupAll();
    }

    /***
     * Clean up Raods and Geometries
     *
     */
    void clean_up() {
        for (auto road : vehicle_road_set) {
            geometry_factory.destroyGeometry(road->geometry);
        }
        vehicle_road_set.clear();
        for (auto road : pedestrian_road_set) {
            geometry_factory.destroyGeometry(road->geometry);
        }
        pedestrian_road_set.clear();
        for (auto entry : sidewalk_map) {
            geometry_factory.destroyGeometry(entry.second->geometry);
        }
        sidewalk_map.clear();
        for (auto road : crossing_set) {
            geometry_factory.destroyGeometry(road->geometry);
        }
        crossing_set.clear();
        pedestrian_node_map.clear();
        vehicle_node_map.clear();
        crossing_node_map.clear();
        finished_segments.clear();
    }


    /*PedestrianRoad* store_pedestrian_road(string name, Geometry* geometry,
        string type, double length, string osm_id) {

        PedestrianRoad* pedestrian_road = new PedestrianRoad(name, geometry,
                type, length, osm_id);
        pedestrian_road_set.insert(pedestrian_road);
	return pedestrian_road;
    }

    VehicleRoad* store_vehicle_road(string name, Geometry* geometry,
        char sidewalk, string type, int lanes, double length,
        string osm_id) {

        VehicleRoad* vehicle_road = new VehicleRoad(name, geometry, sidewalk,
                type, lanes, length, osm_id);
        vehicle_road_set.insert(vehicle_road);
        
	return vehicle_road;
    }*/


    void insert_ways() {
        for (PedestrianRoad* road : pedestrian_road_set) {
            //gid++;
            OGRFeature* feature;
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());

            if (feature->SetGeometry(road->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            //feature->SetField("gid", gid);
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

    /*void insert_vehicle() {
        for (VehicleRoad* road : vehicle_road_set) {
            //gid++;
            OGRFeature* feature;
            feature = OGRFeature::CreateFeature(layer_vehicle->GetLayerDefn());

            if (feature->SetGeometry(road->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            //feature->SetField("gid", gid);
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
    }*/
 
    //debug
    void insert_intersect(Geometry* geometry, double length, double ratio) {
        OGRFeature* feature;
        feature = OGRFeature::CreateFeature(layer_intersects->GetLayerDefn());
        
        OGRGeometry* sidewalk = go.geos2ogr(geometry);
        if (feature->SetGeometry(sidewalk) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for intersects: ";
        }
        feature->SetField("length", length);
        feature->SetField("ratio", ratio);

        if (layer_intersects->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
        OGRGeometryFactory::destroyGeometry(sidewalk);
    }

    /*void insert_orthos(Geometry* geometry) {
        OGRFeature* feature;
        feature = OGRFeature::CreateFeature(layer_orthos->GetLayerDefn());
        
        OGRGeometry* ortho = go.geos2ogr(geometry);
        if (feature->SetGeometry(ortho) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for orthos: ";
        }

        if (layer_orthos->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
        OGRGeometryFactory::destroyGeometry(ortho);
    }*/

    /*void insert_node(Location location, object_id_type osm_id,
        const char* ori, double angle) {
        OGRFeature* feature;
        feature = OGRFeature::CreateFeature(layer_nodes->GetLayerDefn());
        
        OGRPoint* point = ogr_factory.create_point(location).release();
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
        OGRGeometryFactory::destroyGeometry(point);
    }*/

    void insert_sidewalks() {
        /*c for display
        for (auto map_entry : sidewalk_map) {
            Sidewalk* sidewalk = map_entry.second;
            gid++;
            OGRFeature* feature;
            feature = OGRFeature::CreateFeature(layer_sidewalks->GetLayerDefn());

            if (feature->SetGeometry(sidewalk->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for sidewalk: ";
            }

            feature->SetField("id", sidewalk->id.c_str());

            if (layer_sidewalks->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }*/
        for (auto map_entry : sidewalk_map) {
            Sidewalk* sidewalk = map_entry.second;
            //gid++;
            OGRFeature* feature;
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());

            if (feature->SetGeometry(sidewalk->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for sidewalk: ";
            }

            feature->SetField("id", sidewalk->id.c_str());
            //feature->SetField("gid", gid);
            feature->SetField("class_id", 1);
            feature->SetField("type", sidewalk->type.c_str());
            feature->SetField("osm_type", sidewalk->at_osm_type.c_str());
            feature->SetField("length", sidewalk->length);
            feature->SetField("name", sidewalk->name.c_str());
            //feature->SetField("osm_id", sidewalk->osm_id.c_str());



            if (layer_ways->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }/**/
    }

    void fill_sidewalk_tree() {
        for (auto map_entry : sidewalk_map) {
            Sidewalk* sidewalk = map_entry.second;
            sidewalk_tree.insert(sidewalk->geometry->getEnvelopeInternal(), sidewalk);
        }

    }

    void fill_crossing_tree() {
        for (auto set_entry : crossing_set) {
            Crossing* crossing = set_entry;
            crossing_tree.insert(crossing->geometry->getEnvelopeInternal(), crossing);
        }

    }

    void insert_crossings() {
        for (Crossing* crossing : crossing_set) {
            //gid++;
            OGRFeature* feature;
            
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());

            if (feature->SetGeometry(crossing->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for sidewalk: ";
            }

            feature->SetField("id", crossing->id.c_str());
            //feature->SetField("gid", gid);
            feature->SetField("class_id", 1);
            feature->SetField("type", crossing->type.c_str());
            feature->SetField("length", crossing->length);
            feature->SetField("name", crossing->name.c_str());
            //feature->SetField("osm_id", sidewalk->osm_id.c_str());

            if (layer_ways->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
            /*
            feature = OGRFeature::CreateFeature(layer_crossings->GetLayerDefn());

            if (feature->SetGeometry(crossing->get_ogr_geom()) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for sidewalk: ";
            }
            feature->SetField("type", crossing->type.c_str());

            if (layer_crossings->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
            **/
        }
    }


    void insert_in_vehicle_node_map(object_id_type start_node,
            object_id_type end_node, VehicleRoad* road, bool start_is_crossing,
            string start_crossing_type, bool end_is_crossing,
            string end_crossing_type) {

        bool backlink_is_new = (vehicle_node_map[start_node].size() == 0);
        bool forelink_is_new = (vehicle_node_map[end_node].size() == 0);
        int backlink_id = 0;
        int forelink_id = 0;
        if (backlink_is_new) {
            link_counter++;
            backlink_id = link_counter;
        } else {
            backlink_id = vehicle_node_map[start_node][0].from;
        }
        if (forelink_is_new) {
            link_counter++;
            forelink_id = link_counter;
        } else {
            forelink_id = vehicle_node_map[end_node][0].from;
        }
        VehicleMapValue backward = VehicleMapValue(start_node, forelink_id,
                backlink_id, road, is_backward, end_is_crossing,
                end_crossing_type);
        VehicleMapValue foreward = VehicleMapValue(end_node, backlink_id,
                forelink_id, road, is_foreward, start_is_crossing,
                start_crossing_type);
        vehicle_node_map[start_node].push_back(foreward);
        vehicle_node_map[end_node].push_back(backward);
        if (vehicle_node_map[start_node].size() > 1) {
            order_clockwise(start_node);
        }
        if (vehicle_node_map[end_node].size() > 1) {
            order_clockwise(end_node);
        }
    }
};

#endif /* DATASTORAGE_HPP_ */
