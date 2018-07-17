#include "oryx.h"
#include "dp_decode.h"

/** a map for dsa.port_src -> phyical port. */
struct MarvellDSAMap dsa_to_phy_map_list[] = {
	{-1, "unused"},
	{ 5, "et1500 ge5"},
	{ 6, "et1500 ge6"},
	{ 7, "et1500 ge7"},
	{ 8, "et1500 ge8"},
	{ 1, "et1500 ge1"},
	{ 2, "et1500 ge2"},
	{ 3, "et1500 ge3"},
	{ 4, "et1500 ge4"}
};

/** a map for physical port -> dsa.port_src */
struct MarvellDSAMap phy_to_dsa_map_list[] = {
	{-1, "unused"},
	{ 5, "dsa.port_src 5"},
	{ 6, "dsa.port_src 6"},
	{ 7, "dsa.port_src 7"},
	{ 8, "dsa.port_src 8"},
	{ 1, "dsa.port_src 1"},
	{ 2, "dsa.port_src 2"},
	{ 3, "dsa.port_src 3"},
	{ 4, "dsa.port_src 4"}
};

