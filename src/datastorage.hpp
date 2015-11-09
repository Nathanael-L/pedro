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

using namespace std;

typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type,
                                  osmium::Location>
        index_neg_type;

typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
                                           osmium::Location>
        index_pos_type;

typedef osmium::handler::NodeLocationsForWays<index_pos_type, 
                                              index_neg_type>
        location_handler_type;

class DataStorage {
    OGRDataSource* m_data_source;
    OGRLayer* m_layer_polygons;
    OGRLayer* m_layer_relations;
    OGRLayer* m_layer_ways;
    OGRLayer* m_layer_nodes;
    osmium::geom::OGRFactory<> m_ogr_factory;
    string output_filename;

    /***
     * Structure to remember the waterways according to the firstnodes and
     * lastnodes of the waterways.
     *
     * Categories are:
     *  drain, brook, ditch = A
     *  stream              = B
     *  river               = C
     *  other, canal        = ?
     *  >> ignore canals, because can differ in floating direction and size
     */
    struct WaterWay {
        osmium::object_id_type first_node;
        osmium::object_id_type last_node;
        string name;
        char category;

        WaterWay(osmium::object_id_type first_node,
                 osmium::object_id_type last_node,
                 const char *name, const char *type) {
            this->first_node = first_node;
            this->last_node = last_node;
            this->name = name;
            if ((!strcmp(type, "drain")) ||
                (!strcmp(type, "brook")) ||
                (!strcmp(type, "ditch"))) {
                category = 'A';
            } else if (!strcmp(type, "stream")) {
                category = 'B';
            } else if (!strcmp(type, "river")) {
                category = 'C';
            } else {
                category = '?';
            }
        }
    };

    vector<WaterWay*> waterways;

    void init_db() {
        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");
        OGRRegisterAll();

        OGRSFDriver* driver;
        driver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(
            "SQLite");

        if (!driver) {
            cerr << "SQLite" << " driver not available." << endl;
            exit(1);
        }

        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "FALSE");
        const char* options[] = { "SPATIALITE=TRUE", nullptr };
        m_data_source = driver->CreateDataSource(output_filename.c_str(),
                                                 const_cast<char**>(options));
        if (!m_data_source) {
            cerr << "Creation of output file failed." << endl;
            exit(1);
        }

        OGRSpatialReference sparef;
        sparef.SetWellKnownGeogCS("WGS84");

