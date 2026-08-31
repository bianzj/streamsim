//
// Created by bianzunjian on 2024/3/21.
//
#pragma once

#include "structs.h"



class FacetIO
{
public:
    FacetIO()=default;

    std::vector<Facet> facets;
    std::vector<FacetVF> facetVFs;
    std::vector<FacetRT> facetRTs;
    std::vector<FacetEB> facetEBs;


};