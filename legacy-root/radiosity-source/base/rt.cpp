//
// Created by bianzunjian on 2024/3/21.
//

#include "rt.h"

int RT::check0(Facet &facet, Facet &facet0, int ith)
{
    //ics????????????ics0???��?ics????????
    float x1,y1,z1; //?????????????
    int ilot=0,nl=0,check=0;
    x1=facet.points[ith].x;
    y1=facet.points[ith].y;
    z1=facet.points[ith].z;

    for(int j=0;j<3;j++)  //???????????????xy?????????1???pass??0???use
    {;
        if((abs(facet0.points[j].x - x1)<0.001) && (abs(facet0.points[j].y - y1)<0.001) && (abs(facet0.points[j].z - z1)<0.001))
        {
            check=1;
            return check;
        }
    }
    return check;
}


int RT::check1(Facet &facet, Facet &facet0, int ith, int jth)
{

    //??????????????2??????��???
    float x1,x2,y1,y2,z1,z2;
    int ilot=0,nl=0,check=0;
    //??1
    x1 = facet.points[ith].x;
    y1 = facet.points[ith].y;
    z1 = facet.points[ith].z;
    //??2
    x2 = facet.points[jth].x;
    y2 = facet.points[jth].y;
    z2 = facet.points[jth].z;

    //???????????????????????????��?????????2????????pass???????1??????0??????use
    for(int j=0;j<3;j++)
    {
        if((abs(facet0.points[j].x - x1)<0.001) && (abs(facet0.points[j].y - y1)<0.001 ))
        {
            check = check+1;
        }
        if((abs(facet0.points[j].x - x2)<0.001) && (abs(facet0.points[j].y - y2)<0.001 ))
        {
            check = check+1;
        }
    }
    return check;
}


/*
void RT::add(int ics0,int ics,int npoly, std::vector<int> na, std::vector<int> alist)
{
    int ii=0,jj=0,iloc=0,k0,k;
    //  int aics0,aics,ioffs,ilast,is;
    //?????��??0?????????????????????
    ics = -ics;
    k0 = abs(ics0);
    k = abs(ics);
    if (ics0 < 0) k0 = abs(ics0)+npoly;
    if (ics < 0 ) k = abs(ics)+npoly;
    if (k0 < k)
    {
        ii = k0-1;
        jj = k-1;
    }
    else
    {
        ii=k-1;
        jj=k0-1;
    }


    iloc = alist[ii]+(jj-ii-1); //????????????��??��?????????????????????????????????????????50???????????????????????
    na[iloc] = na[iloc]+1; //????????1??
}
*/

void RT::addnew(int ics0,int ics,int npoly, std::vector<FacetVF> & facetvfs) {

    int i0, ii0, i, ii;
    //  int aics0,aics,ioffs,ilast,is;
    //?????��??0?????????????????????
    ics = -ics;
    i0 = abs(ics0) - 1;
    i = abs(ics) - 1;
    if (ics0 < 0) {
        ii0 = abs(ics0) + npoly;
        int loci = 0;
        int *it = std::find(facetvfs[i].ja2, facetvfs[i].ja2 + NJ, ii0);
        if (it == facetvfs[i].ja2 + NJ) {
            int jsum = facetvfs[i].jsum2;
            loci = jsum;
            facetvfs[i].ja2[loci] = ii0;
            facetvfs[i].jna2[loci]++;
            facetvfs[i].jsum2++;

            if(facetvfs[i].jsum2>=NJ)
            {
                facetvfs[i].jsum2 = NJ-1;
                int a = 10;
            }
        } else {
            loci = std::distance(facetvfs[i].ja2, it);
            facetvfs[i].jna2[loci]++;
        }

    } else {
        ii0 = abs(ics0);
        int loci = 0;
        int *it = std::find(facetvfs[i].ja1, facetvfs[i].ja1 + NJ, ii0);
        if (it == facetvfs[i].ja1 + NJ) {
            int jsum = facetvfs[i].jsum1;
            loci = jsum;
            facetvfs[i].ja1[loci] = ii0;
            facetvfs[i].jna1[loci]++;
            facetvfs[i].jsum1++;
            if(facetvfs[i].jsum1>=NJ)
            {
                facetvfs[i].jsum1 = NJ-1;
                int a = 10;
            }
        } else {
            loci = std::distance(facetvfs[i].ja1, it);
            facetvfs[i].jna1[loci]++;
        }
    }

    if (ics < 0){
        ii = abs(ics) + npoly;
        int locj = 0;
        int *jt = std::find(facetvfs[i0].ja2, facetvfs[i0].ja2 + NJ, ii);
        if (jt == facetvfs[i0].ja2 + NJ) {
            int jsum = facetvfs[i0].jsum2;
            locj = jsum;
            facetvfs[i0].ja2[locj] = ii;
            facetvfs[i0].jna2[locj]++;
            facetvfs[i0].jsum2++;
            if(facetvfs[i0].jsum2>=NJ)
            {
                facetvfs[i0].jsum2 = NJ-1;
                int a = 10;
            }
        } else {
            locj = std::distance(facetvfs[i0].ja2, jt);
            facetvfs[i0].jna2[locj]++;
        }
    }
    else {
        ii = abs(ics);
        int locj = 0;
        int *jt = std::find(facetvfs[i0].ja1, facetvfs[i0].ja1 + NJ, ii);
        if (jt == facetvfs[i0].ja1 + NJ) {
            int jsum = facetvfs[i0].jsum1;
            locj = jsum;
            facetvfs[i0].ja1[locj] = ii;
            facetvfs[i0].jna1[locj]++;
            facetvfs[i0].jsum1++;
            if(facetvfs[i0].jsum1>=NJ)
            {
                facetvfs[i0].jsum1 = NJ-1;
                int a = 10;
            }
        } else {
            locj = std::distance(facetvfs[i0].ja1, jt);
            facetvfs[i0].jna1[locj]++;
        }
    }



//    facetvfs[ii].ja[jsum] = jj;




//    iloc = alist[ii]+(jj-ii-1); //????????????��??��?????????????????????????????????????????50???????????????????????
//    na[iloc] = na[iloc]+1; //????????1??
}




void RT::directproject(std::shared_ptr<RadiosityIO> &modelio, float zenith, float azimuth) {

    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;

    int npoly = modelio->m_npoly;
    int npic = MXPIC*MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pinttemp = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<int> na;
    std::vector<int> alist;

    float zenith_d = zenith*RD;
    float azimuth_d = azimuth*RD;
    bool isreverse, isdirect, isdiffuse;

    isreverse = 0;
    isdirect = 1;
    isdiffuse = 0;
    project(modelio,zenith_d,azimuth_d,isreverse,isdirect,isdiffuse,pic, dint,pint );


    if(modelio->isInfinite) {
//        std::fill(pic0.begin(), pic0.end(), 0);
        isreverse = 1;
        isdirect = 0;
        isdiffuse = 0;
        project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);
        isreverse = 0;
        isdirect = 1;
        isdiffuse = 0;
        foreground(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint);
    }

    float xxx,zh[3];
    int ics = 0;
    zh[0] = sin(zenith_d)*cos(azimuth_d);
    zh[1] = sin(zenith_d)*sin(azimuth_d);
    zh[2] = cos(zenith_d);
    for (int i = 0; i < npoly; i++)
    {

        xxx = facets[i].pnorm.x * zh[0] + facets[i].pnorm.y * zh[1] + facets[i].pnorm.z * zh[2];
        xxx = fabs(xxx); //??????????��?????????????????????????????��???????????
        facetvfs[i].xxx = xxx;

        if(xxx>1)
        {
            int a = 10;
        }

        if (pint[i] == 0)
        {
            facetvfs[i].fsunlit[0] = 0;
            facetvfs[i].directvf[0] = 0;
        } //?????��?????????????????
        else
        {
            facetvfs[i].fsunlit[0] = 1.0 * dint[i] / pint[i];
            if (dint[i] == 0)
            {

                facetvfs[i].directvf[0] = 0;
            }
            else
            {
                facetvfs[i].directvf[0] = facetvfs[i].fsunlit[0] * xxx;
            }
        } //??????????????????????

        ics = i + npoly;
        if (pint[ics] == 0)
        {
            facetvfs[i].fsunlit[1] = 0;
            facetvfs[i].directvf[1] = 0;
        } //?????��?????????????????
        else
        {
            facetvfs[i].fsunlit[1] = 1.0 * dint[ics] / pint[ics];
            if (dint[ics] == 0)
            {
                facetvfs[i].directvf[1] = 0;
            }
            else
            {
                facetvfs[i].directvf[1] = facetvfs[i].fsunlit[1] * xxx;
            }
        } //??????????????????????
    }


    writedirectvf(modelio);

}

void RT::diffuseproject(std::shared_ptr<RadiosityIO> &modelio){
    float zenith_d, azimuth_d;
    float zen, azim;
    auto npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    int npic = MXPIC * MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pintsum = std::vector<int>(npoly*2,0);
    std::vector<int> dintsum = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
//    long long na_size = uint32_t(npoly)*(2*npoly+1);
    std::vector<int> na;
    std::vector<int> alist;


//    for (int i = 0, sum = 0; i < 2 * npoly; i++) {
//        alist[i] = sum; //???i????????????��????????
//        sum = sum + (2 * npoly - 1 - i);
//    }

    std::cout << "diffuse view factor:" << std::endl;
    float zh[3];
    bool isreverse, isdirect, isdiffuse;
    for (int in = 0; in < NSKY; in++) {

        zenith_d = modelio->skyvza[in];
        azimuth_d = modelio->skyvaa[in];
        zen = zenith_d * 180 / 3.1415;
        azim = azimuth_d * 180 / 3.1415;
        //?????????????��???????
        zh[0] = sin(zenith_d)*cos(azimuth_d);
        zh[1] = sin(zenith_d)*sin(azimuth_d);
        zh[2] = cos(zenith_d);

        std::cout << in + 1 << ": " << zen << " " << azim << std::endl;

        isreverse = 0;
        isdirect = 1;
        isdiffuse = 0;
        project(modelio,zenith_d,azimuth_d,isreverse,isdirect,isdiffuse,pic, dint,pint );

//        std::string outfileName2 = modelio->projectDir + "/med/pic.tif";
//        double trans[6];
//        Utils::saveImage1(outfileName2,pic,MXPIC,MYPIC,1," ",trans);

        if(modelio->isInfinite) {
            isreverse = 0;
            isdirect = 1;
            isdiffuse = 0;
            background(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint); //??????????��???

            isreverse = 0;
            isdirect = 0;
            isdiffuse = 1;
            project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);

            isreverse = 1;
            isdirect = 0;
            isdiffuse = 0;
            project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);

            isreverse = 0;
            isdirect = 1;
            isdiffuse = 0;
            foreground(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint);
        }

        for (int j = 0; j < 2 * npoly; j++) {
            pintsum[j] = pintsum[j] + pint[j];
            dintsum[j] = dintsum[j] + dint[j];
        }
    }

    for (int i = 0; i < npoly; i++)
    {
        int ics = i + npoly;
        facetvfs[i].pintsum[0] = pintsum[i];
        facetvfs[i].pintsum[1] = pintsum[ics];
        //float xxx = vfacetio[ii]->pnorm[0] * zh[0] + vfacetio[ii]->pnorm[1] * zh[1] + vfacetio[ii]->pnorm[2] * zh[2];
        if (pintsum[i] <= 0)
            facetvfs[i].diffusevf[0] = 0;
        else
            facetvfs[i].diffusevf[0] = 1.0 *dintsum[i] / pintsum[i];

        if (pintsum[ics] <= 0)
            facetvfs[i].diffusevf[1] = 0;
        else
            facetvfs[i].diffusevf[1] = 1.0 *dintsum[ics] / pintsum[ics];
    }

    writediffusevf(modelio,pintsum);
    //writediffusetable(modelio );
}

void RT::project(std::shared_ptr<RadiosityIO> &modelio, float zen_d, float azim_d, bool isreverse, bool isdirect, bool isdiffuse,
                 std::vector<int> &pic, std::vector<int> &dint, std::vector<int> &pint) {

    auto xvw = modelio->m_xvw;
    auto scenescale = modelio->m_scenescale;
    //????????9?????????
    float xh[3], yh[3], zh[3];
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    int npic = MXPIC*MYPIC;
    int npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    std::vector<int> pic0 = std::vector<int>(npic,0);
//    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<float> vdistance = std::vector<float>(npoly,0);
    //std::vector<int> vindex = std::vector<int>(npoly,0);
    float rr = 0;
    for (int i = 0; i < npoly; i++)
    {
        rr = 0;
        for (int j = 0; j < 3; j++) rr = rr + (facets[i].pcenter[j] - modelio->m_xvw[j])*zh[j];
        vdistance[i] = rr;
    }

//    std::vector<int> vindex(npoly,0);
//    Utils::arraysort(vindex,vdistance,npoly);
    std::vector<int> vindex = Utils::sort_index(vdistance);
    vdistance.clear();

    int ir = 0;
    int k = 0;
    float xp[3],yp[3];
    int pol[3][2];
    for(int i = 0;i<npoly;i++)
    {
        if(isreverse) ir = npoly - i -1;
        else ir = i;
        k = vindex[ir];

        for(int ii = 0;ii<3;ii++)
        {
            float xx = 0;
            float yy = 0;
            Utils::transform(xvw,facets[k].points[ii],xh,yh,xx,yy);
            xp[ii] = scenescale.a*xx + scenescale.b;
            yp[ii] = scenescale.c*yy + scenescale.d;
        }

        float farea = 0, outside = 0;
        int miny = 100000, maxy = 0;
        for (int j = 0; j < 3; j++)
        {
            //???????????????
            if (j < 3 - 1) farea = farea + (xp[j + 1] - xp[j])*(yp[j + 1] + yp[j]);
            if (j == 3 - 1) farea = farea + (xp[0] - xp[j])*(yp[0] + yp[j]);
            pol[j][0] = int(xp[j]);
            pol[j][1] = int(yp[j]);
            //?��???????
            if (pol[j][0] > MXPIC - 1 || pol[j][0] < 0) outside = 1;
            if (pol[j][1] > MYPIC - 1 || pol[j][1] < 0) outside = 1;
            if (miny > pol[j][1]) miny = pol[j][1];
            if (maxy < pol[j][1]) maxy = pol[j][1];
        }
        //???????????????��??????x??????????????
        farea = int(abs(farea)*0.5);
        if (farea <= 0 || outside == 1) continue;
        //??????????????????????????????
        auto norm = facets[k].pnorm;
        float yyy = norm[0] * zh[0] + norm[1] * zh[1] + norm[2] * zh[2];
        int kcs = k + 1;//?????????
        if(yyy <0) kcs = -kcs;

        int pointnum = 0;
        polyproj0(modelio,pol,kcs,farea,isdiffuse,pointnum,pic,pic0 );
        polyproj1(modelio,pol,kcs,farea,miny,maxy,isdiffuse,pointnum,pic,pic0 );

        if (isdirect == 1)
        {
            pint[k] = pint[k] + pointnum;
            pint[k + npoly] = pint[k + npoly] + pointnum;
        }
    }

    if(isdirect == 1)
    {
        for (int i = 0; i < MXPIC*MYPIC; i++)
        {
            if (pic[i] != 0)
            {
                k = abs(pic[i]) - 1;
                if (pic[i] < 0) k = k + npoly; //?��????

                dint[k] = dint[k] + 1;
            }
        }
    }

}



