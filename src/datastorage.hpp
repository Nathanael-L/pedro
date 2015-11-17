/***
 * Stores all important Data over the runtime and handle the database.
 */

#ifndef DATASTORAGE_HPP_
#define DATASTORAGE_HPP_

#include <math.h>
#include <geos/index/strtree/STRtree.h>
#include <geos/index/ItemVisitor.h>
#include <geos/geom/prep/PreparedPolygon.h>
#include <gdal/ogrsf_frmts.h> 
#include <gdal/ogr_api.h>
#include <string>

using namespace std;

/***
 * Structure to remember the pedestrian and vehicle used roads to create
 * sidewalks and crossings.
 *
 * length is in kilometers (pgRouting-style)
 *
 * Sidewalk Characters are:
 *  'l' = left
 *  'r' = right
 *  'n' = none
 * length is in kilometers (pgRouting-style)
 *
 */

struct Road {
    string name;
    OGRGeometry *geometry;
    char sidewalk;
    string type;
    int lanes;
    double length;
    string osm_id;

    Road(string name, OGRGeometry *geometry,
            string type, double length, string osm_id) {

        this->name = name;
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->type = type;
        this->length = length;
        this->osm_id = osm_id;
    }

    Road(string name, OGRGeometry *geometry,
            char sidewalk, string type, int lanes,
            double length, string osm_id) {

        this->name = name;
	if (!geometry) {
	    cerr << "bad reference for geometry" << endl;
	    exit(1);
	}
        this->geometry = geometry;
        this->sidewalk = sidewalk;
        this->type = type;
        this->lanes = lanes;
        this->length = length;
        this->osm_id = osm_id;
    }
};


class DataStorage {