        /*---- TABLE POLYGONS ----*/
        m_layer_polygons = m_data_source->CreateLayer("polygons", &sparef,
                                                      wkbMultiPolygon, nullptr);
        if (!m_layer_polygons) {
            cerr << "Layer polygons creation failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_way_id("way_id", OFTInteger);
        layer_polygons_field_way_id.SetWidth(12);
        if (m_layer_polygons->CreateField(&layer_polygons_field_way_id)
                != OGRERR_NONE) {
            cerr << "Creating way_id field failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_relation_id("relation_id", 
                                                      OFTInteger);
        layer_polygons_field_relation_id.SetWidth(12);
        if (m_layer_polygons->CreateField(&layer_polygons_field_relation_id)
                != OGRERR_NONE) {
            cerr << "Creating relation_id field failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_type("type", OFTString);
        layer_polygons_field_type.SetWidth(10);
        if (m_layer_polygons->CreateField(&layer_polygons_field_type)
                != OGRERR_NONE) {
            cerr << "Creating type field failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_name("name", OFTString);
        layer_polygons_field_name.SetWidth(30);
        if (m_layer_polygons->CreateField(&layer_polygons_field_name)
                != OGRERR_NONE) {
            cerr << "Creating name field failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_lastchange("lastchange", OFTString);
        layer_polygons_field_lastchange.SetWidth(20);
        if (m_layer_polygons->CreateField(&layer_polygons_field_lastchange)
                != OGRERR_NONE) {
            cerr << "Creating lastchange field failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_polygons_field_error("error", OFTString);
        layer_polygons_field_error.SetWidth(6);
        if (m_layer_polygons->CreateField(&layer_polygons_field_error)
                != OGRERR_NONE) {
            cerr << "Creating error field failed." << endl;
            exit(1);
        }

        int ogrerr = m_layer_polygons->StartTransaction();
        if (ogrerr != OGRERR_NONE) {
            cerr << "Creating polygons table failed." << endl;
            cerr << "OGRERR: " << ogrerr << endl;
            exit(1);
        }


        /*---- TABLE RELATIONS ----*/
        m_layer_relations = m_data_source->CreateLayer("relations", &sparef,
                                                       wkbMultiLineString,
                                                       nullptr);
        if (!m_layer_relations) {
            cerr << "Layer relations creation failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_id("relation_id", OFTInteger);
        layer_relations_field_id.SetWidth(12);
        if (m_layer_relations->CreateField(&layer_relations_field_id)
                != OGRERR_NONE) {
            cerr << "Creating relation_id field in table realtions failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_type("type", OFTString);
        layer_relations_field_type.SetWidth(10);
        if (m_layer_relations->CreateField(&layer_relations_field_type)
                != OGRERR_NONE) {
            cerr << "Creating type field in table realtions failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_name("name", OFTString);
        layer_relations_field_type.SetWidth(30);
        if (m_layer_relations->CreateField(&layer_relations_field_name)
                != OGRERR_NONE) {
            cerr << "Creating name field in table realtions failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_lastchange("lastchange", OFTString);
        layer_relations_field_lastchange.SetWidth(20);
        if (m_layer_relations->CreateField(&layer_relations_field_lastchange)
                != OGRERR_NONE) {
            cerr << "Creating lastchange field in table realtions failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_nowaterway_error("nowaterway_error",
                                                            OFTString);
        layer_relations_field_type.SetWidth(6);
        if (m_layer_relations->CreateField(
                &layer_relations_field_nowaterway_error)
                != OGRERR_NONE) {
            cerr << "Creating nowaterway_error field in table realtions failed."
                 << endl;
            exit(1);
        }

        OGRFieldDefn layer_relations_field_tagging_error("tagging_error",
                                                         OFTString);
        layer_relations_field_type.SetWidth(6);
        if (m_layer_relations->CreateField(&layer_relations_field_tagging_error)
                != OGRERR_NONE) {
            cerr << "Creating tagging_error field in table realtions failed." << endl;
            exit(1);
        }

        ogrerr = m_layer_relations->StartTransaction();
        if (ogrerr != OGRERR_NONE) {
            cerr << "Creating relations table failed." << endl;
            cerr << "OGRERR: " << ogrerr << endl;
            exit(1);
        }

        /*---- TABLE WAYS ----*/
        m_layer_ways = m_data_source->CreateLayer("ways", &sparef,
                                                  wkbLineString, nullptr);
        if (!m_layer_ways) {
            cerr << "Layer ways creation in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_id("way_id", OFTInteger);
        layer_ways_field_id.SetWidth(12);
        if (m_layer_ways->CreateField(&layer_ways_field_id) != OGRERR_NONE) {
            cerr << "Creating way_id field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_type("type", OFTString);
        layer_ways_field_type.SetWidth(10);
        if (m_layer_ways->CreateField(&layer_ways_field_type) != OGRERR_NONE) {
            cerr << "Creating type field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_name("name", OFTString);
        layer_ways_field_name.SetWidth(30);
        if (m_layer_ways->CreateField(&layer_ways_field_name) != OGRERR_NONE) {
            cerr << "Creating name field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_first_node("firstnode", OFTString);
        layer_ways_field_first_node.SetWidth(11);
        if (m_layer_ways->CreateField(&layer_ways_field_first_node)
                != OGRERR_NONE) {
            cerr << "Creating firstnode field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_last_node("lastnode", OFTString);
        layer_ways_field_last_node.SetWidth(11);
        if (m_layer_ways->CreateField(&layer_ways_field_last_node)
                != OGRERR_NONE) {
            cerr << "Creating lastnode field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_relation("relation_id", OFTInteger);
        layer_ways_field_relation.SetWidth(10);
        if (m_layer_ways->CreateField(&layer_ways_field_relation)
                != OGRERR_NONE) {
            cerr << "Creating relation_id field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_width("width", OFTString);
        layer_ways_field_width.SetWidth(10);
        if (m_layer_ways->CreateField(&layer_ways_field_width) != OGRERR_NONE) {
            cerr << "Creating width field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_lastchange("lastchange", OFTString);
        layer_ways_field_lastchange.SetWidth(20);
        if (m_layer_ways->CreateField(&layer_ways_field_lastchange)
                != OGRERR_NONE) {
            cerr << "Creating lastchange field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_construction("construction", OFTString);
        layer_ways_field_construction.SetWidth(7);
        if (m_layer_ways->CreateField(&layer_ways_field_construction)
                != OGRERR_NONE) {
            cerr << "Creating construction field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_width_error("width_error", OFTString);
        layer_ways_field_width_error.SetWidth(6);
        if (m_layer_ways->CreateField(&layer_ways_field_width_error)
                != OGRERR_NONE) {
            cerr << "Creating width_error field in table ways failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_ways_field_tagging_error("tagging_error", OFTString);
        layer_ways_field_type.SetWidth(6);
        if (m_layer_ways->CreateField(&layer_ways_field_tagging_error)
                != OGRERR_NONE) {
            cerr << "Creating tagging_error field in table ways failed." << endl;
            exit(1);
        }

        ogrerr = m_layer_ways->StartTransaction();
        if (ogrerr != OGRERR_NONE) {
            cerr << "Creating ways table failed." << endl;
            cerr << "OGRERR: " << ogrerr << endl;
            exit(1);
        }

        /*---- TABLE NODES ----*/
        m_layer_nodes = m_data_source->CreateLayer("nodes", &sparef,
                                                   wkbPoint, nullptr);
        if (!m_layer_nodes) {
            cerr << "Layer nodes creation failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_id("node_id", OFTString);
        layer_nodes_field_id.SetWidth(12);
        if (m_layer_nodes->CreateField(&layer_nodes_field_id) != OGRERR_NONE) {
            cerr << "Creating node_id field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_specific("specific", OFTString);
        layer_nodes_field_specific.SetWidth(11);
        if (m_layer_nodes->CreateField(&layer_nodes_field_specific)
                != OGRERR_NONE) {
            cerr << "Creating id field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_direction_error("direction_error",
                                                       OFTString);
        layer_nodes_field_direction_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_direction_error)
                != OGRERR_NONE) {
            cerr << "Creating direction_error field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_name_error("name_error", OFTString);
        layer_nodes_field_name_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_name_error)
                != OGRERR_NONE) {
            cerr << "Creating name_error field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_type_error("type_error", OFTString);
        layer_nodes_field_type_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_type_error)
                != OGRERR_NONE) {
            cerr << "Creating type_error field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_spring_error("spring_error", OFTString);
        layer_nodes_field_spring_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_spring_error)
                != OGRERR_NONE) {
            cerr << "Creating spring_error field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_end_error("end_error", OFTString);
        layer_nodes_field_end_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_end_error)
                != OGRERR_NONE) {
            cerr << "Creating end_error field in table nodes failed." << endl;
            exit(1);
        }

        OGRFieldDefn layer_nodes_field_way_error("way_error", OFTString);
        layer_nodes_field_way_error.SetWidth(6);
        if (m_layer_nodes->CreateField(&layer_nodes_field_way_error)
                != OGRERR_NONE) {
            cerr << "Creating way_error field in table nodes failed." << endl;
            exit(1);
        }

        ogrerr = m_layer_nodes->StartTransaction();
        if (ogrerr != OGRERR_NONE) {
            cerr << "Creating nodes table failed." << endl;
            cerr << "OGRERR: " << ogrerr << endl;
            exit(1);
        }
    }

//    const string get_timestamp(osmium::Timestamp timestamp) {
//        string time_str = timestamp.to_iso();
//        time_str.replace(10, 1, " ");
//        time_str.replace(19, 1, "");
//        return time_str;
//    }
//
//    /***
//     * Get width as float in meter from the common formats. Detect errors
//     * within the width string.
//     * A ',' as separator dedicates an erroror, but is handled.
//     */
//    bool get_width(const char *width_chr, float &width) {
//        string width_str = width_chr;
//        bool error = false;
//
//        if (width_str.find(",") != string::npos) {
//            width_str.replace(width_str.find(","), 1, ".");
//            error = true;
//            width_chr = width_str.c_str();
//        }
//
//        char *endptr;
//        width = strtof(width_chr, &endptr);
//
//        if (endptr == width_chr) {
//            width = -1;
//        } else if (*endptr) {
//            while(isspace(*endptr)) endptr++;
//            if (!strcasecmp(endptr, "m")) {
//            } else if (!strcasecmp(endptr, "km")) {
//                width *= 1000;
//            } else if (!strcasecmp(endptr, "mi")) {
//                width *= 1609.344;
//            } else if (!strcasecmp(endptr, "nmi")) {
//                width *= 1852;
//            } else if (!strcmp(endptr, "'")) {
//                width *= 12.0 * 0.0254;
//            } else if (!strcmp(endptr, "\"")) {
//                width *= 0.0254;
//            } else if (*endptr == '\'') {
//                endptr++;
//                char *inchptr;
//                float inch = strtof(endptr, &inchptr);
//                if ((!strcmp(inchptr, "\"")) && (endptr != inchptr)) {
//                    width = (width * 12 + inch) * 0.0254;
//                } else {
//                    width = -1;
//                    error = true;
//                }
//            } else {
//                width = -1;
//                error = true;
//            }
//        }
//        return error;
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
//    void remember_way(osmium::object_id_type first_node,
//                      osmium::object_id_type last_node,
//                      const char *name, const char *type) {
//        WaterWay *wway = new WaterWay(first_node, last_node, name, type);
//        waterways.push_back(wway);
//        node_map[first_node].push_back(wway);
//        node_map[last_node].push_back(wway);
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
//    /***
//     * node_map: Contains all first_nodes and last_nodes of found waterways with
//     * the names and categories of the connected ways.
//     * error_map: Contains ids of the potential error nodes (or mouths) to be
//     * checked in pass 3.
//     * error_tree: The potential error nodes remaining after pass 3 are stored
//     * in here for a geometrical analysis in pass 5.
//     * polygon_tree: contains prepared polygons of all water polygons except of
//     * riverbanks found in pass 4. 
//     */
//    google::sparse_hash_map<osmium::object_id_type, vector<WaterWay*>> node_map;
//    google::sparse_hash_map<osmium::object_id_type, ErrorSum*> error_map;
////    geos::index::strtree::STRtree error_tree;
//    google::sparse_hash_set<geos::geom::prep::PreparedPolygon*> prepared_polygon_set;
//    google::sparse_hash_set<geos::geom::MultiPolygon*> multipolygon_set;
//    geos::index::strtree::STRtree polygon_tree;
//
//    explicit DataStorage(string outfile) :
//            output_filename(outfile) {
//        init_db();
//        node_map.set_deleted_key(-1);
//        error_map.set_deleted_key(-1);
//        prepared_polygon_set.set_deleted_key(nullptr);
//        multipolygon_set.set_deleted_key(nullptr);
//    }
//
//    ~DataStorage() {
//        m_layer_polygons->CommitTransaction();
//        m_layer_relations->CommitTransaction();
//        m_layer_ways->CommitTransaction();
//        m_layer_nodes->CommitTransaction();
//
//        OGRDataSource::DestroyDataSource(m_data_source);
//        OGRCleanupAll();
//        destroy_polygons();
//        for (auto wway : waterways) {
//            delete wway;
//        }
//    }
//
//    void insert_polygon_feature(OGRGeometry *geom, const osmium::Area &area) {
//        osmium::object_id_type way_id;
//        osmium::object_id_type relation_id;
//        if (area.from_way()) {
//            way_id = area.orig_id();
//            relation_id = 0;
//        } else {
//            way_id = 0;
//            relation_id = area.orig_id();
//        }
//
//        const char *type = TagCheck::get_polygon_type(area);
//        const char *name = TagCheck::get_name(area);
//
//        OGRFeature *feature;
//        feature = OGRFeature::CreateFeature(m_layer_polygons->GetLayerDefn());
//        if (feature->SetGeometry(geom) != OGRERR_NONE) {
//            cerr << "Failed to create geometry feature for polygon of ";
//            if (area.from_way()) cerr << "way: ";
//            else cerr << "relation: ";
//            cerr << area.orig_id() << endl;
//        }
//
//        feature->SetField("way_id", static_cast<int>(way_id));
//        feature->SetField("relation_id", static_cast<int>(relation_id));
//        feature->SetField("type", type);
//        feature->SetField("name", name);
//        feature->SetField("lastchange", 
//                          get_timestamp(area.timestamp()).c_str());
//        if (m_layer_polygons->CreateFeature(feature) != OGRERR_NONE) {
//            cerr << "Failed to create polygon feature." << endl;
//        }
//        OGRFeature::DestroyFeature(feature);
//    }
//
//    void insert_relation_feature(OGRGeometry *geom,
//                                 const osmium::Relation &relation,
//                                 bool contains_nowaterway) {
//        const char *type = TagCheck::get_way_type(relation);
//        const char *name = TagCheck::get_name(relation);
//
//        OGRFeature *feature;
//        feature = OGRFeature::CreateFeature(m_layer_relations->GetLayerDefn());
//        if (feature->SetGeometry(geom) != OGRERR_NONE) {
//            cerr << "Failed to create geometry feature for relation: "
//                 << relation.id() << endl;
//        }
//
//        feature->SetField("relation_id", static_cast<int>(relation.id()));
//        feature->SetField("type", type);
//        feature->SetField("name", name);
//        feature->SetField("lastchange",
//                          get_timestamp(relation.timestamp()).c_str());
//        if (contains_nowaterway)
//            feature->SetField("nowaterway_error", "true");
//        else
//            feature->SetField("nowaterway_error", "false");
//        if (m_layer_relations->CreateFeature(feature) != OGRERR_NONE) {
//            cerr << "Failed to create relation feature." << endl;
//        }
//        OGRFeature::DestroyFeature(feature);
//    }
//
//    void insert_way_feature(OGRGeometry *geom,
//                            const osmium::Way &way,
//                            osmium::object_id_type rel_id) {
//        const char *type = TagCheck::get_way_type(way);
//        const char *name = TagCheck::get_name(way);
//        const char *width = TagCheck::get_width(way);
//        const char *construction = TagCheck::get_construction(way);
//
//        bool width_err;
//        float w = 0;
//        width_err = get_width(width, w);
//
//        char first_node_chr[13], last_node_chr[13];
//        osmium::object_id_type first_node = way.nodes().cbegin()->ref();
//        osmium::object_id_type last_node = way.nodes().crbegin()->ref();
//        sprintf(first_node_chr, "%ld", first_node);
//        sprintf(last_node_chr, "%ld", last_node);
//
//        OGRFeature *feature;
//        feature = OGRFeature::CreateFeature(m_layer_ways->GetLayerDefn());
//        if (feature->SetGeometry(geom) != OGRERR_NONE) {
//            cerr << "Failed to create geometry feature for way: "
//                 << way.id() << endl;
//        }
//        feature->SetField("way_id", static_cast<int>(way.id()));
//        feature->SetField("type", type);
//        feature->SetField("name", name);
//        feature->SetField("firstnode", first_node_chr);
//        feature->SetField("lastnode", last_node_chr);
//        feature->SetField("relation_id", static_cast<int>(rel_id));
//        feature->SetField("lastchange", get_timestamp(way.timestamp()).c_str());
//        feature->SetField("construction", construction);
//        feature->SetField("width_error", (width_err) ? "true" : "false");
//        if (w > -1) feature->SetField("width", width2string(w).c_str());
//
//        if (m_layer_ways->CreateFeature(feature) != OGRERR_NONE) {
//            cerr << "Failed to create way feature." << endl;
//        }
//
//        remember_way(first_node, last_node, name, type);
//
//        OGRFeature::DestroyFeature(feature);
//    }
//
//    void insert_node_feature(osmium::Location location,
//                             osmium::object_id_type node_id,
//                             ErrorSum *sum) {
//        osmium::geom::OGRFactory<> ogr_factory;
//        OGRFeature *feature;
//        feature = OGRFeature::CreateFeature(m_layer_nodes->GetLayerDefn());
//        OGRPoint *point = nullptr;
//        try {
//             point = ogr_factory.create_point(location).release();
//        } catch (osmium::geometry_error) {
//            cerr << "Error at node: " << node_id << endl;
//        } catch (...) {
//            cerr << "Error at node: " << node_id << endl 
//                 << "Unexpected error" << endl;
//        }
//        char id_chr[12];
//        sprintf(id_chr, "%ld", node_id);
//
//        if ((point) && (feature->SetGeometry(point) != OGRERR_NONE)) {
//            cerr << "Failed to create geometry feature for node: "
//                 << node_id << endl;
//        }
//        feature->SetField("node_id", id_chr);
//        if (sum->is_rivermouth()) feature->SetField("specific", "rivermouth");
//        else feature->SetField("specific", (sum->is_outflow()) ? "outflow": "");
//        feature->SetField("direction_error",
//                          (sum->is_direction_error()) ? "true" : "false");
//        feature->SetField("name_error",
//                          (sum->is_name_error()) ? "true" : "false");
//        feature->SetField("type_error",
//                          (sum->is_type_error()) ? "true" : "false");
//        feature->SetField("spring_error",
//                          (sum->is_spring_error()) ? "true" : "false");
//        feature->SetField("end_error",
//                          (sum->is_end_error()) ? "true" : "false");
//        feature->SetField("way_error",
//                          (sum->is_way_error()) ? "true" : "false");
//
//        if (m_layer_nodes->CreateFeature(feature) != OGRERR_NONE) {
//            cerr << "Failed to create node feature." << endl;
//        } else {
//            OGRFeature::DestroyFeature(feature);
//        }
//        if (point) OGRGeometryFactory::destroyGeometry(point);
//    }
//
//    /***
//     * unused: Change boolean value in already inserted rows.
//     */
//    void change_bool_feature(char table, const long fid, const char *field,
//                             const char *value, char *error_advice) {
//        OGRFeature *feature;
//        OGRLayer *layer;
//        switch (table) {
//            case 'r':
//                layer = m_layer_relations;
//                break;
//            case 'w':
//                layer = m_layer_ways;
//                break;
//            case 'n':
//                layer = m_layer_nodes;
//                break;
//            default:
//                cerr << "change_bool_feature expects {'r','w','n'}" <<
//                        " = {relations, ways, nodes}" << endl;
//                exit(1);
//        }
//        feature = layer->GetFeature(fid);
//        feature->SetField(field, value);
//        if (layer->SetFeature(feature) != OGRERR_NONE) {
//            cerr << "Failed to change boolean field " << error_advice << endl;
//        }
//        OGRFeature::DestroyFeature(feature);
//    }
//
////    /***
////     * Insert the error nodes remaining after first indicate false positives
////     * in pass 3 into the error_tree.
////     * FIXME: memory for point isn't free'd
////     */
////    void init_tree(location_handler_type &location_handler) {
////        osmium::geom::GEOSFactory<> geos_factory;
////        geos::geom::Point *point;
////        for (auto& node : error_map) {
////            if (!(node.second->is_rivermouth()) &&
////                  (!(node.second->is_outflow()))) {
////                point = geos_factory.create_point(
////                        location_handler.get_node_location(node.first))
////                        .release();
////                error_tree.insert(point->getEnvelopeInternal(),
////                                  (osmium::object_id_type *) &(node.first));
////            }
////        }
////        if (error_map.size() == 0) {
////            geos::geom::GeometryFactory org_geos_factory;
////            geos::geom::Coordinate coord(0, 0);
////            point = org_geos_factory.createPoint(coord);
////            error_tree.insert(point->getEnvelopeInternal(), 0);
////        }
////    }
//
//    /***
//     * Insert the error nodes into the nodes table.
//     */
//    void insert_error_nodes(location_handler_type &location_handler) {
//        osmium::Location location;
//        for (auto node : error_map) {
//            node.second->switch_poss();
//            osmium::object_id_type node_id = node.first;
//            location = location_handler.get_node_location(node_id);
//            insert_node_feature(location, node_id, node.second);
//            delete node.second;
//        }
//    }

};

#endif /* DATASTORAGE_HPP_ */