void RT::polyproj0(std::shared_ptr<RadiosityIO> &modelio,int pol[3][2],int ics, float farea, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0) {
    // pic0 is used for outline of the polygon;????0??1??????
    // pic is used for satistic;???????
    int nl = 3;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    int npoly = modelio->m_npoly;


    if (farea > nl)  //?��?????????????????????????��???
    {
        int x1, x2, y1, y2, yy1, yy2, xx1, xx2, ics0, xx, yy, check, icss;
        float slope = 0;
        //???????��??????????��??????????????????????????????????????��??????
        for (int i = 0; i < nl; i++)
        {
            //i??j???????2?????????????
            int j = 0;
            if (i < nl - 1) j = i + 1; else j = 0;
            x1 = pol[i][0];
            y1 = pol[i][1];
            x2 = pol[j][0];
            y2 = pol[j][1];

            //vertix
            //ics0???????????��????
            ics0 = pic0[x1*MYPIC + y1];
            //pic0????��????????????????
            pic0[x1*MYPIC + y1] = ics;

            //?????��???????????????????
            //?????1????pass???????0????????
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);
            //if the point has not been occupied by other polygon and itself.
            if (check == 0 && abs(ics0) != abs(ics))
            {
                //eat it
                (pointnum)++;
                pic[x1*MYPIC + y1] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    //add it
                    //????????????????????1?????????????
                    addnew(ics0, ics, npoly, facetvfs);
                }
            }
            //????????????x2,y2?????��??
            ics0 = pic[x2*MYPIC + y2];
            pic0[x2*MYPIC + y2] = ics;
            //?��?????????????
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],j);

            if (check == 0 && abs(ics0) != abs(ics))
            {
                (pointnum)++;
                pic[x2*MYPIC + y2] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }
            }
            //???????????????????????????
            //??????????????????????
            //???
            if (x1 == x2)
            {
                //????��????????
                if (y1 < y2)
                {
                    yy1 = y1; yy2 = y2;
                }
                else
                {
                    yy1 = y2; yy2 = y1;
                }
                //????????????????????��??
                for (int yy = yy1 + 1; yy < yy2; yy++)
                {
                    ics0 = pic[x1*MYPIC + yy];
                    pic0[x1*MYPIC + yy] = ics;
                    //?��???????????????????????
                    if (ics0 <= 0) check = 0;
                    else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);

                    //??check=2?????????????????????????????check??????????????
                    if (check >= 2 || abs(ics0) == abs(ics)) continue;
                    (pointnum)++;
                    pic[x1*MYPIC + yy] = ics;
                    if (ics0 != 0 && isdiffuse == 1)
                    {
                        addnew(ics0, ics,npoly, facetvfs);
                    }
                }
            }
            else if (y1 == y2) //??????????????
            {
                if (x1 < x2)
                {
                    xx1 = x1; xx2 = x2;
                }
                else
                {
                    xx1 = x2; xx2 = x1;
                }
                for (int xx = xx1 + 1; xx < xx2; xx++)
                {
                    ics0 = pic[xx*MYPIC + y1];
                    pic0[xx*MYPIC + y1] = ics;
                    //?��?
                    if (ics0 <= 0) check = 0;
                    else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                    if (check >= 2 || abs(ics0) == abs(ics)) continue;
                    //?????????????????????????
                    (pointnum)++;
                    pic[xx*MYPIC + y1] = ics;
                    if (ics0 != 0 && isdiffuse == 1)
                    {
                        addnew(ics0, ics,npoly, facetvfs);
                    }
                }
            }
            else
            {
                //????????��????????��?��???????????????
                slope = 1.0*(y2 - y1) / (x2 - x1);
                if (abs(slope) > 1) //dang ????��???
                {

                    if (y1 < y2)
                    {
                        xx1 = x1; xx2 = x2; yy1 = y1; yy2 = y2;
                    }
                    else
                    {
                        xx1 = x2; xx2 = x1; yy1 = y2; yy2 = y1;
                    }
                    float fi = xx1 + 1.0 / slope;
                    for (int yy = yy1 + 1; yy < yy2; yy++)
                    {

                        xx = round(fi);
                        ics0 = pic[xx*MYPIC + yy];
                        pic0[xx*MYPIC + yy] = ics;
                        fi = fi + 1.0 / slope;

                        if (ics0 <= 0) check = 0;
                        else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                        if (check >= 2 || abs(ics0) == abs(ics)) continue;

                        (pointnum)++;
                        pic[xx*MYPIC + yy] = ics;
                        if (ics0 != 0 && isdiffuse == 1)
                        {
                            addnew(ics0, ics,npoly, facetvfs);
                        }
                    }
                }
                else
                {
                    if (x1 < x2)
                    {
                        xx1 = x1; xx2 = x2; yy1 = y1; yy2 = y2;
                    }
                    else
                    {
                        xx1 = x2; xx2 = x1; yy1 = y2; yy2 = y1;
                    }
                    float fi = yy1 + 1.0*slope;
                    for (int xx = xx1 + 1; xx < xx2; xx++)
                    {
                        yy = round(fi);
                        ics0 = pic[xx*MYPIC + yy];
                        pic0[xx*MYPIC + yy] = ics;
                        fi = fi + 1.0*slope;
                        if (ics0 <= 0) check = 0;
                        else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                        if (check == 2 || abs(ics0) == abs(ics)) continue;
                        (pointnum)++;
                        pic[yy + xx*MYPIC] = ics;
                        if (ics0 != 0 && isdiffuse == 1)
                        {
                            addnew(ics0, ics,npoly, facetvfs);
                        }
                    }
                }
            }
        }
    }
    else
    {
        //???????????????????��?????????
        //??????????��??????
        int ics0 = 0, check = 0, icss = 0;
        for (int i = 0; i < nl; i++)
        {
            ics0 = pic[pol[i][0] * MYPIC + pol[i][1]];
            pic0[pol[i][0] * MYPIC + pol[i][1]] = ics;
//            check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);

            if (check == 0 && abs(ics0) != abs(ics))
            {
                (pointnum)++;
                pic[pol[i][0] * MYPIC + pol[i][1]] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }
            }
        }
    }
}


void RT::polyproj1(std::shared_ptr<RadiosityIO> &modelio, int (*pol)[2], int ics, float farea, int miny, int maxy, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0) {
//?????��???????????????
    int npoly = modelio->m_npoly;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    for (int yy = miny; yy <= maxy; yy++)
    {
        int num = 0, ind1 = 0, ind2 = 0, ics0 = 0, ics1 = 0;
        //??????????????????????????????
        for (int xx = 0; xx < MXPIC; xx++)
            if (pic0[yy + xx*MYPIC] == ics)
            {
                num++;
                if (num == 1) ind1 = xx;
                if (num != 1) ind2 = xx;
            }
        //??????��???2??????????????????????
        if (num > 1)
        {
            for (int xx = ind1 + 1; xx < ind2; xx++)
            {
                //????��????��??????????????
                //?��????��??pic0?????????????��??????????pic0????��?????????????
                ics0 = pic[yy + xx*MYPIC];
                if (abs(ics0) == abs(ics)) continue;
                ics1 = pic0[yy + xx*MYPIC];
                if (abs(ics1) == abs(ics)) continue;
                //???
                (pointnum)++;
                pic[yy + xx*MYPIC] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }

            }
        }
    }
}

void RT::foreground(std::shared_ptr<RadiosityIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic, float zen_d, float azim_d, bool isdirect, std::vector<int> &dint )
{

    auto dx = modelio->dx;
    auto dy = modelio->dy;
    auto scenescale = modelio->m_scenescale;
    auto npoly = modelio->m_npoly;
    //???????????
    float xh[3], yh[3], zh[3];
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    //??????????????????????
    int maxx = 0, minx = 10000, maxy = 0, miny = 10000;
    int *picbug= new int[MXPIC*MYPIC];


    for (int i = 0; i < MXPIC; i++)
        for (int j = 0; j < MYPIC; j++)
            if (pic0[i*MYPIC + j] != 0)
            {
                if (i > maxx) maxx = i;
                if (i < minx) minx = i;
                if (j < miny) miny = j;
                if (j > maxy) maxy = j;
            }
    int nblock = 10, ipp = 0;
    float xt, yt, zproj, xdis, ydis, xtp[400], ytp[400];
    std::vector<float> ztp(400,0);
    for (int i = -nblock; i <= nblock; i++)
    {
        xt = i*dx;
        for (int j = -nblock; j <= nblock; j++)
        {
            if (ipp > 350) continue;
            if (i == 0 && j == 0) continue;
            yt = j*dy;
            zproj = -xt*zh[0] - yt*zh[1];

            if (zproj > 0) continue;
            xdis = xt*xh[0] + yt*xh[1];
            if (xdis > scenescale.delx) continue;
            ydis = xt*yh[0] + yt*yh[1];
            if (ydis > scenescale.dely) continue;
            if (maxx - minx < abs(scenescale.a*xdis)) continue;
            if (maxy - miny < abs(scenescale.c*ydis)) continue;
            xtp[ipp] = scenescale.a*xdis;
            ytp[ipp] = scenescale.c*ydis;
            ztp[ipp] = -zproj;
            ipp = ipp + 1;
        }
    }
    //??????????????????????
    if (ipp <= 0) return;
//    std::vector<int> indx(ipp,0);
//    Utils::arraysort(indx, ztp, ipp);
    ztp.resize(ipp);
    std::vector<int> indx = Utils::sort_index(ztp);
    ztp.clear();


    for (int i = 0; i < MXPIC*MYPIC; i++)
    {
        picbug[i] = 0;
    }
    //???????????????????��?��?
    int k = 0, ixt, iyt, ix1, ix2, iy1, iy2, ixp, iyp, ics, ics0;
    for (int i = 0; i < ipp; i++)
    {
        k = indx[i];
        ixt = xtp[k];
        iyt = ytp[k];

        //ix1,ix2,iy1,iy2,??????????????????????????????????????????????
        ix1 = std::max(minx, minx + ixt);
        ix2 = std::min(maxx, maxx + ixt);
        iy1 = std::max(miny, miny + iyt);
        iy2 = std::min(maxy, maxy + iyt);

        if (iy1 >= MYPIC || iy2 < 0) continue;
        if (ix1 >= MXPIC || ix2 < 0) continue;
        for (int ix = ix1; ix < ix2; ix++)
        {
            ixp = ix - ixt;//ix???????????????ixp??��?????????
            for (int iy = iy1; iy < iy2; iy++)
            {
                iyp = iy - iyt;
                //??pic0??pic??????????????ics0???????��??ics?????????????
                ics = pic0[ixp*MYPIC + iyp];
                ics0 = pic[ix*MYPIC + iy];
                if (ics0 != 0 && ics != 0)
                {
                    //???????��????????
                    //????????????????????????????????????????
                    if (picbug[ix*MYPIC + iy] == 1) continue;  //???????????????????????

                    pic[ix*MYPIC + iy] = ics;

                    if (isdirect == 1 && abs(ics) != abs(ics0))
                    {
                        k = abs(ics0) - 1;
                        if (ics0 < 0) k = k + npoly;

                        picbug[ix*MYPIC + iy] = 1;
                        /*	if (k == 12390)
                                cout << "why" << endl;*/
                        //???????????????????????????????????????????????????????

                        if (dint[k] > 1)dint[k] = dint[k] - 1;

                    }
                    //?????????????????????????????????????????��??��???
                    // if(diffuse) add(ics0,-ics );

                }

            }
        }
    }

    delete [] picbug;
}