    OGRSpatialReference sparef_webmercator;
    OGRCoordinateTransformation *srs_tranformation;
    OGRDataSource *data_source;
    OGRLayer *layer_ways;
    OGRLayer *layer_vhcl;
    OGRLayer *layer_nodes;
    OGRLayer *layer_sidewalks;
    geom::OGRFactory<> ogr_factory;
    string output_database;
    string output_filename;
    //const char *SRS = "WGS84";
    location_handler_type &location_handler;
    GeomOperate go;

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
            int width) {

        OGRFieldDefn ogr_field(name, type);
        ogr_field.SetWidth(width);
        if (layer->CreateField(&ogr_field) != OGRERR_NONE) {
            cerr << "Creating " << name << " field failed." << endl;
            exit(1);
        }
    }

    void init_db() {
        OGRRegisterAll();

        OGRSFDriver* driver;
        driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(
            "postgresql");
            //"SQLite");
            

        if (!driver) {
            cerr << "postgresql" << " driver not available." << endl;
            //cerr << "SQLite" << " driver not available." << endl;
            exit(1);
        }

        CPLSetConfigOption("OGR_TRUNCATE", "YES");
        //CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "FALSE");
        string connection_string = "PG:dbname=" + output_filename;
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
        create_field(layer_ways, "gid", OFTInteger, 10); 
        create_field(layer_ways, "class_id", OFTInteger, 10);
        create_field(layer_ways, "length", OFTReal, 10);
        create_field(layer_ways, "name", OFTString, 40);
        create_field(layer_ways, "osm_id", OFTString, 14); 

        create_table(layer_vhcl, "vhcl", wkbLineString);
        create_field(layer_vhcl, "gid", OFTInteger, 10); 
        create_field(layer_vhcl, "class_id", OFTInteger, 10);
        create_field(layer_vhcl, "sidewalk", OFTString, 1);
        create_field(layer_vhcl, "type", OFTString, 20);
        create_field(layer_vhcl, "lanes", OFTInteger, 10);
        create_field(layer_vhcl, "length", OFTReal, 10);
        create_field(layer_vhcl, "name", OFTString, 40);
        create_field(layer_vhcl, "osm_id", OFTString, 14); 

        create_table(layer_nodes, "nodes", wkbPoint);
        create_field(layer_nodes, "osm_id", OFTString, 14);

        create_table(layer_sidewalks, "sidewalks", wkbLineString);
    }

    void order_clockwise(object_id_type node_id) {
        Location node_location; 
        Location last_location; 
        int vector_size = node_map[node_id].size();
        int mem_size = sizeof(pair<object_id_type, Road*>);
        node_location = location_handler.get_node_location(node_id);
        last_location = location_handler.get_node_location(node_map[node_id][vector_size - 1].first);

        double angle_last = go.orientation(node_location, last_location);
        for (int i = vector_size - 1; i > 1; --i) {
            object_id_type test_id = node_map[node_id][i - 1].first;
            Location test_location;
            test_location = location_handler.get_node_location(test_id);
            double angle_test = go.orientation(node_location, test_location);
            if (angle_last < angle_test) {
                swap(node_map[node_id][i],
                        node_map[node_id][i-1]);
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
    /***
     * node_map: Contains all first_nodes and last_nodes of found waterways with
     * the names and categories of the connected ways.
     * error_map: Contains ids of the potential error nodes (or mouths) to be
     * checked in pass 3.
     * error_tree: The potential error nodes remaining after pass 3 are stored
     * in here for a geometrical analysis in pass 5.
     * polygon_tree: contains prepared polygons of all water polygons except of
     * riverbanks found in pass 4. 
     */

    OGRSpatialReference sparef_wgs84;
    google::sparse_hash_set<Road*> vehicle_road_set;
    google::sparse_hash_set<Road*> pedestrian_road_set;
    google::sparse_hash_map<object_id_type,
	    vector<pair<object_id_type, Road*>>> node_map;
    google::sparse_hash_set<string> finished_connections;

    explicit DataStorage(string outfile,
            location_handler_type &location_handler) :
            output_filename(outfile),
            location_handler(location_handler) {

        init_db(); 
	node_map.set_deleted_key(-1);
	finished_connections.set_deleted_key("");
        vehicle_road_set.set_deleted_key(nullptr);
        pedestrian_road_set.set_deleted_key(nullptr);
    }

    ~DataStorage() {
        layer_ways->CommitTransaction();

        OGRDataSource::DestroyDataSource(data_source);
        OGRCleanupAll();
    }

    Road *store_pedestrian_road(string name, OGRGeometry *geometry,
        string type, double length, string osm_id) {

        Road *pedestrian_road = new Road(name, geometry,
                type, length, osm_id);
        pedestrian_road_set.insert(pedestrian_road);
	return pedestrian_road;
    }

    Road *store_vehicle_road(string name, OGRGeometry *geometry,
        char sidewalk, string type, int lanes, double length,
        string osm_id) {

        Road *vehicle_road = new Road(name, geometry, sidewalk,
                type, lanes, length, osm_id);
        vehicle_road_set.insert(vehicle_road);
	return vehicle_road;
    }

    void insert_ways() {
        int gid = 0;
        for (Road *road : pedestrian_road_set) {
            gid++;
            OGRFeature *feature;
            feature = OGRFeature::CreateFeature(layer_ways->GetLayerDefn());

            if (feature->SetGeometry(road->geometry) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            feature->SetField("gid", gid);
            feature->SetField("class_id", 0);
            feature->SetField("length", road->length);
            feature->SetField("name", road->name.c_str());
            feature->SetField("osm_id", road->osm_id.c_str());

            if (layer_ways->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }
    }

    void insert_vhcl() {
        int gid = 0;
        for (Road *road : vehicle_road_set) {
            gid++;
            OGRFeature *feature;
            feature = OGRFeature::CreateFeature(layer_vhcl->GetLayerDefn());

            if (feature->SetGeometry(road->geometry) != OGRERR_NONE) {
                cerr << "Failed to create geometry feature for way: ";
                cerr << road->osm_id << endl;
            }

            feature->SetField("gid", gid);
            feature->SetField("class_id", 0);
            feature->SetField("sidewalk", road->sidewalk);
            feature->SetField("type", road->type.c_str());
            feature->SetField("lanes", road->lanes);
            feature->SetField("length", road->length);
            feature->SetField("name", road->name.c_str());
            feature->SetField("osm_id", road->osm_id.c_str());

            if (layer_vhcl->CreateFeature(feature) != OGRERR_NONE) {
                cerr << "Failed to create ways feature." << endl;
            }
            OGRFeature::DestroyFeature(feature);
        }
    }

    void insert_node(Location location, object_id_type osm_id) {
        OGRFeature *feature;
        feature = OGRFeature::CreateFeature(layer_nodes->GetLayerDefn());
        
        OGRPoint *point = ogr_factory.create_point(location).release();
        if (feature->SetGeometry(point) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for way: ";
            cerr << osm_id << endl;
        }
        feature->SetField("osm_id", to_string(osm_id).c_str());

        if (layer_nodes->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
    }
    
    void insert_sidewalk(OGRGeometry *sidewalk) {
        OGRFeature *feature;
        feature = OGRFeature::CreateFeature(layer_sidewalks->GetLayerDefn());

        if (feature->SetGeometry(sidewalk) != OGRERR_NONE) {
            cerr << "Failed to create geometry feature for sidewalk.";
        }

        if (layer_sidewalks->CreateFeature(feature) != OGRERR_NONE) {
            cerr << "Failed to create ways feature." << endl;
        }
        OGRFeature::DestroyFeature(feature);
    }

    void insert_in_node_map(object_id_type start_node,
            object_id_type end_node,
            Road *road) {
	
        node_map[start_node].push_back(pair<object_id_type, Road*>
		(end_node, road));
        node_map[end_node].push_back(pair<object_id_type, Road*>
		(start_node, road));
        if (node_map[start_node].size() > 2) {
            order_clockwise(start_node);
        }
    }
};

#endif /* DATASTORAGE_HPP_ */
