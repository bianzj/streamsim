//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_RT_H
#define FIELD_RADIOSITY_RT_H

#include "structs.h"
#include "utils.h"
#include "scifuns.h"
#include "../radiosity/radiosityio.h"
#include "../radiosityeb/radiosityebio.h"

class RT {
public:
    RT() = default;

    int check0(Facet &facet, Facet &facet0, int ith);
    int check1(Facet &facet, Facet &facet0, int ith, int jth);
//    void add(int ics0,int ics,int npoly);
    void addnew(int ics0,int ics,int npoly, std::vector<FacetVF> & facetvfs);



    void directproject(std::shared_ptr<RadiosityIO> &modelio,float tempsza,float tempsaa);
    void diffuseproject(std::shared_ptr<RadiosityIO> &modelio);
    void radiosity(std::shared_ptr<RadiosityIO> &modelio);
    void project(std::shared_ptr<RadiosityIO> &modelio, float zen_d, float azim_d, bool isreverse, bool isdirect, bool isdiffuse,
                 std::vector<int> &pic, std::vector<int> &dint, std::vector<int> &pint);
    void polyproj0(std::shared_ptr<RadiosityIO> &modelio,int pol[3][2],int ics, float farea, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0  );
    void polyproj1(std::shared_ptr<RadiosityIO> &modelio,int pol[3][2],int ics, float farea, int miny, int maxy, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0  );
    void foreground(std::shared_ptr<RadiosityIO> &modelio, std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint  );
    void background(std::shared_ptr<RadiosityIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint  );
    void writedirectvf(std::shared_ptr<RadiosityIO> &mio);
    void writediffusevf(std::shared_ptr<RadiosityIO> &mio,std::vector<int> pintsum);
    void writediffusetable(std::shared_ptr<RadiosityIO> &mio  );
    void writepoly(std::shared_ptr<RadiosityIO> &modelio);
    void writerad(std::shared_ptr<RadiosityIO> &modelio);
    void readdata(std::shared_ptr<RadiosityIO> &modelio);



    void directproject(std::shared_ptr<RadiosityEBIO> &modelio,float tempsza,float tempsaa);
    void diffuseproject(std::shared_ptr<RadiosityEBIO> &modelio);
    void radiosity(std::shared_ptr<RadiosityEBIO> &modelio);
    void project(std::shared_ptr<RadiosityEBIO> &modelio, float zen_d, float azim_d, bool isreverse, bool isdirect, bool isdiffuse,
                 std::vector<int> &pic, std::vector<int> &dint, std::vector<int> &pint);
    void polyproj0(std::shared_ptr<RadiosityEBIO> &modelio,int pol[3][2],int ics, float farea, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0  );
    void polyproj1(std::shared_ptr<RadiosityEBIO> &modelio,int pol[3][2],int ics, float farea, int miny, int maxy, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0  );
    void foreground(std::shared_ptr<RadiosityEBIO> &modelio, std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint  );
    void background(std::shared_ptr<RadiosityEBIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint  );
    void writedirectvf(std::shared_ptr<RadiosityEBIO> &mio);
    void writediffusevf(std::shared_ptr<RadiosityEBIO> &mio,std::vector<int> pintsum);
    void writediffusetable(std::shared_ptr<RadiosityEBIO> &mio  );
    void writepoly(std::shared_ptr<RadiosityEBIO> &modelio);
    void writerad(std::shared_ptr<RadiosityEBIO> &modelio);
    void readdata(std::shared_ptr<RadiosityEBIO> &modelio);

    void netrad_shortwave(std::shared_ptr<RadiosityEBIO> &modelio);
    void netrad_longwave(std::shared_ptr<RadiosityEBIO> &modelio);
    void radiosity_vnet(std::shared_ptr<RadiosityEBIO> &modelio, int kwave, std::vector<glm::vec2> &result);
    void radiosity_tnet(std::shared_ptr<RadiosityEBIO> &modelio, int kwave);


};


#endif //FIELD_RADIOSITY_RT_H