void RT::background(std::shared_ptr<RadiosityIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint)
{

    auto dx = modelio->dx;
    auto dy = modelio->dy;
    auto scenescale = modelio->m_scenescale;
    auto npoly = modelio->m_npoly;
    float xh[3],yh[3],zh[3];
    //???????
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    //??????????????????????????????
    //????????????????????????��??????????????????????
    //????��???????????????????????????��?????????????????????
    //????????????????????????????????????????
    int maxx=0,minx=10000,maxy=0,miny=10000;
    for(int i=0;i<MXPIC;i++)
        for(int j=0;j<MYPIC;j++)
            if(pic0[i*MYPIC+j]!=0)
            {
                if (i>maxx) maxx=i;
                if(i<minx) minx=i;
                if(j<miny) miny=j;
                if(j>maxy) maxy=j;
            }
    int nblock=10,ipp=0;
    //?????11*11?????????????????��???????????????????????

    float xt,yt,zproj,xdis,ydis,xtp[400],ytp[400];
    std::vector<float> ztp(400,0);
    for(int i=-nblock;i<=nblock;i++)
    {
        xt=i*dx;
        for(int j=-nblock;j<=nblock;j++)
        {
            if(ipp> 350) continue;
            if(i==0 && j==0) continue;
            yt = j*dy;
            zproj = -xt*zh[0]-yt*zh[1];

            if (zproj<0) continue;
            xdis = xt*xh[0]+yt*xh[1];
            if(xdis > scenescale.delx) continue;
            ydis = xt*yh[0]+yt*yh[1];
            if(ydis > scenescale.dely) continue;
            if(maxx-minx < abs(scenescale.a*xdis)) continue;
            if(maxy-miny < abs(scenescale.c*ydis)) continue;
            xtp[ipp]=scenescale.a*xdis;
            ytp[ipp]=scenescale.c*ydis;
            ztp[ipp]=-zproj;
            ipp=ipp+1;
        }
    }

    if (ipp <=0) return;
    //int *indx = new int[ipp];
    std::vector<int> indx(ipp,0);
    //???????????????????????????????????????
//    sortt(indx,ztp,ipp);
//    Utils::arraysort(indx,ztp,ipp);
    ztp.resize(ipp);
    std::vector<int> indxx = Utils::sort_index(ztp);
    ztp.clear();

    int k=0,ixt,iyt,ix1,ix2,iy1,iy2,ixp,iyp,ics;
    for(int i=0;i<ipp;i++)
    {
        //?????????��??
        k=indxx[i];
        ixt = xtp[k];
        iyt = ytp[k];

        ix1 = std::max(minx,minx+ixt);
        ix2 = std::min(maxx,maxx+ixt);
        iy1 = std::max(miny,miny+iyt);
        iy2 = std::min(maxy,maxy+iyt);
        if(iy1 >= MYPIC || iy2 < 0) continue;
        if(ix1 >= MXPIC || ix2 < 0) continue;
        for(int ix=ix1;ix<ix2;ix++)
        {
            ixp=ix-ixt;  //?????????????????
            for(int iy=iy1;iy<iy2;iy++)
            {
                iyp = iy-iyt;//?????????????????
                ics = pic0[ixp*MYPIC+iyp];   //???????????????????????????????????????????????
                //???��??????
                //?????????????????????????????????
                if(ics != 0)
                {
                    pic[ix*MYPIC+iy]=ics;
                }

            }
        }
    }
}

void RT::writediffusevf(std::shared_ptr<RadiosityIO> &mio, std::vector<int> pintsum) {

    auto npoly = mio->m_npoly;
    auto &facets = mio->m_facetio->facets;
    auto &facetvfs = mio->m_facetio->facetVFs;
    std::string outfileName = mio->projectDir + "/med/diffusevf.dat";
    std::ofstream outfile(outfileName.c_str());
    int ics=0;
    if (outfile.is_open())
    {
        outfile.setf(std::ios::scientific);
        outfile<<" number of polygons is:"<<std::endl;
        outfile<<npoly<<std::endl;
        outfile<<"DIFFUSE PART (no rho/tau used, fd is normalized)"<<std::endl;
        outfile<<"facet no.       fd        pint     polygon no."<<std::endl;
        for(int i=0;i<npoly;i++)
        {
            ics = i+ npoly;
            //????????????????????????????????
            outfile <<i<<" "<<facetvfs[i].diffusevf[0]<<" "<<pintsum[i]<<" "<<facets[i].psize<<" "<<i<<std::endl;
            outfile <<i<<" "<<facetvfs[i].diffusevf[1]<<" "<<pintsum[ics]<<" "<<facets[i].psize<<" "<<ics<<std::endl;
        }

        outfile.close();
    }
}

void RT::writedirectvf(std::shared_ptr<RadiosityIO> &mio){

    auto npoly = mio->m_npoly;
    auto &facets = mio->m_facetio->facets;
    auto &facetvfs = mio->m_facetio->facetVFs;
    std::string outfileName = mio->projectDir + "/med/directvf.dat";
    std::ofstream outfile(outfileName.c_str());
    int ii=0;
    if (outfile.is_open())
    {
        outfile.setf(std::ios::scientific);
        outfile<<"specular part of E; number of polygons is"<<std::endl;
        outfile<<npoly<<std::endl;
        outfile<<"sun zenith =  "<<" "<<"    azimuth ="<<" "<<std::endl;
        outfile<<"facet no., darea(n), %lit, irea(n), 2.pint(index)/nn, psize(n), polygon no."<<std::endl;
        for(int i=0;i<npoly;i++)
        {
            ii = i % npoly;
            //????????????????????????????????
            outfile <<i<<" "<<facetvfs[ii].directvf[0]<<" "<<facetvfs[ii].fsunlit[0]<<" "<<facets[i].psize<<" "<<ii<<std::endl;
            outfile <<i<<" "<<facetvfs[ii].directvf[1]<<" "<<facetvfs[ii].fsunlit[1]<<" "<<facets[i].psize<<" "<<ii<<std::endl;
        }

        outfile.close();
    }
}

void RT::writediffusetable(std::shared_ptr<RadiosityIO> &mio) {

    std::string wdir = mio->projectDir;
    auto npoly = mio->m_npoly;
    std::string outfileName1 = wdir+"/med/table1.dat";
    std::string outfileName2 = wdir+"/med/table2.dat";
    std::string outfileName3 = wdir+"/med/vfnum.dat";
    int iloc=0,ia=0;
    int isum=0;
    int *sum =  new int[2*npoly]; // for all
    // these two are temp variables
    std::vector<FacetVF> &facetvfs = mio->m_facetio->facetVFs;

    std::ofstream outfilet1(outfileName1.c_str(),std::ios::binary);
//    std::ofstream outfilet2(outfileName2.c_str(),std::ios::binary);

    outfilet1.write((char *) &facetvfs, sizeof(FacetVF)*facetvfs.size());
    outfilet1.close();
//    outfilet2.close();


}

void RT::writerad(std::shared_ptr<RadiosityIO> &modelio)
{
    std::string outfileName2 = modelio->projectDir + "/med/radflux.dat";
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto n_wave = modelio->n_wave;
    auto npoly = modelio->m_npoly;
    ///// remove(outfileName2.c_str());
    std::ofstream outfile2(outfileName2.c_str());
    if (outfile2.is_open())
    {
        outfile2 << modelio->n_wave << " wavebands" << std::endl;
        outfile2 << "facets, sunlit_, shaded_....sunlit_, shaded_..." << std::endl;
        for (int i = 0; i < npoly; i++)
        {
            //A??
            outfile2 << i << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile2 << facetrts[i].radiosu[kband].x << " " << facetrts[i].radiosh[kband].x << " ";
            outfile2 << std::endl;
            //B ??
            outfile2 << i << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile2 << facetrts[i].radiosu[kband].y << " " << facetrts[i].radiosh[kband].y << " ";
            outfile2 << std::endl;
        }
        outfile2.close();
    }
}

void RT::writepoly(std::shared_ptr<RadiosityIO> &modelio) {

    //��????????????????
    auto wdir = modelio->projectDir;
    auto npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    std::string outfileName1 = wdir + "/med/shape.dat";
    std::ofstream outfile1(outfileName1.c_str());
    if (outfile1.is_open()) {
        outfile1.setf(std::ios::scientific);
        for (int i = 0; i < npoly; i++) {
            outfile1 << facets[i].fsign<< " ";
            for (int j = 0; j < 3; j++) outfile1 << facets[i].pcenter[j]<< " ";
            for (int j = 0; j < 3; j++) outfile1 << facets[i].pnorm[j] << " ";
            outfile1 << std::endl;
        }

        outfile1.close();
    }

    //?????A?????????
//    string outfileName2 = wdir + "/Data1/pnorm.dat";
//    ofstream outfile2(outfileName2.c_str());
//    if (outfile2.is_open()) {
//        outfile2.setf(ios::scientific);
//        for (int i = 0; i < npoly; i++) {
//            for (int j = 0; j < 3; j++) outfile2 << pnorm[i][j] << " ";
//            outfile2 << endl;
//        }
//        outfile2.close();
//    }

    //??????????
//    string outfileName3 = wdir + "/Data1/fsign.dat";
//    ofstream outfile3(outfileName3.c_str());
//    if (outfile3.is_open()) {
//        outfile3.setf(ios::scientific);
//        for (int i = 0; i < npoly; i++) {
//            outfile3 << fsign[i] << " " << i << endl;
//        }
//
//        outfile3.close();
//    }
}

void RT::readdata(std::shared_ptr<RadiosityIO> &modelio){

//    std::string infile_ja = modelio->projectDir+"/med/table1.dat";
//    std::string infile_jna = modelio -> projectDir+"/med/table2.dat";
//    std::string infile_vfnum = modelio->projectDir+"/med/vfnum.dat";
//
//    int num;
//    modelio->ja = Utils::infile2num_bi(infile_ja);
//    modelio->jna = Utils::infile2num_bi(infile_jna);
//    modelio->vfum = Utils::infile2num_int(infile_vfnum, 0, 0, num);

}

void RT::radiosity(std::shared_ptr<RadiosityIO> &modelio) {

    auto npoly = modelio->m_npoly;
    auto nwave = modelio->n_wave;

    auto meshlinks = modelio->m_meshio->meshLinks;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto &thermals = modelio->m_meshio->thermals;
    auto &spectrals = modelio->m_meshio->spectrals;
    float raddif = modelio->light.diffuse;
    float raddir = modelio->light.direct;
    float sza_d = modelio->sza * RD;
    float saa_d = modelio->saa * RD;

//    auto &ja = modelio->ja;
//    auto &jna = modelio->jna;
//    auto &vfnum = modelio->vfum;

    for(int kwave = 0;kwave<nwave;kwave++) {

        auto wl = modelio->waves[kwave];

        float *eb = new float[npoly * 2];
        float Epart1, Spart1, Dpart1, Epart2, Spart2, Dpart2;
        float *Right = new float[npoly * 2];
        float *xsol = new float[npoly * 2];
        uint32_t ics = 0;


        float f = 1.3, eps = 0.001;

        if (wl < 0) {
            raddir = SCI::bemission(modelio->light.solarTemperature);
            raddif = SCI::bemission(modelio->light.skyTemperature);
            for (int i = 0; i < npoly; i++) {
                ics = i + npoly;

                int meshid = facets[i].fsign;
                int thermalid = meshlinks[meshid].thermalId;
                float Tss = thermals[thermalid].sunlitTemperature;
                float Tsh = thermals[thermalid].shadedTemperature;
                eb[i] = SCI::bemission(Tss); // sunlit
                eb[ics] = SCI::bemission(Tsh); // shaded
                if (eb[ics] < 7) {
                    int a = 10;
                }
            }
        } else if (wl < 3000 && wl > 0) {
            memset(eb, 0, npoly * 2*sizeof(float));
        } else {
            raddir = 0;
            raddif = SCI::planck(wl, modelio->light.skyTemperature);
            for (int i = 0; i < npoly; i++) {
                ics = i + npoly;

                int meshid = facets[i].fsign;
                int thermalid = meshlinks[meshid].thermalId;
                float Tss = thermals[thermalid].sunlitTemperature;
                float Tsh = thermals[thermalid].shadedTemperature;
                eb[i] = SCI::planck(wl, Tss); // sunlit
                eb[ics] = SCI::planck(wl, Tsh); // shaded

                if (std::isinf(eb[i]) || std::isinf(eb[ics])) {
                    int a = 10;
                }

                if (eb[ics] < 7) {
                    int a = 10;
                }
            }
        }

        int fs = 0;
        float crho, ctau, xsunlit;
        for (int i = 0; i < npoly; i++) {
            int meshid = facets[i].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = spectrals[spectralid * nwave + kwave].reflectance;
            ctau = spectrals[spectralid * nwave + kwave].transmittance;

            eb[i] = eb[i] * (1 - crho - ctau);
            eb[i + npoly] = eb[i + npoly] * (1 - crho - ctau);

            Dpart1 = raddif * (crho * facetvfs[i].diffusevf[0] + ctau * facetvfs[i].diffusevf[1]);
            Dpart2 = raddif * (crho * facetvfs[i].diffusevf[1] + ctau * facetvfs[i].diffusevf[0]);

            Spart1 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[0], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[1], float(0.0)));
            Spart2 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[1], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[0], float(0.0)));
            if(Spart1 > 0 || Spart2 > 0)
                int a = 10;
            if (wl < 0 || wl > 3000) {
                xsunlit = facetvfs[i].fsunlit[0] + facetvfs[i].fsunlit[1];
                Epart1 = eb[i] * xsunlit + eb[i + npoly] * (1 - xsunlit);
                Epart2 = Epart1;
            } else {
                Epart1 = 0;
                Epart2 = 0;
            }

            Right[i] = Dpart1 + Spart1 + Epart1;
            Right[i + npoly] = Dpart2 + Spart2 + Epart2;
            xsol[i] = Right[i];
            xsol[i + npoly] = Right[i + npoly];
        }


        int finish = 0, maxit = 50, it = 0, ioffs, ilast, num1, num2, pl;
        float usum1, usum2, varerr1, varerr2, ab1, ab2;
        while (finish == 0) {
            finish = 1;
            if (it > maxit) {
                std::cout << wl << ":maxit obtained" << std::endl;
                break;
            }

            for (int k = 0, kk = 0; k < npoly; k++) {
                kk = k + npoly;
                int meshid = facets[k].fsign;
                int spectralid = meshlinks[meshid].spectralId;
                crho = spectrals[spectralid * nwave + kwave].reflectance;
                ctau = spectrals[spectralid * nwave + kwave].transmittance;


//            num1 = vfnum[k * 2];
//            num2 = vfnum[k * 2 + 1];

                num1 = facetvfs[k].jsum1;
                num2 = facetvfs[k].jsum2;

                //  if(num1+num2<=0) continue;
                usum1 = 0;
                usum2 = 0;
                ioffs = 0;
                if (num1 > 0) {
                    ilast = ioffs + num1;
                    for (int j = ioffs; j < ilast; j++) {
                        pl = facetvfs[k].ja1[j] - 1;
                        usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                    }
                    usum2 = usum1;
                    usum1 = usum1 * crho;
                    usum2 = usum2 * ctau;
                    ioffs = ilast;
                }

                ioffs = 0;
                if (num2 > 0) {
                    ilast = ioffs + num2;
                    for (int j = ioffs; j < ilast; j++) {
                        pl = facetvfs[k].ja2[j] - 1;
                        usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                        usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                    }
                    ioffs = ilast;

                }
                if (fabs(facetvfs[k].pintsum[0]) < 0.5) {
                    varerr1 = 0;
                    varerr2 = 0;
                } else {
                    varerr1 = usum1 / abs(facetvfs[k].pintsum[0]);
                    varerr2 = usum2 / abs(facetvfs[k].pintsum[1]);
                }
                usum1 = Right[k] + varerr1 - xsol[k];
                xsol[k] = xsol[k] + f * usum1;
                usum2 = Right[kk] + varerr2 - xsol[kk];
                xsol[kk] = xsol[kk] + f * usum2;

                ab1 = fabs(usum1);
                ab2 = fabs(usum2);

                if (ab1 > eps || ab2 > eps) {
                    finish = 0;
                }
            }


            it = it + 1;
        }
        //////////????? calculation////////////////////////////
        float u1, u2;
        ioffs = 0;
        for (int k = 0, kk = 0; k < npoly; k++) {
            kk = k + npoly;
            int meshid = facets[k].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = spectrals[spectralid * nwave + kwave].reflectance;
            ctau = spectrals[spectralid * nwave + kwave].transmittance;

            num1 = facetvfs[k].jsum1;
            num2 = facetvfs[k].jsum2;
            //  if(num1+num2<=0) continue;
            usum1 = 0;
            usum2 = 0;
            ioffs = 0;
            if (num1 > 0) {
                ilast = ioffs + num1;
                for (int j = ioffs; j < ilast; j++) {
                    pl = facetvfs[k].ja1[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                }
                usum2 = usum1;
                usum1 = usum1 * crho;
                usum2 = usum2 * ctau;

            }
            ioffs = 0;
            if (num2 > 0) {
                ilast = ioffs + num2;
                for (int j = ioffs; j < ilast; j++) {

                    pl = facetvfs[k].ja2[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                    usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                }


            }
            if (fabs(facetvfs[k].pintsum[0]) < 0.05) {
                u1 = 0;
                u2 = 0;
            } else {
                u1 = usum1 / abs(facetvfs[k].pintsum[0]);
                u2 = usum2 / abs(facetvfs[k].pintsum[1]);
            }
            Dpart1 = raddif * facetvfs[k].diffusevf[0];
            Dpart2 = raddif * facetvfs[k].diffusevf[1];
            Spart1 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[0], float(0));
            Spart2 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[1], float(0));
            xsunlit = (facetvfs[k].fsunlit[0] + facetvfs[k].fsunlit[1]);
            if (xsunlit <= 0) xsunlit = 1000.0;
            if (xsunlit > 1) xsunlit = 1.0;


            //??????????radiositu??eb??????????A?��B????????????????????????
            facetrts[k].radiosu[kwave][0] = (Spart1 + Dpart1) * crho + (Dpart2 + Spart2) * ctau + u1 + eb[k];
            facetrts[k].radiosu[kwave][1] = (Spart1 + Dpart1) * ctau + (Dpart2 + Spart2) * crho + u2 + eb[k];

            //?????????��???????
            facetrts[k].radiosh[kwave][0] = (Dpart1) * crho + (Dpart2) * ctau + u1 + eb[k + npoly];
            facetrts[k].radiosh[kwave][1] = (Dpart1) * ctau + (Dpart2) * crho + u2 + eb[k + npoly];

            if (std::isinf(facetrts[k].radiosu[kwave][0]) || std::isinf(facetrts[k].radiosu[kwave][1])) {
                int a = 10;
            }

        }


        delete[] eb;
        delete[] Right;
        delete[] xsol;
    }
}


