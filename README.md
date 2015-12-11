# pedro
##Pedestrian Routing on OSM

pedro follows a different way of creating a routing graph out of OpenStreetMap data. The main concept is to generate sidewalks and to consider human behaviour of crossing roads. The plan is also to implement a polygonal routing solution.
The extraction is written into a postgres database. To route on the graph pgRouting (https://github.com/pgRouting/pgrouting) should work (-;

This is my second project using osmium (https://github.com/osmcode/libosmium) and the GEOS API(https://trac.osgeo.org/geos/). I write pedro in the context of my bachelor thesis.

#Install

To run you need osmium, pgRouting and whose dependencies. I'm compiling with gcc (version 4.8.4 (Ubuntu 4.8.4-2ubuntu1~14.04)).

But it's not finished at all yet. 