void RT::directproject(std::shared_ptr<RadiosityEBIO> &modelio, float zenith, float azimuth) {

    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;

    int npoly = modelio->m_npoly;
    int npic = MXPIC*MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pinttemp = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<int> na;
    std::vector<int> alist;

    float zenith_d = zenith*RD;
    float azimuth_d = azimuth*RD;
    bool isreverse, isdirect, isdiffuse;

    isreverse = 0;
    isdirect = 1;
    isdiffuse = 0;
    project(modelio,zenith_d,azimuth_d,isreverse,isdirect,isdiffuse,pic, dint,pint );


    if(modelio->isInfinite) {
//        std::fill(pic0.begin(), pic0.end(), 0);
        isreverse = 1;
        isdirect = 0;
        isdiffuse = 0;
        project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);
        isreverse = 0;
        isdirect = 1;
        isdiffuse = 0;
        foreground(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint);
    }

    float xxx,zh[3];
    int ics = 0;
    zh[0] = sin(zenith_d)*cos(azimuth_d);
    zh[1] = sin(zenith_d)*sin(azimuth_d);
    zh[2] = cos(zenith_d);
    for (int i = 0; i < npoly; i++)
    {

        xxx = facets[i].pnorm[0] * zh[0] + facets[i].pnorm[1] * zh[1] + facets[i].pnorm[2] * zh[2];
        xxx = fabs(xxx); //??????????��?????????????????????????????��???????????
        facetvfs[i].xxx = xxx;

        if(xxx>1)
        {
            int a = 10;
        }

        if (pint[i] == 0)
        {
            facetvfs[i].fsunlit[0] = 0;
            facetvfs[i].directvf[0] = 0;
        } //?????��?????????????????
        else
        {
            facetvfs[i].fsunlit[0] = 1.0 * dint[i] / pint[i];
            if (dint[i] == 0)
            {

                facetvfs[i].directvf[0] = 0;
            }
            else
            {
                facetvfs[i].directvf[0] = facetvfs[i].fsunlit[0] * xxx;
            }
        } //??????????????????????

        ics = i + npoly;
        if (pint[ics] == 0)
        {
            facetvfs[i].fsunlit[1] = 0;
            facetvfs[i].directvf[1] = 0;
        } //?????��?????????????????
        else
        {
            facetvfs[i].fsunlit[1] = 1.0 * dint[ics] / pint[ics];
            if (dint[ics] == 0)
            {
                facetvfs[i].directvf[1] = 0;
            }
            else
            {
                facetvfs[i].directvf[1] = facetvfs[i].fsunlit[1] * xxx;
            }
        } //??????????????????????
    }


    writedirectvf(modelio);

}

void RT::diffuseproject(std::shared_ptr<RadiosityEBIO> &modelio){
    float zenith_d, azimuth_d;
    float zen, azim;
    auto npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    int npic = MXPIC * MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pintsum = std::vector<int>(npoly*2,0);
    std::vector<int> dintsum = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
//    long long na_size = uint32_t(npoly)*(2*npoly+1);
    std::vector<int> na;
    std::vector<int> alist;


//    for (int i = 0, sum = 0; i < 2 * npoly; i++) {
//        alist[i] = sum; //???i????????????��????????
//        sum = sum + (2 * npoly - 1 - i);
//    }

    std::cout << "diffuse view factor:" << std::endl;
    float zh[3];
    bool isreverse, isdirect, isdiffuse;
    for (int in = 0; in < NSKY; in++) {

        zenith_d = modelio->skyvza[in];
        azimuth_d = modelio->skyvaa[in];
        zen = zenith_d * 180 / 3.1415;
        azim = azimuth_d * 180 / 3.1415;
        //?????????????��???????
        zh[0] = sin(zenith_d)*cos(azimuth_d);
        zh[1] = sin(zenith_d)*sin(azimuth_d);
        zh[2] = cos(zenith_d);

        std::cout << in + 1 << ": " << zen << " " << azim << std::endl;

        std::fill(dint.begin(),dint.end(),0);
        std::fill(pint.begin(),pint.end(),0);
        std::fill(pic.begin(),pic.end(),0);
        std::fill(pic0.begin(),pic0.end(),0);


//        std::string outfileName2 = modelio->projectDir + "/med/pic.tif";
//        double trans[6];
//        Utils::saveImage1(outfileName2,pic,MXPIC,MYPIC,1," ",trans);

        if(modelio->isInfinite) {

            isreverse = 0;
            isdirect = 1;
            isdiffuse = 0;
            project(modelio,zenith_d,azimuth_d,isreverse,isdirect,isdiffuse,pic, dint,pint );

            isreverse = 0;
            isdirect = 1;
            isdiffuse = 0;
            background(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint); //??????????��???

            isreverse = 0;
            isdirect = 0;
            isdiffuse = 1;
            project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);

            isreverse = 1;
            isdirect = 0;
            isdiffuse = 0;
            project(modelio, zenith_d, azimuth_d, isreverse, isdirect, isdiffuse, pic0, dint, pint);

            isreverse = 0;
            isdirect = 1;
            isdiffuse = 0;
            foreground(modelio, pic0, pic, zenith_d, azimuth_d, isdirect, dint);
        }else
        {
            isreverse = 0;
            isdirect = 1;
            isdiffuse = 1;
            project(modelio,zenith_d,azimuth_d,isreverse,isdirect,isdiffuse,pic, dint,pint );
        }

        for (int j = 0; j < 2 * npoly; j++) {
            pintsum[j] = pintsum[j] + pint[j];
            dintsum[j] = dintsum[j] + dint[j];
        }
    }

    for (int i = 0; i < npoly; i++)
    {
        int ics = i + npoly;
        facetvfs[i].pintsum[0] = pintsum[i];
        facetvfs[i].pintsum[1] = pintsum[ics];
        //float xxx = vfacetio[ii]->pnorm[0] * zh[0] + vfacetio[ii]->pnorm[1] * zh[1] + vfacetio[ii]->pnorm[2] * zh[2];
        if (pintsum[i] <= 0)
            facetvfs[i].diffusevf[0] = 0;
        else
            facetvfs[i].diffusevf[0] = 1.0 *dintsum[i] / pintsum[i];

        if (pintsum[ics] <= 0)
            facetvfs[i].diffusevf[1] = 0;
        else
            facetvfs[i].diffusevf[1] = 1.0 *dintsum[ics] / pintsum[ics];
    }

    writediffusevf(modelio,pintsum);
    //writediffusetable(modelio );
}

void RT::project(std::shared_ptr<RadiosityEBIO> &modelio, float zen_d, float azim_d, bool isreverse, bool isdirect, bool isdiffuse,
                 std::vector<int> &pic, std::vector<int> &dint, std::vector<int> &pint) {

    auto xvw = modelio->m_xvw;
    auto scenescale = modelio->m_scenescale;
    //????????9?????????
    float xh[3], yh[3], zh[3];
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    int npic = MXPIC*MYPIC;
    int npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    std::vector<int> pic0 = std::vector<int>(npic,0);
//    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<float> vdistance = std::vector<float>(npoly,0);
    //std::vector<int> vindex = std::vector<int>(npoly,0);
    float rr = 0;
    for (int i = 0; i < npoly; i++)
    {
        rr = 0;
        for (int j = 0; j < 3; j++) rr = rr + (facets[i].pcenter[j] - modelio->m_xvw[j])*zh[j];
        vdistance[i] = rr;
    }

//    std::vector<int> vindex(npoly,0);
//    Utils::arraysort(vindex,vdistance,npoly);
    std::vector<int> vindex = Utils::sort_index(vdistance);
    vdistance.clear();

    int ir = 0;
    int k = 0;
    float xp[3],yp[3];
    int pol[3][2];
    for(int i = 0;i<npoly;i++)
    {
        if(isreverse) ir = npoly - i -1;
        else ir = i;
        k = vindex[ir];

        for(int ii = 0;ii<3;ii++)
        {
            float xx = 0;
            float yy = 0;
            Utils::transform(xvw,facets[k].points[ii],xh,yh,xx,yy);
            xp[ii] = scenescale.a*xx + scenescale.b;
            yp[ii] = scenescale.c*yy + scenescale.d;
        }

        float farea = 0, outside = 0;
        int miny = 100000, maxy = 0;
        for (int j = 0; j < 3; j++)
        {
            //???????????????
            if (j < 3 - 1) farea = farea + (xp[j + 1] - xp[j])*(yp[j + 1] + yp[j]);
            if (j == 3 - 1) farea = farea + (xp[0] - xp[j])*(yp[0] + yp[j]);
            pol[j][0] = int(xp[j]);
            pol[j][1] = int(yp[j]);
            //?��???????
            if (pol[j][0] > MXPIC - 1 || pol[j][0] < 0) outside = 1;
            if (pol[j][1] > MYPIC - 1 || pol[j][1] < 0) outside = 1;
            if (miny > pol[j][1]) miny = pol[j][1];
            if (maxy < pol[j][1]) maxy = pol[j][1];
        }
        //???????????????��??????x??????????????
        farea = int(abs(farea)*0.5);
        if (farea <= 0 || outside == 1) continue;
        //??????????????????????????????
        auto norm = facets[k].pnorm;
        float yyy = norm[0] * zh[0] + norm[1] * zh[1] + norm[2] * zh[2];
        int kcs = k + 1;//?????????
        if(yyy <0) kcs = -kcs;

        int pointnum = 0;
        polyproj0(modelio,pol,kcs,farea,isdiffuse,pointnum,pic,pic0 );
        polyproj1(modelio,pol,kcs,farea,miny,maxy,isdiffuse,pointnum,pic,pic0 );

        if (isdirect == 1)
        {
            pint[k] = pint[k] + pointnum;
            pint[k + npoly] = pint[k + npoly] + pointnum;
        }
    }

    if(isdirect == 1)
    {
        for (int i = 0; i < MXPIC*MYPIC; i++)
        {
            if (pic[i] != 0)
            {
                k = abs(pic[i]) - 1;
                if (pic[i] < 0) k = k + npoly; //?��????

                dint[k] = dint[k] + 1;
            }
        }
    }

}



void RT::polyproj0(std::shared_ptr<RadiosityEBIO> &modelio,int pol[3][2],int ics, float farea, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0) {
    // pic0 is used for outline of the polygon;????0??1??????
    // pic is used for satistic;???????
    int nl = 3;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    int npoly = modelio->m_npoly;


    if (farea > nl)  //?��?????????????????????????��???
    {
        int x1, x2, y1, y2, yy1, yy2, xx1, xx2, ics0, xx, yy, check, icss;
        float slope = 0;
        //???????��??????????��??????????????????????????????????????��??????
        for (int i = 0; i < nl; i++)
        {
            //i??j???????2?????????????
            int j = 0;
            if (i < nl - 1) j = i + 1; else j = 0;
            x1 = pol[i][0];
            y1 = pol[i][1];
            x2 = pol[j][0];
            y2 = pol[j][1];

            //vertix
            //ics0???????????��????
            ics0 = pic0[x1*MYPIC + y1];
            //pic0????��????????????????
            pic0[x1*MYPIC + y1] = ics;

            //?????��???????????????????
            //?????1????pass???????0????????
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);
            //if the point has not been occupied by other polygon and itself.
            if (check == 0 && abs(ics0) != abs(ics))
            {
                //eat it
                (pointnum)++;
                pic[x1*MYPIC + y1] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    //add it
                    //????????????????????1?????????????
                    addnew(ics0, ics,npoly, facetvfs);
                }
            }
            //????????????x2,y2?????��??
            ics0 = pic[x2*MYPIC + y2];
            pic0[x2*MYPIC + y2] = ics;
            //?��?????????????
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],j);

            if (check == 0 && abs(ics0) != abs(ics))
            {
                (pointnum)++;
                pic[x2*MYPIC + y2] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }
            }
            //???????????????????????????
            //??????????????????????
            //???
            if (x1 == x2)
            {
                //????��????????
                if (y1 < y2)
                {
                    yy1 = y1; yy2 = y2;
                }
                else
                {
                    yy1 = y2; yy2 = y1;
                }
                //????????????????????��??
                for (int yy = yy1 + 1; yy < yy2; yy++)
                {
                    ics0 = pic[x1*MYPIC + yy];
                    pic0[x1*MYPIC + yy] = ics;
                    //?��???????????????????????
                    if (ics0 <= 0) check = 0;
                    else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);

                    //??check=2?????????????????????????????check??????????????
                    if (check >= 2 || abs(ics0) == abs(ics)) continue;
                    (pointnum)++;
                    pic[x1*MYPIC + yy] = ics;
                    if (ics0 != 0 && isdiffuse == 1)
                    {
                        addnew(ics0, ics,npoly, facetvfs);
                    }
                }
            }
            else if (y1 == y2) //??????????????
            {
                if (x1 < x2)
                {
                    xx1 = x1; xx2 = x2;
                }
                else
                {
                    xx1 = x2; xx2 = x1;
                }
                for (int xx = xx1 + 1; xx < xx2; xx++)
                {
                    ics0 = pic[xx*MYPIC + y1];
                    pic0[xx*MYPIC + y1] = ics;
                    //?��?
                    if (ics0 <= 0) check = 0;
                    else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                    if (check >= 2 || abs(ics0) == abs(ics)) continue;
                    //?????????????????????????
                    (pointnum)++;
                    pic[xx*MYPIC + y1] = ics;
                    if (ics0 != 0 && isdiffuse == 1)
                    {
                        addnew(ics0, ics,npoly, facetvfs);
                    }
                }
            }
            else
            {
                //????????��????????��?��???????????????
                slope = 1.0*(y2 - y1) / (x2 - x1);
                if (abs(slope) > 1) //dang ????��???
                {

                    if (y1 < y2)
                    {
                        xx1 = x1; xx2 = x2; yy1 = y1; yy2 = y2;
                    }
                    else
                    {
                        xx1 = x2; xx2 = x1; yy1 = y2; yy2 = y1;
                    }
                    float fi = xx1 + 1.0 / slope;
                    for (int yy = yy1 + 1; yy < yy2; yy++)
                    {

                        xx = round(fi);
                        ics0 = pic[xx*MYPIC + yy];
                        pic0[xx*MYPIC + yy] = ics;
                        fi = fi + 1.0 / slope;

                        if (ics0 <= 0) check = 0;
                        else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                        if (check >= 2 || abs(ics0) == abs(ics)) continue;

                        (pointnum)++;
                        pic[xx*MYPIC + yy] = ics;
                        if (ics0 != 0 && isdiffuse == 1)
                        {
                            addnew(ics0, ics,npoly, facetvfs);
                        }
                    }
                }
                else
                {
                    if (x1 < x2)
                    {
                        xx1 = x1; xx2 = x2; yy1 = y1; yy2 = y2;
                    }
                    else
                    {
                        xx1 = x2; xx2 = x1; yy1 = y2; yy2 = y1;
                    }
                    float fi = yy1 + 1.0*slope;
                    for (int xx = xx1 + 1; xx < xx2; xx++)
                    {
                        yy = round(fi);
                        ics0 = pic[xx*MYPIC + yy];
                        pic0[xx*MYPIC + yy] = ics;
                        fi = fi + 1.0*slope;
                        if (ics0 <= 0) check = 0;
                        else check = check1(facets[abs(ics)-1],facets[abs(ics0)-1],i,j);
                        if (check == 2 || abs(ics0) == abs(ics)) continue;
                        (pointnum)++;
                        pic[yy + xx*MYPIC] = ics;
                        if (ics0 != 0 && isdiffuse == 1)
                        {
                            addnew(ics0, ics,npoly, facetvfs);
                        }
                    }
                }
            }
        }
    }
    else
    {
        //???????????????????��?????????
        //??????????��??????
        int ics0 = 0, check = 0, icss = 0;
        for (int i = 0; i < nl; i++)
        {
            ics0 = pic[pol[i][0] * MYPIC + pol[i][1]];
            pic0[pol[i][0] * MYPIC + pol[i][1]] = ics;
//            check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);
            if(ics0 <= 0) check =0;
            else check = check0(facets[std::abs(ics)-1],facets[abs(ics0)-1],i);

            if (check == 0 && abs(ics0) != abs(ics))
            {
                (pointnum)++;
                pic[pol[i][0] * MYPIC + pol[i][1]] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }
            }
        }
    }
}


void RT::polyproj1(std::shared_ptr<RadiosityEBIO> &modelio, int (*pol)[2], int ics, float farea, int miny, int maxy, bool isdiffuse, int &pointnum,
                   std::vector<int> &pic, std::vector<int> &pic0) {
//?????��???????????????
    int npoly = modelio->m_npoly;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    for (int yy = miny; yy <= maxy; yy++)
    {
        int num = 0, ind1 = 0, ind2 = 0, ics0 = 0, ics1 = 0;
        //??????????????????????????????
        for (int xx = 0; xx < MXPIC; xx++)
            if (pic0[yy + xx*MYPIC] == ics)
            {
                num++;
                if (num == 1) ind1 = xx;
                if (num != 1) ind2 = xx;
            }
        //??????��???2??????????????????????
        if (num > 1)
        {
            for (int xx = ind1 + 1; xx < ind2; xx++)
            {
                //????��????��??????????????
                //?��????��??pic0?????????????��??????????pic0????��?????????????
                ics0 = pic[yy + xx*MYPIC];
                if (abs(ics0) == abs(ics)) continue;
                ics1 = pic0[yy + xx*MYPIC];
                if (abs(ics1) == abs(ics)) continue;
                //???
                (pointnum)++;
                pic[yy + xx*MYPIC] = ics;
                if (ics0 != 0 && isdiffuse == 1)
                {
                    addnew(ics0, ics,npoly, facetvfs);
                }

            }
        }
    }
}

void RT::foreground(std::shared_ptr<RadiosityEBIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic, float zen_d, float azim_d, bool isdirect, std::vector<int> &dint )
{

    auto dx = modelio->dx;
    auto dy = modelio->dy;
    auto scenescale = modelio->m_scenescale;
    auto npoly = modelio->m_npoly;
    //???????????
    float xh[3], yh[3], zh[3];
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    //??????????????????????
    int maxx = 0, minx = 10000, maxy = 0, miny = 10000;
    int *picbug= new int[MXPIC*MYPIC];


    for (int i = 0; i < MXPIC; i++)
        for (int j = 0; j < MYPIC; j++)
            if (pic0[i*MYPIC + j] != 0)
            {
                if (i > maxx) maxx = i;
                if (i < minx) minx = i;
                if (j < miny) miny = j;
                if (j > maxy) maxy = j;
            }
    int nblock = 10, ipp = 0;
    float xt, yt, zproj, xdis, ydis, xtp[400], ytp[400];
    std::vector<float> ztp(400,0);
    for (int i = -nblock; i <= nblock; i++)
    {
        xt = i*dx;
        for (int j = -nblock; j <= nblock; j++)
        {
            if (ipp > 350) continue;
            if (i == 0 && j == 0) continue;
            yt = j*dy;
            zproj = -xt*zh[0] - yt*zh[1];

            if (zproj > 0) continue;
            xdis = xt*xh[0] + yt*xh[1];
            if (xdis > scenescale.delx) continue;
            ydis = xt*yh[0] + yt*yh[1];
            if (ydis > scenescale.dely) continue;
            if (maxx - minx < abs(scenescale.a*xdis)) continue;
            if (maxy - miny < abs(scenescale.c*ydis)) continue;
            xtp[ipp] = scenescale.a*xdis;
            ytp[ipp] = scenescale.c*ydis;
            ztp[ipp] = -zproj;
            ipp = ipp + 1;
        }
    }
    //??????????????????????
    if (ipp <= 0) return;
//    std::vector<int> indx(ipp,0);
//    Utils::arraysort(indx, ztp, ipp);
    ztp.resize(ipp);
    std::vector<int> indx = Utils::sort_index(ztp);
    ztp.clear();


    for (int i = 0; i < MXPIC*MYPIC; i++)
    {
        picbug[i] = 0;
    }
    //???????????????????��?��?
    int k = 0, ixt, iyt, ix1, ix2, iy1, iy2, ixp, iyp, ics, ics0;
    for (int i = 0; i < ipp; i++)
    {
        k = indx[i];
        ixt = xtp[k];
        iyt = ytp[k];

        //ix1,ix2,iy1,iy2,??????????????????????????????????????????????
        ix1 = std::max(minx, minx + ixt);
        ix2 = std::min(maxx, maxx + ixt);
        iy1 = std::max(miny, miny + iyt);
        iy2 = std::min(maxy, maxy + iyt);

        if (iy1 >= MYPIC || iy2 < 0) continue;
        if (ix1 >= MXPIC || ix2 < 0) continue;
        for (int ix = ix1; ix < ix2; ix++)
        {
            ixp = ix - ixt;//ix???????????????ixp??��?????????
            for (int iy = iy1; iy < iy2; iy++)
            {
                iyp = iy - iyt;
                //??pic0??pic??????????????ics0???????��??ics?????????????
                ics = pic0[ixp*MYPIC + iyp];
                ics0 = pic[ix*MYPIC + iy];
                if (ics0 != 0 && ics != 0)
                {
                    //???????��????????
                    //????????????????????????????????????????
                    if (picbug[ix*MYPIC + iy] == 1) continue;  //???????????????????????

                    pic[ix*MYPIC + iy] = ics;

                    if (isdirect == 1 && abs(ics) != abs(ics0))
                    {
                        k = abs(ics0) - 1;
                        if (ics0 < 0) k = k + npoly;

                        picbug[ix*MYPIC + iy] = 1;
                        /*	if (k == 12390)
                                cout << "why" << endl;*/
                        //???????????????????????????????????????????????????????

                        if (dint[k] > 1)dint[k] = dint[k] - 1;

                    }
                    //?????????????????????????????????????????��??��???
                    // if(diffuse) add(ics0,-ics );

                }

            }
        }
    }

    delete [] picbug;
}

void RT::background(std::shared_ptr<RadiosityEBIO> &modelio,std::vector<int> &pic0, std::vector<int> &pic,
                    float zen_d, float azim_d, bool isdirect, std::vector<int> &dint)
{

    auto dx = modelio->dx;
    auto dy = modelio->dy;
    auto scenescale = modelio->m_scenescale;
    auto npoly = modelio->m_npoly;
    float xh[3],yh[3],zh[3];
    //???????
    xh[0] = -sin(azim_d);
    xh[1] = cos(azim_d);
    xh[2] = 0;
    yh[0] = -cos(zen_d)*cos(azim_d);
    yh[1] = -cos(zen_d)*sin(azim_d);
    yh[2] = sin(zen_d);
    zh[0] = sin(zen_d)*cos(azim_d);
    zh[1] = sin(zen_d)*sin(azim_d);
    zh[2] = cos(zen_d);

    //??????????????????????????????
    //????????????????????????��??????????????????????
    //????��???????????????????????????��?????????????????????
    //????????????????????????????????????????
    int maxx=0,minx=10000,maxy=0,miny=10000;
    for(int i=0;i<MXPIC;i++)
        for(int j=0;j<MYPIC;j++)
            if(pic0[i*MYPIC+j]!=0)
            {
                if (i>maxx) maxx=i;
                if(i<minx) minx=i;
                if(j<miny) miny=j;
                if(j>maxy) maxy=j;
            }
    int nblock=10,ipp=0;
    //?????11*11?????????????????��???????????????????????

    float xt,yt,zproj,xdis,ydis,xtp[400],ytp[400];
    std::vector<float> ztp(400,0);
    for(int i=-nblock;i<=nblock;i++)
    {
        xt=i*dx;
        for(int j=-nblock;j<=nblock;j++)
        {
            if(ipp> 350) continue;
            if(i==0 && j==0) continue;
            yt = j*dy;
            zproj = -xt*zh[0]-yt*zh[1];

            if (zproj<0) continue;
            xdis = xt*xh[0]+yt*xh[1];
            if(xdis > scenescale.delx) continue;
            ydis = xt*yh[0]+yt*yh[1];
            if(ydis > scenescale.dely) continue;
            if(maxx-minx < abs(scenescale.a*xdis)) continue;
            if(maxy-miny < abs(scenescale.c*ydis)) continue;
            xtp[ipp]=scenescale.a*xdis;
            ytp[ipp]=scenescale.c*ydis;
            ztp[ipp]=-zproj;
            ipp=ipp+1;
        }
    }

    if (ipp <=0) return;
    //int *indx = new int[ipp];
    std::vector<int> indx(ipp,0);
    //???????????????????????????????????????
//    sortt(indx,ztp,ipp);
//    Utils::arraysort(indx,ztp,ipp);
    ztp.resize(ipp);
    std::vector<int> indxx = Utils::sort_index(ztp);
    ztp.clear();

    int k=0,ixt,iyt,ix1,ix2,iy1,iy2,ixp,iyp,ics;
    for(int i=0;i<ipp;i++)
    {
        //?????????��??
        k=indxx[i];
        ixt = xtp[k];
        iyt = ytp[k];

        ix1 = std::max(minx,minx+ixt);
        ix2 = std::min(maxx,maxx+ixt);
        iy1 = std::max(miny,miny+iyt);
        iy2 = std::min(maxy,maxy+iyt);
        if(iy1 >= MYPIC || iy2 < 0) continue;
        if(ix1 >= MXPIC || ix2 < 0) continue;
        for(int ix=ix1;ix<ix2;ix++)
        {
            ixp=ix-ixt;  //?????????????????
            for(int iy=iy1;iy<iy2;iy++)
            {
                iyp = iy-iyt;//?????????????????
                ics = pic0[ixp*MYPIC+iyp];   //???????????????????????????????????????????????
                //???��??????
                //?????????????????????????????????
                if(ics != 0)
                {
                    pic[ix*MYPIC+iy]=ics;
                }

            }
        }
    }
}

void RT::writediffusevf(std::shared_ptr<RadiosityEBIO> &mio, std::vector<int> pintsum) {

    auto npoly = mio->m_npoly;
    auto &facets = mio->m_facetio->facets;
    auto &facetvfs = mio->m_facetio->facetVFs;
    std::string outfileName = mio->projectDir + "/med/diffusevf.dat";
    std::ofstream outfile(outfileName.c_str());
    int ics=0;
    if (outfile.is_open())
    {
        outfile.setf(std::ios::scientific);
        outfile<<" number of polygons is:"<<std::endl;
        outfile<<npoly<<std::endl;
        outfile<<"DIFFUSE PART (no rho/tau used, fd is normalized)"<<std::endl;
        outfile<<"facet no.       fd        pint     polygon no."<<std::endl;
        for(int i=0;i<npoly;i++)
        {
            ics = i+ npoly;
            //????????????????????????????????
            outfile <<i<<" "<<facetvfs[i].diffusevf[0]<<" "<<pintsum[i]<<" "<<facets[i].psize<<" "<<i<<std::endl;
            outfile <<i<<" "<<facetvfs[i].diffusevf[1]<<" "<<pintsum[ics]<<" "<<facets[i].psize<<" "<<ics<<std::endl;
        }

        outfile.close();
    }
}

void RT::writedirectvf(std::shared_ptr<RadiosityEBIO> &mio){

    auto npoly = mio->m_npoly;
    auto &facets = mio->m_facetio->facets;
    auto &facetvfs = mio->m_facetio->facetVFs;
    std::string outfileName = mio->projectDir + "/med/directvf.dat";
    std::ofstream outfile(outfileName.c_str());
    int ii=0;
    if (outfile.is_open())
    {
        outfile.setf(std::ios::scientific);
        outfile<<"specular part of E; number of polygons is"<<std::endl;
        outfile<<npoly<<std::endl;
        outfile<<"sun zenith =  "<<" "<<"    azimuth ="<<" "<<std::endl;
        outfile<<"facet no., darea(n), %lit, irea(n), 2.pint(index)/nn, psize(n), polygon no."<<std::endl;
        for(int i=0;i<npoly;i++)
        {
            ii = i % npoly;
            //????????????????????????????????
            outfile <<i<<" "<<facetvfs[ii].directvf[0]<<" "<<facetvfs[ii].fsunlit[0]<<" "<<facets[i].psize<<" "<<ii<<std::endl;
            outfile <<i<<" "<<facetvfs[ii].directvf[1]<<" "<<facetvfs[ii].fsunlit[1]<<" "<<facets[i].psize<<" "<<ii<<std::endl;
        }

        outfile.close();
    }
}

void RT::writediffusetable(std::shared_ptr<RadiosityEBIO> &mio) {

    std::string wdir = mio->projectDir;
    auto npoly = mio->m_npoly;
    std::string outfileName1 = wdir+"/med/table1.dat";
    std::string outfileName2 = wdir+"/med/table2.dat";
    std::string outfileName3 = wdir+"/med/vfnum.dat";
    int iloc=0,ia=0;
    int isum=0;
    int *sum =  new int[2*npoly]; // for all
    // these two are temp variables
    std::vector<FacetVF> &facetvfs = mio->m_facetio->facetVFs;

    std::ofstream outfilet1(outfileName1.c_str(),std::ios::binary);
//    std::ofstream outfilet2(outfileName2.c_str(),std::ios::binary);

    outfilet1.write((char *) &facetvfs, sizeof(FacetVF)*facetvfs.size());
    outfilet1.close();
//    outfilet2.close();


}

void RT::writerad(std::shared_ptr<RadiosityEBIO> &modelio)
{
    std::string outfileName2 = modelio->projectDir + "/med/radflux.dat";
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto n_wave = modelio->n_wave;
    auto npoly = modelio->m_npoly;
    ///// remove(outfileName2.c_str());
    std::ofstream outfile2(outfileName2.c_str());
    if (outfile2.is_open())
    {
        outfile2 << modelio->n_wave << " wavebands" << std::endl;
        outfile2 << "facets, sunlit_, shaded_....sunlit_, shaded_..." << std::endl;
        for (int i = 0; i < npoly; i++)
        {
            //A??
            outfile2 << i << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile2 << facetrts[i].radiosu[kband].x << " " << facetrts[i].radiosh[kband].x << " ";
            outfile2 << std::endl;
            //B ??
            outfile2 << i << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile2 << facetrts[i].radiosu[kband].y << " " << facetrts[i].radiosh[kband].y << " ";
            outfile2 << std::endl;
        }
        outfile2.close();
    }
}

void RT::writepoly(std::shared_ptr<RadiosityEBIO> &modelio) {

    //��????????????????
    auto wdir = modelio->projectDir;
    auto npoly = modelio->m_npoly;
    auto &facets = modelio->m_facetio->facets;
    std::string outfileName1 = wdir + "/med/shape.dat";
    std::ofstream outfile1(outfileName1.c_str());
    if (outfile1.is_open()) {
        outfile1.setf(std::ios::scientific);
        for (int i = 0; i < npoly; i++) {
            outfile1 << facets[i].fsign<< " ";
            for (int j = 0; j < 3; j++) outfile1 << facets[i].pcenter[j]<< " ";
            for (int j = 0; j < 3; j++) outfile1 << facets[i].pnorm[j] << " ";
            outfile1 << std::endl;
        }

        outfile1.close();
    }

    //?????A?????????
//    string outfileName2 = wdir + "/Data1/pnorm.dat";
//    ofstream outfile2(outfileName2.c_str());
//    if (outfile2.is_open()) {
//        outfile2.setf(ios::scientific);
//        for (int i = 0; i < npoly; i++) {
//            for (int j = 0; j < 3; j++) outfile2 << pnorm[i][j] << " ";
//            outfile2 << endl;
//        }
//        outfile2.close();
//    }

    //??????????
//    string outfileName3 = wdir + "/Data1/fsign.dat";
//    ofstream outfile3(outfileName3.c_str());
//    if (outfile3.is_open()) {
//        outfile3.setf(ios::scientific);
//        for (int i = 0; i < npoly; i++) {
//            outfile3 << fsign[i] << " " << i << endl;
//        }
//
//        outfile3.close();
//    }
}

void RT::readdata(std::shared_ptr<RadiosityEBIO> &modelio){

//    std::string infile_ja = modelio->projectDir+"/med/table1.dat";
//    std::string infile_jna = modelio -> projectDir+"/med/table2.dat";
//    std::string infile_vfnum = modelio->projectDir+"/med/vfnum.dat";
//
//    int num;
//    modelio->ja = Utils::infile2num_bi(infile_ja);
//    modelio->jna = Utils::infile2num_bi(infile_jna);
//    modelio->vfum = Utils::infile2num_int(infile_vfnum, 0, 0, num);

}

void RT::radiosity(std::shared_ptr<RadiosityEBIO> &modelio) {

    auto npoly = modelio->m_npoly;
    auto nwave = modelio->n_wave;

    auto meshlinks = modelio->m_meshio->meshLinks;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &thermals = modelio->m_meshio->thermals;
    auto &spectrals = modelio->m_meshio->spectrals;
    float raddif = modelio->light.diffuse;
    float raddir = modelio->light.direct;
    float sza_d = modelio->sza * RD;
    float saa_d = modelio->saa * RD;
    float sza = modelio->sza;

//    auto &ja = modelio->ja;
//    auto &jna = modelio->jna;
//    auto &vfnum = modelio->vfum;

    for(int kwave = 0;kwave<nwave;kwave++) {

        auto wl = modelio->waves[kwave];
        float *eb = new float[npoly * 2];
        float Epart1, Spart1, Dpart1, Epart2, Spart2, Dpart2;
        float *Right = new float[npoly * 2];
        float *xsol = new float[npoly * 2];
        uint32_t ics = 0;


        float f = 1.3, eps = 0.001;

        if (wl < 0) {
            raddir = SCI::bemission(modelio->light.solarTemperature);
            raddif = SCI::bemission(modelio->light.skyTemperature);
            for (int i = 0; i < npoly; i++) {
                ics = i + npoly;

                int meshid = facets[i].fsign;
                int thermalid = meshlinks[meshid].thermalId;
                float Tss = facetebs[i].thermals[0];
                float Tsh = facetebs[i].thermals[1];
                eb[i] = SCI::bemission(Tss); // sunlit
                eb[ics] = SCI::bemission(Tsh); // shaded
                if (eb[ics] < 7) {
                    int a = 10;
                }
            }
        } else if (wl < 3000 && wl > 0) {
            memset(eb, 0, npoly * 2*sizeof(float));
        } else {
            raddir = 0;
            raddif = SCI::planck(wl, modelio->light.skyTemperature);
            for (int i = 0; i < npoly; i++) {
                ics = i + npoly;

                int meshid = facets[i].fsign;
                int thermalid = meshlinks[meshid].thermalId;
                float Tss = facetebs[i].thermals[0];
                float Tsh = facetebs[i].thermals[1];
//            float Tss = thermals[thermalid].sunlitTemperature;
//            float Tsh = thermals[thermalid].shadedTemperature;
                eb[i] = SCI::planck(wl, Tss); // sunlit
                eb[ics] = SCI::planck(wl, Tsh); // shaded

                if (std::isinf(eb[i]) || std::isinf(eb[ics])) {
                    int a = 10;
                }

                if (eb[ics] < 7) {
                    int a = 10;
                }
            }
        }

        int fs = 0;
        float crho, ctau, xsunlit;
        for (int i = 0; i < npoly; i++) {
            int meshid = facets[i].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = spectrals[spectralid * nwave + kwave].reflectance;
            ctau = spectrals[spectralid * nwave + kwave].transmittance;

            eb[i] = eb[i] * (1 - crho - ctau);
            eb[i + npoly] = eb[i + npoly] * (1 - crho - ctau);

            Dpart1 = raddif * (crho * facetvfs[i].diffusevf[0] + ctau * facetvfs[i].diffusevf[1]);
            Dpart2 = raddif * (crho * facetvfs[i].diffusevf[1] + ctau * facetvfs[i].diffusevf[0]);
            Spart1 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[0], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[1], float(0.0)));
            Spart2 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[1], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[0], float(0.0)));

            if(sza > 85){
                Spart1 = 0;
                Spart2 = 0;
            }

            if (wl < 0 || wl > 3000) {
                xsunlit = facetvfs[i].fsunlit[0] + facetvfs[i].fsunlit[1];
                Epart1 = eb[i] * xsunlit + eb[i + npoly] * (1 - xsunlit);
                Epart2 = Epart1;
            } else {
                Epart1 = 0;
                Epart2 = 0;
            }

            Right[i] = Dpart1 + Spart1 + Epart1;
            Right[i + npoly] = Dpart2 + Spart2 + Epart2;
            xsol[i] = Right[i];
            xsol[i + npoly] = Right[i + npoly];
        }


        int finish = 0, maxit = 50, it = 0, ioffs, ilast, num1, num2, pl;
        float usum1, usum2, varerr1, varerr2, ab1, ab2;
        while (finish == 0) {
            finish = 1;
            if (it > maxit) {
                std::cout << wl << ":maxit obtained" << std::endl;
                break;
            }

            for (int k = 0, kk = 0; k < npoly; k++) {
                kk = k + npoly;
                int meshid = facets[k].fsign;
                int spectralid = meshlinks[meshid].spectralId;
                crho = spectrals[spectralid * nwave + kwave].reflectance;
                ctau = spectrals[spectralid * nwave + kwave].transmittance;


//            num1 = vfnum[k * 2];
//            num2 = vfnum[k * 2 + 1];

                num1 = facetvfs[k].jsum1;
                num2 = facetvfs[k].jsum2;

                //  if(num1+num2<=0) continue;
                usum1 = 0;
                usum2 = 0;
                ioffs = 0;
                if (num1 > 0) {
                    ilast = ioffs + num1;
                    for (int j = ioffs; j < ilast; j++) {
                        pl = facetvfs[k].ja1[j] - 1;
                        usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                    }
                    usum2 = usum1;
                    usum1 = usum1 * crho;
                    usum2 = usum2 * ctau;
                    ioffs = ilast;
                }

                ioffs = 0;
                if (num2 > 0) {
                    ilast = ioffs + num2;
                    for (int j = ioffs; j < ilast; j++) {
                        pl = facetvfs[k].ja2[j] - 1;
                        usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                        usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                    }
                    ioffs = ilast;

                }
                if (fabs(facetvfs[k].pintsum[0]) < 0.5) {
                    varerr1 = 0;
                    varerr2 = 0;
                } else {
                    varerr1 = usum1 / abs(facetvfs[k].pintsum[0]);
                    varerr2 = usum2 / abs(facetvfs[k].pintsum[1]);
                }
                usum1 = Right[k] + varerr1 - xsol[k];
                xsol[k] = xsol[k] + f * usum1;
                usum2 = Right[kk] + varerr2 - xsol[kk];
                xsol[kk] = xsol[kk] + f * usum2;

                ab1 = fabs(usum1);
                ab2 = fabs(usum2);

                if (ab1 > eps || ab2 > eps) {
                    finish = 0;
                }
            }


            it = it + 1;
        }
        //////////????? calculation////////////////////////////
        float u1, u2;
        ioffs = 0;
        for (int k = 0, kk = 0; k < npoly; k++) {
            kk = k + npoly;
            int meshid = facets[k].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = spectrals[spectralid * nwave + kwave].reflectance;
            ctau = spectrals[spectralid * nwave + kwave].transmittance;

            num1 = facetvfs[k].jsum1;
            num2 = facetvfs[k].jsum2;
            //  if(num1+num2<=0) continue;
            usum1 = 0;
            usum2 = 0;
            ioffs = 0;
            if (num1 > 0) {
                ilast = ioffs + num1;
                for (int j = ioffs; j < ilast; j++) {
                    pl = facetvfs[k].ja1[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                }
                usum2 = usum1;
                usum1 = usum1 * crho;
                usum2 = usum2 * ctau;

            }
            ioffs = 0;
            if (num2 > 0) {
                ilast = ioffs + num2;
                for (int j = ioffs; j < ilast; j++) {

                    pl = facetvfs[k].ja2[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                    usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                }


            }
            if (fabs(facetvfs[k].pintsum[0]) < 0.05) {
                u1 = 0;
                u2 = 0;
            } else {
                u1 = usum1 / abs(facetvfs[k].pintsum[0]);
                u2 = usum2 / abs(facetvfs[k].pintsum[1]);
            }
            Dpart1 = raddif * facetvfs[k].diffusevf[0];
            Dpart2 = raddif * facetvfs[k].diffusevf[1];
            if(sza > 80) {
                Spart1 = 0;
                Spart2 = 0;
            }
            else {
                Spart1 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[0], float(0));
                Spart2 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[1], float(0));
                xsunlit = (facetvfs[k].fsunlit[0] + facetvfs[k].fsunlit[1]);
                if (xsunlit <= 0) xsunlit = 1000.0;
                if (xsunlit > 1) xsunlit = 1.0;
            }

            //??????????radiositu??eb??????????A?��B????????????????????????
            facetrts[k].radiosu[kwave][0] = (Spart1 + Dpart1) * crho + (Dpart2 + Spart2) * ctau + u1 + eb[k];
            facetrts[k].radiosu[kwave][1] = (Spart1 + Dpart1) * ctau + (Dpart2 + Spart2) * crho + u2 + eb[k];

            //?????????��???????
            facetrts[k].radiosh[kwave][0] = (Dpart1) * crho + (Dpart2) * ctau + u1 + eb[k + npoly];
            facetrts[k].radiosh[kwave][1] = (Dpart1) * ctau + (Dpart2) * crho + u2 + eb[k + npoly];

            if (std::isinf(facetrts[k].radiosu[kwave][0]) || std::isinf(facetrts[k].radiosu[kwave][1])) {
                int a = 10;
            }

            if(facetrts[k].radiosu[kwave][0] > 1 || facetrts[k].radiosu[kwave][1] >1)
            {
                int a = 10;
            }
            if( facetrts[k].radiosu[kwave][0] <0 || facetrts[k].radiosu[kwave][1] <0){
                int a = 10;
            }

        }


        delete[] eb;
        delete[] Right;
        delete[] xsol;
    }
}




void RT::radiosity_vnet(std::shared_ptr<RadiosityEBIO> &modelio, int kwave, std::vector<glm::vec2> &result) {

    auto npoly = modelio->m_npoly;
    auto nwave = modelio->n_wave;
    auto wl = modelio->atomcoeff.wl[kwave];
    auto meshlinks = modelio->m_meshio->meshLinks;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto &thermals = modelio->m_meshio->thermals;
    auto &fixedSpectrals = modelio->m_meshio->fixedSpectrals;
    auto &facetebs = modelio->m_facetio->facetEBs;
    auto &meteo = modelio->meteos[modelio->m_knode];
    auto &atomcoeff = modelio->atomcoeff;
    // float raddif = atomcoeff.fesky[kwave]*meteo.Rli*0.001;
    // float raddir = atomcoeff.fesun[kwave]*meteo.Rin*0.001;
    float raddif = atomcoeff.fesky[kwave]*meteo.Rin * 0.001;
    float raddir = atomcoeff.fesun[kwave]*meteo.Rin * 0.001;
    float sza_d = modelio->sza * RD;
    float saa_d = modelio->saa * RD;
    float sza = modelio->sza;

//    auto &ja = modelio->ja;
//    auto &jna = modelio->jna;
//    auto &vfnum = modelio->vfum;


    float *eb = new float[npoly * 2];
    float Epart1, Spart1, Dpart1, Epart2, Spart2, Dpart2;
    float *Right = new float[npoly * 2];
    float *xsol = new float[npoly * 2];
    uint32_t ics = 0;


    float f = 1.3, eps = 0.001;

    if (wl<0 || kwave <0)
    {
        raddir = SCI::bemission(modelio->light.solarTemperature);
        raddif = SCI::bemission(modelio->light.skyTemperature);
        for (int i = 0; i<npoly; i++) {
            ics = i + npoly;

            int meshid = facets[i].fsign;
            int thermalid = meshlinks[meshid].thermalId;
            float Tss = thermals[thermalid].sunlitTemperature;
            float Tsh = thermals[thermalid].shadedTemperature;
            eb[i] = SCI::bemission(Tss); // sunlit
            eb[ics] = SCI::bemission(Tsh); // shaded
            if(eb[ics] < 7)
            {
                int a = 10;
            }
        }
    }
    else if (wl < 3000 && wl > 0)
    {
        memset(eb,0,npoly * 2*sizeof(float));
    }
    else
    {
        raddir = 0;
        raddif = SCI::planck(wl,modelio->light.skyTemperature);
        for (int i = 0; i<npoly; i++) {
            ics = i + npoly;

            int meshid = facets[i].fsign;
            int thermalid = meshlinks[meshid].thermalId;
            float Tss = thermals[thermalid].sunlitTemperature;
            float Tsh = thermals[thermalid].shadedTemperature;
            eb[i] = SCI::planck(wl,Tss); // sunlit
            eb[ics] = SCI::planck(wl,Tsh); // shaded
            // eb[i] = SCI::bemission(Tss); // sunlit
            // eb[ics] = SCI::bemission(Tsh); // shaded

            if( std::isinf(eb[i]) || std::isinf(eb[ics]) )
            {
                int a = 10;
            }

            if(eb[ics] < 7)
            {
                int a = 10;
            }
        }
    }

    int fs = 0;
    float crho, ctau, xsunlit;
    for (int i = 0; i<npoly; i++)
    {
        int meshid = facets[i].fsign;
        int spectralid = meshlinks[meshid].spectralId;
        crho = fixedSpectrals[spectralid].Refl_[kwave];
        ctau = fixedSpectrals[spectralid].Tran_[kwave];

        eb[i] = eb[i] * (1 - crho - ctau);
        eb[i + npoly] = eb[i + npoly] * (1 - crho - ctau);

        Dpart1 = raddif*(crho*facetvfs[i].diffusevf[0] + ctau*facetvfs[i].diffusevf[1]);
        Dpart2 = raddif*(crho*facetvfs[i].diffusevf[1] + ctau*facetvfs[i].diffusevf[0]);
        if(sza < 85) {
            Spart1 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[0], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[1], float(0.0)));
            Spart2 = raddir / cos(sza_d) * (crho * std::max(facetvfs[i].directvf[1], float(0.0)) +
                                            ctau * std::max(facetvfs[i].directvf[0], float(0.0)));
            // Spart1 = raddir / cos(sza_d) * std::max(facetvfs[i].directvf[0], float(0));
            // Spart2 = raddir / cos(sza_d) * std::max(facetvfs[i].directvf[1], float(0));
        }else
        {
            Spart1 =0;
            Spart2 = 0;
        }
        if (wl< 0 || wl>3000)
        {
            xsunlit = facetvfs[i].fsunlit[0] + facetvfs[i].fsunlit[1];
            Epart1 = eb[i] * xsunlit + eb[i + npoly] * (1 - xsunlit);
            Epart2 = Epart1;
        }
        else
        {
            Epart1 = 0;
            Epart2 = 0;
        }

        Right[i] = Dpart1 + Spart1 + Epart1;
        Right[i + npoly] = Dpart2 + Spart2 + Epart2;
        xsol[i] = Right[i];
        xsol[i + npoly] = Right[i + npoly];
    }



    int finish = 0, maxit = 50, it = 0, ioffs, ilast, num1, num2, pl;
    float usum1, usum2, varerr1, varerr2, ab1, ab2;
    while (finish == 0)
    {
        finish = 1;
        if (it > maxit)
        {
            std::cout << wl << ":maxit obtained" << std::endl;
            break;
        }

        for (int k = 0, kk = 0; k<npoly; k++)
        {
            kk = k + npoly;
            int meshid = facets[k].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = fixedSpectrals[spectralid].Refl_[kwave];
            ctau = fixedSpectrals[spectralid].Tran_[kwave];


//            num1 = vfnum[k * 2];
//            num2 = vfnum[k * 2 + 1];

            num1 = facetvfs[k].jsum1;
            num2 = facetvfs[k].jsum2;

            //  if(num1+num2<=0) continue;
            usum1 = 0;
            usum2 = 0;
            ioffs = 0;
            if (num1>0)
            {
                ilast = ioffs + num1;
                for (int j = ioffs; j<ilast; j++)
                {
                    pl = facetvfs[k].ja1[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                }
                usum2 = usum1;
                usum1 = usum1*crho;
                usum2 = usum2*ctau;
                ioffs = ilast;
            }

            ioffs = 0;
            if (num2>0)
            {
                ilast = ioffs + num2;
                for (int j = ioffs; j<ilast; j++)
                {
                    pl = facetvfs[k].ja2[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                    usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                }
                ioffs = ilast;

            }
            if (fabs(facetvfs[k].pintsum[0])<0.5)
            {
                varerr1 = 0;
                varerr2 = 0;
            }
            else
            {
                varerr1 = usum1 / abs(facetvfs[k].pintsum[0]);
                varerr2 = usum2 / abs(facetvfs[k].pintsum[1]);
            }
            usum1 = Right[k] + varerr1 - xsol[k];
            xsol[k] = xsol[k] + f*usum1;
            usum2 = Right[kk] + varerr2 - xsol[kk];
            xsol[kk] = xsol[kk] + f*usum2;

            ab1 = fabs(usum1);
            ab2 = fabs(usum2);

            if (ab1 > eps || ab2 > eps)
            {
                finish = 0;
            }
        }


        it = it + 1;
    }
    //////////????? calculation////////////////////////////
    float u1, u2;
    ioffs = 0;
    for (int k = 0, kk = 0; k<npoly; k++)
    {
        kk = k + npoly;
        int meshid = facets[k].fsign;
        int spectralid = meshlinks[meshid].spectralId;
        crho = fixedSpectrals[spectralid].Refl_[kwave];
        ctau = fixedSpectrals[spectralid].Tran_[kwave];

        num1 = facetvfs[k].jsum1;
        num2 = facetvfs[k].jsum2;
        //  if(num1+num2<=0) continue;
        usum1 = 0;
        usum2 = 0;
        ioffs = 0;
        if (num1>0)
        {
            ilast = ioffs + num1;
            for (int j = ioffs; j<ilast; j++)
            {
                pl = facetvfs[k].ja1[j] - 1;
                usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
            }
            // usum2 = usum1;
            // usum1 = usum1*crho;
            // usum2 = usum2*ctau;

        }
        ioffs = 0;
        if (num2>0)
        {
            ilast = ioffs + num2;
            for (int j = ioffs; j<ilast; j++)
            {

                pl = facetvfs[k].ja2[j] - 1;
                // usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl];
            }


        }
        if (fabs(facetvfs[k].pintsum[0])<0.05)
        {
            u1 = 0;
            u2 = 0;
        }
        else
        {
            u1 = usum1 / abs(facetvfs[k].pintsum[0]);
            u2 = usum2 / abs(facetvfs[k].pintsum[1]);
        }
        Dpart1 = raddif*facetvfs[k].diffusevf[0];
        Dpart2 = raddif*facetvfs[k].diffusevf[1];
        if(sza > 85)
        {
            Spart2 = 0;
            Spart1 = 0;
        }else {
            Spart1 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[0], float(0));
            Spart2 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[1], float(0));
            // Spart1 = raddir / cos(sza_d);
            // Spart2 = raddir / cos(sza_d);
        }
        xsunlit = (facetvfs[k].fsunlit[0] + facetvfs[k].fsunlit[1]);
        if (xsunlit <= 0) xsunlit = 1000.0;
        if (xsunlit > 1) xsunlit = 1.0;


        //??????????radiositu??eb??????????A?��B????????????????????????
//        facetrts[k].radiosu[kwave][0] = (Spart1 + Dpart1)*crho + (Dpart2 + Spart2)*ctau + u1 + eb[k];
//        facetrts[k].radiosu[kwave][1] = (Spart1 + Dpart1)*ctau + (Dpart2 + Spart2)*crho + u2 + eb[k];
//        //?????????��???????
//        facetrts[k].radiosh[kwave][0] = (Dpart1)*crho + (Dpart2)*ctau + u1 + eb[k + npoly];
//        facetrts[k].radiosh[kwave][1] = (Dpart1)*ctau + (Dpart2)*crho + u2 + eb[k + npoly];
//        if( std::isinf(facetrts[k].radiosu[kwave][0]) || std::isinf(facetrts[k].radiosu[kwave][1]) )
//        {
//            int a = 10;
//        }



        int type = meshlinks[meshid].type;
        glm::vec2 temp;
        if(type == 1)  //�������֣�
        {
            //����������Ԫ�����̫�����䣬����Ĵ������䣬�Ͷ��ɢ�������Ȼ���ȥ��������������Ӧ����һ���ģ�k��k+npoly�ֱ��ʾ���ղ��ֺ���Ӱ����
            //����������֣��ǲ�����Ҫȫ���ķ��䣿
            temp[0] = ((Spart1+Spart2)/xsunlit+(Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k];        //�˴�Ϊ���ղ��ֵľ����䣬�������ڶ̲�����Ϊ0
            // temp[0] = ((Spart1+Spart2)+(Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k];
            temp[1] = ((Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k+npoly];

            if(sza > 85){
                if(temp[0]<0 || temp[1]<0){
                    int a = 10;
                }
                temp[0] = 0;
                temp[1] = 0;
            }

        }
        else
        {
            //���������־�Ҫ���������������
            //�����������Ҳ���������������������ֻ��һ����֮��
            temp[0] = ((Spart1+Spart2)/(xsunlit)+(Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k]*2;
            temp[1] = ((Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k+npoly]*2;


            if(sza > 85){
                if(temp[0]<0 || temp[1]<0){
                    int a = 10;
                }
                temp[0] = 0;
                temp[1] = 0;
            }
        }

        result[k][0] = temp[0];
        result[k][1] = temp[1];
        facetebs[k].vnetrad[0] += temp[0];
        facetebs[k].vnetrad[1] += temp[1];
    }



    delete[] eb;
    delete[] Right;
    delete[] xsol;
}

void RT::radiosity_tnet(std::shared_ptr<RadiosityEBIO> &modelio, int kwave) {

    auto npoly = modelio->m_npoly;
    auto nwave = modelio->n_wave;
//    auto wl = -1;
    // auto wl = modelio->atomcoeff.wl[kwave];
    auto wl = 10500;

    auto meshlinks = modelio->m_meshio->meshLinks;
    auto &facets = modelio->m_facetio->facets;
    auto &facetvfs = modelio->m_facetio->facetVFs;
    auto &facetrts = modelio->m_facetio->facetRTs;
    auto &thermals = modelio->m_meshio->thermals;
    auto &fixedspectrals = modelio->m_meshio->fixedSpectrals;
    auto &facetebs = modelio->m_facetio->facetEBs;
//    float raddif = modelio->light.diffuse;
//    float raddir = modelio->light.direct;
    auto &meteo = modelio->meteos[modelio->m_knode];
    auto &atomcoeff = modelio->atomcoeff;
    float raddif = meteo.Rli;
    float raddir = 0;


    float sza_d = modelio->sza * RD;
    float saa_d = modelio->saa * RD;
    float sza = modelio->sza;

//    auto &ja = modelio->ja;
//    auto &jna = modelio->jna;
//    auto &vfnum = modelio->vfum;


    float *eb = new float[npoly * 2];
    float Epart1, Spart1, Dpart1, Epart2, Spart2, Dpart2;
    float *Right = new float[npoly * 2];
    float *xsol = new float[npoly * 2];
    uint32_t ics = 0;


    float f = 1.3, eps = 0.001;

    if (wl<0 || kwave <0)
    {
//        raddir = SCI::bemission(modelio->light.solarTemperature);
//        raddif = SCI::bemission(modelio->light.skyTemperature);
        for (int i = 0; i<npoly; i++) {
            ics = i + npoly;

            int meshid = facets[i].fsign;
            int thermalid = meshlinks[meshid].thermalId;
            float Tss = facetebs[i].thermals[0];
            float Tsh = facetebs[i].thermals[1];
            eb[i] = SCI::bemission(Tss); // sunlit
            eb[ics] = SCI::bemission(Tsh); // shaded
            if(eb[ics] < 7)
            {
                int a = 10;
            }
        }
    }
    else if (wl < 3000 && wl > 0)
    {
        memset(eb,0,npoly * 2*sizeof(float));
    }
    else
    {
        raddir = 0;
        raddif = SCI::planck(wl,modelio->light.skyTemperature);
        for (int i = 0; i<npoly; i++) {
            ics = i + npoly;

            int meshid = facets[i].fsign;
            int thermalid = meshlinks[meshid].thermalId;
            float Tss = facetebs[i].thermals[0];
            float Tsh = facetebs[i].thermals[1];
            eb[i] = SCI::planck(wl,Tss); // sunlit
            eb[ics] = SCI::planck(wl,Tsh); // shaded

            // eb[i] = SCI::bemission(Tss);
            // eb[ics] =  SCI::bemission(Tsh); // shaded
            if( std::isinf(eb[i]) || std::isinf(eb[ics]) )
            {
                int a = 10;
            }

            if(eb[ics] < 7)
            {
                int a = 10;
            }
        }
    }

    int fs = 0;
    float crho, ctau, xsunlit;
    for (int i = 0; i<npoly; i++)
    {
        int meshid = facets[i].fsign;
        int spectralid = meshlinks[meshid].spectralId;
        crho = fixedspectrals[spectralid].Refl_ir;
        ctau = fixedspectrals[spectralid].Tran_ir;

        eb[i] = eb[i] * (1 - crho - ctau);
        eb[i + npoly] = eb[i + npoly] * (1 - crho - ctau);

        Dpart1 = raddif*(crho*facetvfs[i].diffusevf[0] + ctau*facetvfs[i].diffusevf[1]);
        Dpart2 = raddif*(crho*facetvfs[i].diffusevf[1] + ctau*facetvfs[i].diffusevf[0]);
        Spart1 = raddir / cos(sza_d)*(crho*std::max(facetvfs[i].directvf[0],float(0.0)) + ctau*std::max(facetvfs[i].directvf[1],float(0.0)));
        Spart2 = raddir / cos(sza_d)*(crho*std::max(facetvfs[i].directvf[1],float(0.0)) + ctau*std::max(facetvfs[i].directvf[0],float(0.0)));
        if (wl< 0 || wl>3000 || kwave < 0)
        {
            xsunlit = facetvfs[i].fsunlit[0] + facetvfs[i].fsunlit[1];
            Epart1 = eb[i] * xsunlit + eb[i + npoly] * (1 - xsunlit);
            Epart2 = Epart1;
        }
        else
        {
            Epart1 = 0;
            Epart2 = 0;
        }

        Right[i] = Dpart1 + Spart1 + Epart1;
        Right[i + npoly] = Dpart2 + Spart2 + Epart2;
        xsol[i] = Right[i];
        xsol[i + npoly] = Right[i + npoly];
    }



    int finish = 0, maxit = 50, it = 0, ioffs, ilast, num1, num2, pl;
    float usum1, usum2, varerr1, varerr2, ab1, ab2;
    while (finish == 0)
    {
        finish = 1;
        if (it > maxit)
        {
            std::cout << wl << ":maxit obtained" << std::endl;
            break;
        }


        ioffs = 0; //��Ԫ��ؾ���ϵ������ʼֵ
        for (int k = 0, kk = 0; k<npoly; k++)
        {
            kk = k + npoly;
            int meshid = facets[k].fsign;
            int spectralid = meshlinks[meshid].spectralId;
            crho = fixedspectrals[spectralid].Refl_ir;
            ctau = fixedspectrals[spectralid].Tran_ir;


//            num1 = vfnum[k * 2];
//            num2 = vfnum[k * 2 + 1];

            num1 = facetvfs[k].jsum1;
            num2 = facetvfs[k].jsum2;

            //  if(num1+num2<=0) continue;
            usum1 = 0;
            usum2 = 0;
            ioffs = 0;
            if (num1>0)
            {
                ilast = ioffs + num1;
                for (int j = ioffs; j<ilast; j++)
                {
                    pl = facetvfs[k].ja1[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
                }
                usum2 = usum1;
                usum1 = usum1*crho;
                usum2 = usum2*ctau;
                ioffs = ilast;
            }

            ioffs = 0;
            if (num2>0)
            {
                ilast = ioffs + num2;
                for (int j = ioffs; j<ilast; j++)
                {
                    pl = facetvfs[k].ja2[j] - 1;
                    usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                    usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl] * crho;
                }
                ioffs = ilast;

            }
            if (fabs(facetvfs[k].pintsum[0])<0.5)
            {
                varerr1 = 0;
                varerr2 = 0;
            }
            else
            {
                varerr1 = usum1 / abs(facetvfs[k].pintsum[0]);
                varerr2 = usum2 / abs(facetvfs[k].pintsum[1]);
            }
            usum1 = Right[k] + varerr1 - xsol[k];
            xsol[k] = xsol[k] + f*usum1;
            usum2 = Right[kk] + varerr2 - xsol[kk];
            xsol[kk] = xsol[kk] + f*usum2;

            ab1 = fabs(usum1);
            ab2 = fabs(usum2);

            if (ab1 > eps || ab2 > eps)
            {
                finish = 0;
            }
        }


        it = it + 1;
    }
    //////////????? calculation////////////////////////////
    float u1, u2;
    ioffs = 0;
    for (int k = 0, kk = 0; k<npoly; k++)
    {
        kk = k + npoly;
        int meshid = facets[k].fsign;
        int spectralid = meshlinks[meshid].spectralId;
        crho = fixedspectrals[spectralid].Refl_ir;
        ctau = fixedspectrals[spectralid].Tran_ir;

        num1 = facetvfs[k].jsum1;
        num2 = facetvfs[k].jsum2;
        //  if(num1+num2<=0) continue;
        usum1 = 0;
        usum2 = 0;
        ioffs = 0;
        if (num1>0)
        {
            ilast = ioffs + num1;
            for (int j = ioffs; j<ilast; j++)
            {
                pl = facetvfs[k].ja1[j] - 1;
                usum1 = usum1 + facetvfs[k].jna1[j] * xsol[pl];
            }
            // usum2 = usum1;
            // usum1 = usum1*crho;
            // usum2 = usum2*ctau;

        }
        ioffs = 0;
        if (num2>0)
        {
            ilast = ioffs + num2;
            for (int j = ioffs; j<ilast; j++)
            {

                pl = facetvfs[k].ja2[j] - 1;
                // usum1 = usum1 + facetvfs[k].jna2[j] * xsol[pl] * ctau;
                usum2 = usum2 + facetvfs[k].jna2[j] * xsol[pl];
            }


        }
        if (fabs(facetvfs[k].pintsum[0])<0.05)
        {
            u1 = 0;
            u2 = 0;
        }
        else
        {
            u1 = usum1 / abs(facetvfs[k].pintsum[0]);
            u2 = usum2 / abs(facetvfs[k].pintsum[1]);
        }
        Dpart1 = raddif*facetvfs[k].diffusevf[0];
        Dpart2 = raddif*facetvfs[k].diffusevf[1];
        if(sza > 85)
        {
            Spart2 = 0;
            Spart1 = 0;
        }else {
            Spart1 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[0], float(0));
            Spart2 = raddir / cos(sza_d) * std::max(facetvfs[k].directvf[1], float(0));
        }
        xsunlit = (facetvfs[k].fsunlit[0] + facetvfs[k].fsunlit[1]);
        if (xsunlit <= 0) xsunlit = 1000.0;
        if (xsunlit > 1) xsunlit = 1.0;


        //??????????radiositu??eb??????????A?��B????????????????????????
//        facetrts[k].radiosu[kwave][0] = (Spart1 + Dpart1)*crho + (Dpart2 + Spart2)*ctau + u1 + eb[k];
//        facetrts[k].radiosu[kwave][1] = (Spart1 + Dpart1)*ctau + (Dpart2 + Spart2)*crho + u2 + eb[k];
//        //?????????��???????
//        facetrts[k].radiosh[kwave][0] = (Dpart1)*crho + (Dpart2)*ctau + u1 + eb[k + npoly];
//        facetrts[k].radiosh[kwave][1] = (Dpart1)*ctau + (Dpart2)*crho + u2 + eb[k + npoly];
//        if( std::isinf(facetrts[k].radiosu[kwave][0]) || std::isinf(facetrts[k].radiosu[kwave][1]) )
//        {
//            int a = 10;
//        }



        int type = meshlinks[meshid].type;
        if(type == 1)  //�������֣�
        {
            //����������Ԫ�����̫�����䣬����Ĵ������䣬�Ͷ��ɢ�������Ȼ���ȥ��������������Ӧ����һ���ģ�k��k+npoly�ֱ��ʾ���ղ��ֺ���Ӱ����
            //����������֣��ǲ�����Ҫȫ���ķ��䣿
            facetebs[k].tnetrad[0] = ((Spart1+Spart2)/xsunlit+(Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k]*(1-ctau-crho);
            facetebs[k].tnetrad[1] = ((Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k+npoly]*(1-ctau-crho);
        }
        else
        {
            //���������־�Ҫ���������������
            //�����������Ҳ���������������������ֻ��һ����֮��
            facetebs[k].tnetrad[0] = ((Spart1+Spart2)/(xsunlit)+(Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k]*2*(1-ctau-crho);
            facetebs[k].tnetrad[1] = ((Dpart1+Dpart2)+(u1+u2))*(1-ctau-crho)-eb[k+npoly]*2*(1-ctau-crho);


            if(sza > 85){
                if(facetebs[k].tnetrad[0] <0 || facetebs[k].tnetrad[1]<0){
                    int a = 10;
                }
            }

        }

        int a = 10;
    }



    delete[] eb;
    delete[] Right;
    delete[] xsol;
}

void RT::netrad_shortwave(std::shared_ptr<RadiosityEBIO> &modelio)
{
    double A=6.02214E23;
    double H=6.6262E-34;
    double C=299792458.0;
    float AHC0 = A*H*C;
    auto &facetebs = modelio->m_facetio->facetEBs;
    int npoly = modelio->m_npoly;
    std::vector<glm::vec2> result(npoly,glm::vec2(0));

    for(int k=0;k<npoly;k++)
    {
        facetebs[k].pnetrad[0] = 0;
        facetebs[k].pnetrad[1] = 0;
        facetebs[k].vnetrad[0] = 0;
        facetebs[k].vnetrad[1] = 0;
    }

    // std::fill(result.begin(),result.end(),glm::vec2(0,0));

    for(int kwave=0;kwave<N1;kwave++) {

        radiosity_vnet(modelio, kwave, result);
        float wl = modelio->atomcoeff.wl[kwave];
        if (wl>=400 && wl <=700)
        {
            for(int j=0; j<npoly; j++) {
                facetebs[j].pnetrad[0] = facetebs[j].pnetrad[0] + result[j][0] * wl * (1e-3) / (AHC0);
                facetebs[j].pnetrad[1] = facetebs[j].pnetrad[1] + result[j][1] * wl * (1e-3) / (AHC0);
            }
        }

    }




}

void RT::netrad_longwave(std::shared_ptr<RadiosityEBIO> &modelio)
{

//    auto &facetebs = modelio->m_facetio->facetEBs;
//    int npoly = modelio->m_npoly;
//    std::vector<glm::vec2> result(npoly,glm::vec2(0));
//    for(int kwave=0;kwave<N1;kwave++) {
//
//        std::fill(result.begin(),result.end(),glm::vec2(0));
//        radiosity_vnet(modelio, kwave, result);
//        float wl = modelio->atomcoeff.wavelength[kwave];
//
//
//    }

    // for the wide-band tir data
    radiosity_tnet(modelio,-1);
    // radiosity_tnet(modelio,10500);


}



