//
// Created by jiank on 2024/3/25.
//

#include <iomanip>
#include "virtual.h"

void Virtual::observe(std::shared_ptr<RadiosityIO> &modelio) {

    auto npoly = modelio->m_npoly;
    auto sza = modelio->sza;
    auto saa = modelio->saa;
    std::string wdir = modelio->projectDir;
    auto n_angle = modelio->n_angle;
    auto angles = modelio->angles;
    auto n_wave = modelio->n_wave;
    auto facetrts = modelio->m_facetio->facetRTs;
    auto facetvfs = modelio->m_facetio->facetVFs;

//    int *pint = new int[2*npoly];
//    int *dint = new int[2*npoly];
//    float *vs1 = new float[2*npoly];
//    float *vs = new float[2*npoly];
//    int *pic = new int[MXPIC*MYPIC];
    int npic = MXPIC * MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pintsum = std::vector<int>(npoly*2,0);
    std::vector<int> dintsum = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<int> na(npoly*(2*npoly+1),0);
    std::vector<int> alist(2*npoly);
    std::vector<int> pics = std::vector<int>(npic,0);
    std::vector<int> picv = std::vector<int>(npic,0);
    std::vector<float> vs(npoly*2,0);
    std::vector<int> dinttemp;
    std::vector<float> rad_ = std::vector<float>(npic*n_wave,0);

//    int *na=NULL;
//    int *alist=NULL;
    float sza_d, saa_d;

    sza_d = sza*RD;
    saa_d = saa*RD;

    bool isreverse, isdirect, isdiffuse;

// 得到太阳方向的PIC阵列

//    int *pics = new int[MXPIC*MYPIC];
//    int *picv = new int[MXPIC*MYPIC]; //观测方向的投影矩阵；
//    int *pic0 = new int[MXPIC*MYPIC];
//    int *dinttemp = 0;

    std::fill(pics.begin(),pics.end(),0);
    std::fill(pic0.begin(),pic0.end(),0);
    isdirect = 0;
    isreverse = 0;
    isdiffuse = 0;
//    project(pics, sza_d, saa_d, dint, pint, na, alist);
    m_pRT->project(modelio,sza_d,saa_d,isreverse,isdirect,isdiffuse,pics, dint,pint);

    isreverse = 1;
    isdiffuse = 0;
    isdirect = 0;
//    project(pic0, sza_d, saa_d, dint, pint, na, alist);
    m_pRT->project(modelio,sza_d,saa_d,isreverse,isdirect,isdiffuse,pic0, dint,pint );
//    foreground(pic0, pics, sza_d, saa_d, dinttemp, na, alist);
    m_pRT->foreground(modelio,pic0,pics,sza_d,saa_d,isdirect, dinttemp );





    //float *rad_ = new float[MXPIC*MYPIC];

    float* radsum = new float[n_wave];
    float* radtemp = new float[n_wave];
    float obs;
    //float *rad_ = new float[MXPIC*MYPIC];
    float obs_[500];
    char bvza[20], bvaa[20];
    for(int kang=0;kang<n_angle;kang++)
    {
        float vza = angles[kang].vza;
        float vaa = angles[kang].vaa;
        float vza_d = vza*RD;
        float vaa_d = vaa*RD;
        std::cout << "   " << kang << " in " << n_angle << "  VZA: " << vza << "  VAA: " << vaa << " SZA:  " << sza << " SAA: " << saa << std::endl;


        sprintf(bvza, "%d", int(vza));
        sprintf(bvaa, "%d", int(vaa));

        ///////	 得到观测方向的pic阵列
        isdirect = 0;
        isreverse = 0;
        isdiffuse = 0;

        std::fill(picv.begin(),picv.end(),0);
        std::fill(pic0.begin(),pic0.end(),0);
        std::fill(dint.begin(),dint.end(),0);
        std::fill(pint.begin(),pint.end(),0);
//        project(picv, vza_d, vaa_d, dint, pint, na, alist);
        m_pRT->project(modelio,vza_d,vaa_d,isreverse,isdirect,isdiffuse,picv, dint,pint );

//        std::string outfileName2 = wdir+"/med/picv1.dat";
//        std::ofstream outfilet2(outfileName2.c_str(),std::ios::binary);
//        outfilet2.write(reinterpret_cast<const char *>(picv.data()), sizeof(int)*MXPIC*MYPIC);
//        outfilet2.close();

        isreverse = 1;
        isdiffuse = 0;
        isdirect = 0;
//        project(pic0, vza_d, vaa_d, dint, pint, na, alist);
        m_pRT->project(modelio,vza_d,vaa_d,isreverse,isdirect,isdiffuse,pic0, dint,pint );

//        std::string outfileName3 = wdir+"/med/pic0.dat";
//        std::ofstream outfilet3(outfileName3.c_str(),std::ios::binary);
//        outfilet3.write(reinterpret_cast<const char *>(pic0.data()), sizeof(int)*MXPIC*MYPIC);
//        outfilet3.close();
////        foreground(pic0, picv, vza_d, vaa_d, dinttemp, na, alist);
//        m_pRT->foreground(modelio,pic0,picv,vza_d,vaa_d,isdirect, dint );
//        std::string outfileName4 = wdir+"/med/picv2.dat";
//        std::ofstream outfilet4(outfileName4.c_str(),std::ios::binary);
//        outfilet4.write(reinterpret_cast<const char *>(picv.data()), sizeof(int)*MXPIC*MYPIC);
//        outfilet4.close();


        ////vs: the sunlit and shaded proportion of visible polygon;
        //这里是计算光照可视面元比例情况；vs为每个面元的光照比例；
        std::fill(vs.begin(),vs.end(),0);
        hist(modelio,vza_d,vaa_d,sza_d,saa_d,picv,pics,vs);


        memset(radsum,0,n_wave*sizeof(float));
        memset(radtemp,0,n_wave*sizeof(float));


        //float radsum = 0;
        float psum = 0;

        for (int i = 0; i < MXPIC;i++)
            for (int j = 0; j < MYPIC;j++)
            {
                //rad_[i*MYPIC + j] = 0;
                int ics = picv[i*MYPIC + j];
                if (ics == 0) continue;
                int k = abs(ics) - 1;
                //看看大于0和小于0的情况，偶数是A面，奇数是B面；
                //这里并没有这个观测方向上边区分，都是单角度情况；
                //对于一定FOV下的，不能计算；
                if (ics < 0)
                {
                    for (int kband = 0; kband < n_wave; kband++)
                    {
                        float temp = (facetrts[k].radiosu[kband][1] * vs[k]
                                          + facetrts[k].radiosh[kband][1] * (1 - vs[k]));
                        radtemp[kband] = temp;
                        radsum[kband] = radsum[kband] + radtemp[kband];
                        if (radtemp[kband] > 10.5)
                            int t123 = 123;

                        rad_[(i*MYPIC + j)+kband*npic] = temp;
                    }
                }
                else
                {
                    /// positive side
                    for (int kband = 0; kband < n_wave; kband++)
                    {
                        float temp = (facetrts[k].radiosu[kband][0] * vs[k]
                                          + facetrts[k].radiosh[kband][0] * (1 - vs[k]));
                        radtemp[kband] = temp;
                        radsum[kband] = radsum[kband] + radtemp[kband];
                        if (radtemp[kband] < 7)
                            int t123 = 123;
                        rad_[(i*MYPIC + j)+kband*npic] = temp;

                    }
                }

                psum = psum + 1;

            }

        for (int kband = 0; kband < n_wave; kband++)
        {
            obs = 1.0 * radsum[kband] / psum;
            std::cout << obs << " ";
            obs_[kband + kang * n_wave] = obs;
        }

        std::cout<<std::endl;




        if(vza ==0 && vaa == 0){


        std::string outfileName11 = wdir+"/med/pics.dat";
        std::ofstream outfilet11(outfileName11.c_str(),std::ios::binary);
        outfilet11.write(reinterpret_cast<const char *>(pics.data()), sizeof(int)*MXPIC*MYPIC);
        outfilet11.close();
        std::string outfileName5 = wdir+"/med/picv.dat";
        std::ofstream outfilet5(outfileName5.c_str(),std::ios::binary);
        outfilet5.write(reinterpret_cast<const char *>(picv.data()), sizeof(int)*MXPIC*MYPIC);
        outfilet5.close();

            std::ostringstream oss_x;
            oss_x << std::fixed << std::setprecision(1) << std::setfill('0') << vza;
            std::ostringstream oss_y;
            oss_y << std::fixed << std::setprecision(1) << std::setfill('0') << vaa;
            std::string outfileName = wdir + "/med/obs_" + oss_x.str() + "_" + oss_y.str() + ".dat";
            std::ofstream outfilet1(outfileName.c_str(), std::ios::binary);
            outfilet1.write(reinterpret_cast<const char *>(rad_.data()), sizeof(float) * MXPIC * MYPIC * n_wave);
            outfilet1.close();

            std::string outhdrName = wdir + "/med/obs_" + oss_x.str() + "_" + oss_y.str() + ".hdr";
            std::ofstream outfile(outhdrName);
            if (outfile.is_open()) {
                outfile << "ENVI" << std::endl;
                outfile << "description = {" << std::endl;
                outfile << " File Imported into ENVI.} " << std::endl;
                outfile << "samples = " << MXPIC << std::endl;
                outfile << "lines   = " << MYPIC << std::endl;
                outfile << "bands   =  " << n_wave << std::endl;
                outfile << "header offset = 0" << std::endl;
                outfile << "file type = ENVI Standard" << std::endl;
                outfile << "data type = 4" << std::endl;
                outfile << "interleave = bsp" << std::endl;
                outfile << "sensor type = unknown" << std::endl;
                outfile << "byte order = 0" << std::endl;
                outfile << "wavelength units = Unknown" << std::endl;
                outfile.close();
            }
            outfile.close();
        }
    }

    std::string outfileName3 = wdir + "/med/multiobs" + affiliate + ".dat";
    /////	  remove(outfileName3.c_str());
    std::ofstream outfile3(outfileName3.c_str());
    if (outfile3.is_open())
    {
        for (int k = 0; k < n_angle; k++)
        {
            float vza = angles[k].vza;
            float vaa = angles[k].vaa;
            outfile3 << vza << " " << vaa << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile3 << obs_[k * n_wave + kband] << " ";
            outfile3 << std::endl;
        }

        outfile3.close();

    }

//    delete [] pint;
//    //delete [] rad_;
//    delete [] pic0;
//    delete [] pics;
//    delete [] picv;
//    delete [] dint;
//    delete [] pic;
//    delete [] vs;

    delete [] radsum;
    delete [] radtemp;




}

void Virtual::observe(std::shared_ptr<RadiosityEBIO> &modelio) {

    auto npoly = modelio->m_npoly;
    auto sza = modelio->sza;
    auto saa = modelio->saa;
    std::string wdir = modelio->projectDir;
    auto n_angle = modelio->n_angle;
    auto angles = modelio->angles;
    auto n_wave = modelio->n_wave;
    auto facetrts = modelio->m_facetio->facetRTs;
    auto facetvfs = modelio->m_facetio->facetVFs;
    auto meteos = modelio->meteos;
    auto knode = modelio->m_knode;
    float t = modelio->meteos[knode].t;

//    int *pint = new int[2*npoly];
//    int *dint = new int[2*npoly];
//    float *vs1 = new float[2*npoly];
//    float *vs = new float[2*npoly];
//    int *pic = new int[MXPIC*MYPIC];
    int npic = MXPIC * MYPIC;
    std::vector<int> pint = std::vector<int>(npoly*2,0);
    std::vector<int> dint = std::vector<int>(npoly*2,0);
    std::vector<int> pintsum = std::vector<int>(npoly*2,0);
    std::vector<int> dintsum = std::vector<int>(npoly*2,0);
    std::vector<int> pic = std::vector<int>(npic,0);
    std::vector<int> pic0 = std::vector<int>(npic,0);
    std::vector<int> na(npoly*(2*npoly+1),0);
    std::vector<int> alist(2*npoly);
    std::vector<int> pics = std::vector<int>(npic,0);
    std::vector<int> picv = std::vector<int>(npic,0);

    std::vector<int> picvvs = std::vector<int>(npic, 0);    // pixel 级别可视性

    std::vector<float> vs(npoly*2,0);
    std::vector<int> dinttemp;
    std::vector<float> rad_ = std::vector<float>(npic*n_wave,0);


//    int *na=NULL;
//    int *alist=NULL;
    float sza_d, saa_d;

    sza_d = sza*RD;
    saa_d = saa*RD;

    bool isreverse, isdirect, isdiffuse;

// 得到太阳方向的PIC阵列

//    int *pics = new int[MXPIC*MYPIC];
//    int *picv = new int[MXPIC*MYPIC]; //观测方向的投影矩阵；
//    int *pic0 = new int[MXPIC*MYPIC];
//    int *dinttemp = 0;

    std::fill(pics.begin(),pics.end(),0);
    std::fill(pic0.begin(),pic0.end(),0);
    isdirect = 0;
    isreverse = 0;
    isdiffuse = 0;
//    project(pics, sza_d, saa_d, dint, pint, na, alist);
    m_pRT->project(modelio,sza_d,saa_d,isreverse,isdirect,isdiffuse,pics, dint,pint);

    isreverse = 1;
    isdiffuse = 0;
    isdirect = 0;
//    project(pic0, sza_d, saa_d, dint, pint, na, alist);
    m_pRT->project(modelio,sza_d,saa_d,isreverse,isdirect,isdiffuse,pic0, dint,pint );
//    foreground(pic0, pics, sza_d, saa_d, dinttemp, na, alist);
    m_pRT->foreground(modelio,pic0,pics,sza_d,saa_d,isdirect, dinttemp );


    //string outfileName1 = wdir+"/Data1/pics.dat";
    //ofstream outfilet1(outfileName1.c_str(),ios::binary);
    //outfilet1.write(reinterpret_cast<const char *>(pics), sizeof(int)*MXPIC*MYPIC);
    //outfilet1.close();

    //float *rad_ = new float[MXPIC*MYPIC];


    float obs;
    float* radsum = new float[n_wave];
    float* radtemp = new float[n_wave];
    //float *rad_ = new float[MXPIC*MYPIC];
    float obs_[500];
    char bvza[20], bvaa[20];
    for(int kang=0;kang<n_angle;kang++)
    {
        float vza = angles[kang].vza;
        float vaa = angles[kang].vaa;
        float vza_d = vza*RD;
        float vaa_d = vaa*RD;
        std::cout << "   " << kang << " in " << n_angle << "  VZA: " << vza << "  VAA: " << vaa << " SZA:  " << sza << " SAA: " << saa << std::endl;


        sprintf(bvza, "%d", int(vza));
        sprintf(bvaa, "%d", int(vaa));

        ///////	 得到观测方向的pic阵列
        isdirect = 0;
        isreverse = 0;
        isdiffuse = 0;

        std::fill(picv.begin(),picv.end(),0);
        std::fill(pic0.begin(),pic0.end(),0);
        std::fill(dint.begin(),dint.end(),0);
        std::fill(pint.begin(),pint.end(),0);
//        project(picv, vza_d, vaa_d, dint, pint, na, alist);
        m_pRT->project(modelio,vza_d,vaa_d,isreverse,isdirect,isdiffuse,picv, dint,pint );

        isreverse = 1;
        isdiffuse = 0;
        isdirect = 0;
//        project(pic0, vza_d, vaa_d, dint, pint, na, alist);
        m_pRT->project(modelio,vza_d,vaa_d,isreverse,isdirect,isdiffuse,pic0, dint,pint );
//        foreground(pic0, picv, vza_d, vaa_d, dinttemp, na, alist);
        m_pRT->foreground(modelio,pic0,picv,vza_d,vaa_d,isdirect, dint );



        ////vs: the sunlit and shaded proportion of visible polygon;
        //这里是计算光照可视面元比例情况；vs为每个面元的光照比例；
        std::fill(vs.begin(),vs.end(),0);

        // 面元级别光照可视比例计算结果
        // hist(modelio,vza_d,vaa_d,sza_d,saa_d,picv,pics,vs);

        // pixel级别是否光照 计算结果
        hist(modelio, vza_d,vaa_d,sza_d,saa_d,picv,pics, picvvs);


        memset(radsum,0,n_wave*sizeof(float));
        memset(radtemp,0,n_wave*sizeof(float));


        //float radsum = 0;
        float psum = 0;

        for (int i = 0; i < MXPIC;i++)
            for (int j = 0; j < MYPIC;j++)
            {
                /// 面元尺度上的变化
                // //rad_[i*MYPIC + j] = 0;
                // int ics = picv[i*MYPIC + j];
                // if (ics == 0) continue;
                // int k = abs(ics) - 1;
                // //看看大于0和小于0的情况，偶数是A面，奇数是B面；
                // //这里并没有这个观测方向上边区分，都是单角度情况；
                // //对于一定FOV下的，不能计算；
                // if (ics < 0)
                // {
                //     for (int kband = 0; kband < n_wave; kband++)
                //     {
                //         float temp = (facetrts[k].radiosu[kband][1] * vs[k]
                //                       + facetrts[k].radiosh[kband][1] * (1 - vs[k]));
                //         radtemp[kband] = temp;
                //         radsum[kband] = radsum[kband] + radtemp[kband];
                //         if (temp < 0)
                //             int t123 = 123;
                //         rad_[(i*MYPIC + j)+kband*npic] = temp;
                //     }
                // }
                // else
                // {
                //     /// positive side
                //     for (int kband = 0; kband < n_wave; kband++)
                //     {
                //         float temp = (facetrts[k].radiosu[kband][0] * vs[k]
                //                       + facetrts[k].radiosh[kband][0] * (1 - vs[k]));
                //         radtemp[kband] = temp;
                //         radsum[kband] = radsum[kband] + radtemp[kband];
                //         if (temp < 0)
                //             int t123 = 123;
                //
                //         rad_[(i*MYPIC + j)+kband*npic] = temp;
                //     }
                // }

                // 像元尺度上的变化
                int ics = picv[i*MYPIC + j];
                if (ics == 0) continue;
                int k = abs(ics) - 1;
                for (int kband = 0; kband < n_wave; kband++)
                {
                    // if (picvvs[i*MYPIC + j] > 0)
                    // {
                    //     int a = 0;
                    // }
                    // float temp = (facetrts[k].radiosu[kband][0] * picvvs[i*MYPIC + j]
                    //               + facetrts[k].radiosh[kband][0] * (1 - picvvs[i*MYPIC + j]));
                    // radtemp[kband] = temp;
                    // radsum[kband] = radsum[kband] + radtemp[kband];


                    float temp;
                    if (picvvs[i*MYPIC + j] == 1)
                    {
                        temp = facetrts[k].radiosu[kband][0];
                        radsum[kband] = radsum[kband] + facetrts[k].radiosu[kband][0];
                    }
                    else
                    {
                        temp = facetrts[k].radiosh[kband][0];
                        radsum[kband] = radsum[kband] + facetrts[k].radiosh[kband][0];
                    }


                    if (temp < 0)
                        int t123 = 123;

                    rad_[(i*MYPIC + j)+kband*npic] = temp;
                }

                psum = psum + 1;

            }

        for (int kband = 0; kband < n_wave; kband++)
        {
            obs = 1.0 * radsum[kband] / psum;
            std::cout << obs << " ";
            obs_[kband + kang * n_wave] = obs;
        }

        std::cout<<std::endl;


        std::ostringstream  oss_z;
        oss_z << std::fixed << std::setprecision(2)<<std::setfill('0')<<t;
        std::ostringstream  oss_x;
        oss_x << std::fixed << std::setprecision(1)<<std::setfill('0')<<vza;
        std::ostringstream  oss_y;
        oss_y << std::fixed << std::setprecision(1)<<std::setfill('0')<<vaa;
        std::string outfileName = wdir+"/med/obs_"+oss_z.str() +"_"+oss_x.str()+ "_" +oss_y.str()+".dat";
        std::ofstream outfilet1(outfileName.c_str(),std::ios::binary);
        outfilet1.write(reinterpret_cast<const char *>(rad_.data()), sizeof(float)*MXPIC*MYPIC*n_wave);
        outfilet1.close();
        std::string outhdrName =wdir+"/med/obs_"+oss_z.str() +"_"+oss_x.str()+ "_" +oss_y.str()+".hdr";
        std::ofstream outfile(outhdrName);
        if (outfile.is_open())
        {
            outfile << "ENVI" << std::endl;
            outfile << "description = {" << std::endl;
            outfile << " File Imported into ENVI.} " << std::endl;
            outfile << "samples = " << MXPIC << std::endl;
            outfile << "lines   = " << MYPIC << std::endl;
            outfile << "bands   =  " << n_wave << std::endl;
            outfile << "header offset = 0" << std::endl;
            outfile << "file type = ENVI Standard" << std::endl;
            outfile << "data type = 4" << std::endl;
            outfile << "interleave = bsp" << std::endl;
            outfile << "sensor type = unknown" << std::endl;
            outfile << "byte order = 0" << std::endl;
            outfile << "wavelength units = Unknown" << std::endl;
            outfile.close();
        }
        outfile.close();


    }

    std::string outfileName3 = wdir + "/med/multiobs" + affiliate + ".dat";
    /////	  remove(outfileName3.c_str());
    std::ofstream outfile3(outfileName3.c_str(),std::ios::app);
    if (outfile3.is_open())
    {
        for (int k = 0; k < n_angle; k++)
        {
            float vza = angles[k].vza;
            float vaa = angles[k].vaa;
            outfile3 << t <<" " <<vza << " " << vaa << " ";
            for (int kband = 0; kband < n_wave; kband++)
                outfile3 << obs_[k * n_wave + kband] << " ";
            outfile3 << std::endl;
        }
        outfile3.close();
    }

//    delete [] pint;
//    //delete [] rad_;
//    delete [] pic0;
//    delete [] pics;
//    delete [] picv;
//    delete [] dint;
//    delete [] pic;
//    delete [] vs;

    delete [] radsum;
    delete [] radtemp;


}


void Virtual::hist(std::shared_ptr<RadiosityIO> &mio,float vza_d,float vaa_d,float sza_d,float saa_d,
                   std::vector<int> &picv,std::vector<int> &pics, std::vector<float> &vs)
{
    auto npoly = mio->m_npoly;
    auto a = mio->m_scenescale.a;
    auto b = mio->m_scenescale.b;
    auto c = mio->m_scenescale.c;
    auto d = mio->m_scenescale.d;
    auto delx = mio->m_scenescale.delx;
    auto dely = mio->m_scenescale.dely;
    auto xvw = mio->m_xvw;
    auto &facets = mio->m_facetio->facets;



    float azim_d;
    float zen_d;
    float xhv[3],yhv[3],zhv[3];
    int k2,k3;
    glm::vec3 xhs,yhs,zhs;
    //得到转化矩阵；
    azim_d = vaa_d;
    zen_d = vza_d;
    xhv[0] = -sin(azim_d);
    xhv[1] = cos(azim_d);
    xhv[2] = 0;
    yhv[0] = -cos(zen_d)*cos(azim_d);
    yhv[1] = -cos(zen_d)*sin(azim_d);
    yhv[2] = sin(zen_d);
    zhv[0] = sin(zen_d)*cos(azim_d);
    zhv[1] = sin(zen_d)*sin(azim_d);
    zhv[2] = cos(zen_d);
    //得到转化矩阵；
    azim_d = saa_d;
    zen_d = sza_d;
    xhs[0] = -sin(azim_d);
    xhs[1] = cos(azim_d);
    xhs[2] = 0;
    yhs[0] = -cos(zen_d)*cos(azim_d);
    yhs[1] = -cos(zen_d)*sin(azim_d);
    yhs[2] = sin(zen_d);
    zhs[0] = sin(zen_d)*cos(azim_d);
    zhs[1] = sin(zen_d)*sin(azim_d);
    zhs[2] = cos(zen_d);


    //double vec[3];



    int *irea = new int[4*npoly];//面元光照计数；
    int *irea2 = new int[4*npoly]; //面元阴影计数；
    int *indx = new int[4*npoly];  //转换到范围外部之后，就略过，略过的指示；
    for(int i=0;i<4*npoly;i++)
    {
        irea[i] = 0; irea2[i] = 0; indx[i] = 0;
    }

    //这部分是计算投影转换的转换因子，是用于fangshe
    double roundd=1.0;
    int izero=0,ibig=0,itry=0,jza1,kval;
    float ba = b/a;
    float da = d/a;
    float aa=a,bb=b,dd=d;
    float xspmax = -1, yspmax = -1,ixzero = 0, iyzero = 0;
    glm::vec3 point,point1;
    float pt[2][3];
    float xx,yy,det,rdet,deti;
    glm::vec3 vec[3];
    float a0,a1,a2,b0,b1,b2,aa0,aa1,aa2,bb0,bb1,bb2,c0,c1,c2,d0,d1,d2,cc0,cc1,cc2,dd0,dd1,dd2;
    std::vector<float> px0(npoly,0), px1(npoly,0),px2(npoly,0);
    std::vector<float> py0(npoly,0), py1(npoly,0),py2(npoly,0);
    std::vector<int> xsp(npoly,0),ysp(npoly,0);
    int j,nl,jj;

    //通过3个点回归出？还是直接计算出仿射变换的系数；
    //每个面元都计算；
    nl = 3;
    for(int k=0;k<npoly;k++)
    {

        //首先得到面元的总体信息；
        //jj = ind[k];

        int mark=0;
        //坐标
        for(int iofs=0;iofs<nl;iofs++)
        {
            itry = itry+1;
            for(int ii=0;ii<3;ii++)
            {
                int jza = (ii+iofs)%nl;
                point[0] = facets[k].points[jza].x;
                point[1] = facets[k].points[jza].y;
                point[2] = facets[k].points[jza].z;
                xx=0;
                yy=0;
//                look(xvw,point,xhv,yhv,&xx,&yy);
                Utils::transform(xvw,point,xhv,yhv,xx,yy);
                if(roundd)
                {
                    kval = round(a*xx+b);
                    xx = (kval-b)/a;
                    kval = round(a*yy+d);
                    yy = (kval-d)/a;
                }
                pt[0][ii] = xx;
                pt[1][ii] = yy;

                if(ii == 0)
                {
                   //Utils::dvdiff(vec[0],point,xvw);
                   vec[0].x = point.x-xvw.x;
                    vec[0].y = point.y-xvw.y;
                    vec[0].z = point.z-xvw.z;
                    jza1 = jza;
                }
                else
                {
                    point1[0] = facets[k].points[jza1].x;
                    point1[1] = facets[k].points[jza1].y;
                    point1[2] = facets[k].points[jza1].z;
//                    Utils::dvdiff(vec[ii],point,point1);
                    vec[ii].x = point.x-point1.x;
                    vec[ii].y = point.y-point1.y;
                    vec[ii].z = point.z-point1.z;
                }
            }


            //从这里到最后就不知道在讲什么东西；
            //但是这里就只是在计算仿射变换的系数；
            //之前的办法，由于存在1个bug，现在用应该也是好的；
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
            det = (pt[0][1]-pt[0][0])*(pt[1][2]-pt[1][0]) -
                  (pt[1][1]-pt[1][0])*(pt[0][2]-pt[0][0]);
            rdet = det;
            if(abs(rdet) <= 0) continue;

            deti = 1.0/det;

            a0 = deti*(pt[1][0]*pt[0][2] - pt[0][0]*pt[1][2]);
            a1 = deti*(pt[1][2]-pt[1][0]);
            a2 = deti*(pt[0][0]-pt[0][2]);
            b0 = deti*(pt[0][0]*pt[1][1] - pt[1][0]*pt[0][1]);
            b1 = deti*(pt[1][0]-pt[1][1]);
            b2 = deti*(pt[0][1]-pt[0][0]);

            aa0 = aa*a0 - a1*bb - a2*dd;
            aa1 = a1;
            aa2 = a2;
            bb0 = aa*b0 - b1*bb - b2*dd;
            bb1 = b1;
            bb2 = b2;

            c0 = Utils::ddt(vec[0],xhs);
            c1 = Utils::ddt(vec[1],xhs);
            c2 = Utils::ddt(vec[2],xhs);
            d0 = Utils::ddt(vec[0],yhs);
            d1 = Utils::ddt(vec[1],yhs);
            d2 = Utils::ddt(vec[2],yhs);

            cc0 = aa*c0 + c1*aa0 + c2*bb0;
            cc1 = c1*aa1 + c2*bb1;
            cc2 = c1*aa2 + c2*bb2;
            dd0 = aa*d0 + d1*aa0 + d2*bb0;
            dd1 = d1*aa1 + d2*bb1;
            dd2 = d1*aa2 + d2*bb2;

            px0[k] = cc0+bb;
            px1[k] = cc1;
            px2[k] = cc2;
            py0[k] = dd0+dd;
            py1[k] = dd1;
            py2[k] = dd2;

            xsp[k] = int((abs(cc1)+abs(cc2))/2.+0.5 );
            ysp[k] = int((abs(dd1)+abs(dd2))/2.+0.5 );
            ///////////////////////////////////////////////////////////////////////////////////////////////

            if(xsp[k] == 0) ixzero=ixzero+1;
            if(ysp[k] == 0) iyzero=iyzero+1;

            xspmax = std::max(xspmax,float(xsp[k]));
            yspmax = std::max(yspmax,float(ysp[k]));

            //当存在不好的点的时候，就略过
            int xm = 10000;
            if(abs(px0[k]) < xm && abs(px1[k]) < xm && abs(px1[k]) < xm && abs(py0[k]) < xm && abs(py1[k]) < xm &&  abs(py1[k]) <xm)
            {
                mark=1;
                break;
            }
        }
        if (mark ==1) continue;
        indx[k]=1;
        izero = izero+1;
    }

    itry = itry-npoly;


    ///////////////////////////////////////////////////////////////////////


    //随后根据计算得到的投影矩阵，分别进行对照，看看是否是光照叶片；
    int isun = 0;
    int idark = 0;
    int fdark = 1;
    int ics=0;
    int xi,xj,ix,iy;
    int ix1,ix2,iy1,iy2;
    for(int i=0;i<MXPIC;i++)
        for(int j=0;j<MYPIC;j++)
        {
            //因为这里只有1面能够被阳光照射，因此光照并不用特别区分？
            ics = abs(picv[i*MYPIC+j]);
            if(ics <=0) continue;
            k2 = ics - 1;
            //if(k2 < 0) k2 = -k2;

            ////OK case
            if( k2 >= 0 && k2 <= npoly)
            {
                //这里是对于那些失败了的格网而言，1个光照1个阴影；
                if( indx[k2] ==1 )
                {
                    if(fdark ==1 )
                    {
                        idark = idark+1;
                        irea2[k2] = irea2[k2]+1;
                        fdark = 0;
                        //	pic0[i*MYPIC+j] = 1;
                        //	cout<<1<<endl;
                    }
                    else
                    {
                        isun = isun+1;
                        irea[k2] =  irea[k2] +1;
                        fdark = 1;
                        //	pic0[i*MYPIC + j] = 1;
                        //	cout<<2<<endl;
                    }
                    continue;
                }

                //计算得到光照部分的投影区域；
                xi = i;
                xj = j;
                ix = px0[k2] + px1[k2] * xi + px2[k2] * xj + 0.5;
                iy = py0[k2] + py1[k2] * xi + py2[k2] * xj + 0.5;
                ix = ix ;
                iy = iy ;

                //给出一定的范围；
                ix1 = ix-xsp[k2];
                ix2 = ix+xsp[k2];
                iy1 = iy-ysp[k2];
                iy2 = iy+ysp[k2];

                //判断在该范围内，是否是满足约束条件，如果满足就算，不满足就跳出；
                float  perc = 0.25;
                int ithere=0;
                int icnt=0;
                for (int kx = ix1; kx <= ix2; kx++)
                {
                    for (int ky = iy1; ky <= iy2; ky++)
                    {
                        if (kx < 0) continue;
                        if (ky < 0)  continue;
                        if (kx >= MXPIC)  continue;
                        if (ky >= MYPIC)  continue;
                        icnt = icnt + 1;
                        ics = abs(pics[(ky) + kx*MYPIC]);
                        k3 = ics - 1; //太阳光照的点；
                        if (k3 == k2) ithere = ithere + 1; //如果太阳光照和观测的是同样的面元，就累加；
                    }
                }

                //如果满足足够的点信息，就算作是光照，否者就是阴影，这里是大于0.25就算是光照？
                if(ithere >= perc*icnt)
                {
                    isun=isun+1;
                    irea[k2]=irea[k2]+1;
                    //	   pic0[i*MYPIC+j] = 1;
                }
                else
                {
                    irea2[k2]=irea2[k2]+1;
                    idark=idark+1;
                    //   pic0[i*MYPIC+j] = -1;
                }

            }
        }

    //float xx = 100.0*isun/(isun+idark);
    for(int i=0;i<npoly;i++)
    {
        if((irea[i]+irea2[i])==0)
        {
            vs[i]= 0;
            continue;
        }
        vs[i] = 1.0*(irea[i])/(irea[i]+irea2[i]);
    }



    //	string outfileName1 = wdir+"/Data1/pics.dat";
    //  ofstream outfilet1(outfileName1.c_str(),ios::binary);
    //outfilet1.write(reinterpret_cast<const char *>(pics), sizeof(int)*MXPIC*MYPIC);
    //outfilet1.close();


    //string outfileName2 = wdir+"/Data1/picv.dat";
    //   ofstream outfilet2(outfileName2.c_str(),ios::binary);
    //outfilet2.write(reinterpret_cast<const char *>(picv), sizeof(int)*MXPIC*MYPIC);
    //outfilet2.close();

    //string outfileName3 = wdir+"/Data1/pic0_.dat";
    //   ofstream outfilet3(outfileName3.c_str(),ios::binary);
    //outfilet3.write(reinterpret_cast<const char *>(pic0), sizeof(int)*MXPIC*MYPIC);
    //outfilet3.close();

    //string outfileName3 = wdir+"/Data1/multiobs"+affiliate+".dat";
    //    //  remove(outfileName3.c_str());
    //      ofstream outfile3(outfileName3.c_str(),ios::app);
    //      if (outfile3.is_open())
    //      {
    //	 for(int i=0;i<npoly;i++)
    //	{
    //		outfile3<<irea[i]<<"  "<<irea2[i]<<endl;
    //	 }
    //        //  outfile3<<<<" "<<vaa<<" "<<obs<<endl;
    //          outfile3.close();
    //      }


}


void Virtual::hist(std::shared_ptr<RadiosityEBIO> &mio,float vza_d,float vaa_d,float sza_d,float saa_d,
                   std::vector<int> &picv,std::vector<int> &pics, std::vector<float> &vs)
{
    auto npoly = mio->m_npoly;
    auto a = mio->m_scenescale.a;
    auto b = mio->m_scenescale.b;
    auto c = mio->m_scenescale.c;
    auto d = mio->m_scenescale.d;
    auto delx = mio->m_scenescale.delx;
    auto dely = mio->m_scenescale.dely;
    auto xvw = mio->m_xvw;
    auto &facets = mio->m_facetio->facets;



    float azim_d;
    float zen_d;
    float xhv[3],yhv[3],zhv[3];
    int k2,k3;
    glm::vec3 xhs,yhs,zhs;
    //得到转化矩阵；
    azim_d = vaa_d;
    zen_d = vza_d;
    xhv[0] = -sin(azim_d);
    xhv[1] = cos(azim_d);
    xhv[2] = 0;
    yhv[0] = -cos(zen_d)*cos(azim_d);
    yhv[1] = -cos(zen_d)*sin(azim_d);
    yhv[2] = sin(zen_d);
    zhv[0] = sin(zen_d)*cos(azim_d);
    zhv[1] = sin(zen_d)*sin(azim_d);
    zhv[2] = cos(zen_d);
    //得到转化矩阵；
    azim_d = saa_d;
    zen_d = sza_d;
    xhs[0] = -sin(azim_d);
    xhs[1] = cos(azim_d);
    xhs[2] = 0;
    yhs[0] = -cos(zen_d)*cos(azim_d);
    yhs[1] = -cos(zen_d)*sin(azim_d);
    yhs[2] = sin(zen_d);
    zhs[0] = sin(zen_d)*cos(azim_d);
    zhs[1] = sin(zen_d)*sin(azim_d);
    zhs[2] = cos(zen_d);


    //double vec[3];



    int *irea = new int[4*npoly];//面元光照计数；
    int *irea2 = new int[4*npoly]; //面元阴影计数；
    int *indx = new int[4*npoly];  //转换到范围外部之后，就略过，略过的指示；
    for(int i=0;i<4*npoly;i++)
    {
        irea[i] = 0; irea2[i] = 0; indx[i] = 0;
    }

    //这部分是计算投影转换的转换因子，是用于fangshe
    double roundd=1.0;
    int izero=0,ibig=0,itry=0,jza1,kval;
    float ba = b/a;
    float da = d/a;
    float aa=a,bb=b,dd=d;
    float xspmax = -1, yspmax = -1,ixzero = 0, iyzero = 0;
    glm::vec3 point,point1;
    float pt[2][3];
    float xx,yy,det,rdet,deti;
    glm::vec3 vec[3];
    float a0,a1,a2,b0,b1,b2,aa0,aa1,aa2,bb0,bb1,bb2,c0,c1,c2,d0,d1,d2,cc0,cc1,cc2,dd0,dd1,dd2;
    std::vector<float> px0(npoly,0), px1(npoly,0),px2(npoly,0);
    std::vector<float> py0(npoly,0), py1(npoly,0),py2(npoly,0);
    std::vector<int> xsp(npoly,0),ysp(npoly,0);
    int j,nl,jj;

    //通过3个点回归出？还是直接计算出仿射变换的系数；
    //每个面元都计算；
    nl = 3;
    for(int k=0;k<npoly;k++)
    {

        //首先得到面元的总体信息；
        //jj = ind[k];

        int mark=0;
        //坐标
        for(int iofs=0;iofs<nl;iofs++)
        {
            itry = itry+1;
            for(int ii=0;ii<3;ii++)
            {
                int jza = (ii+iofs)%nl;
                point[0] = facets[k].points[jza].x;
                point[1] = facets[k].points[jza].y;
                point[2] = facets[k].points[jza].z;
                xx=0;
                yy=0;
//                look(xvw,point,xhv,yhv,&xx,&yy);
                Utils::transform(xvw,point,xhv,yhv,xx,yy);
                if(roundd)
                {
                    kval = round(a*xx+b);
                    xx = (kval-b)/a;
                    kval = round(a*yy+d);
                    yy = (kval-d)/a;
                }
                pt[0][ii] = xx;
                pt[1][ii] = yy;

                if(ii == 0)
                {
                    Utils::dvdiff(vec[0],point,xvw);
                    jza1 = jza;
                }
                else
                {
                    point1[0] = facets[k].points[jza1].x;
                    point1[1] = facets[k].points[jza1].y;
                    point1[2] = facets[k].points[jza1].z;
                    Utils::dvdiff(vec[ii],point,point1);
                }
            }


            //从这里到最后就不知道在讲什么东西；
            //但是这里就只是在计算仿射变换的系数；
            //之前的办法，由于存在1个bug，现在用应该也是好的；
            ///////////////////////////////////////////////////////////////////////////////////////////////////////
            det = (pt[0][1]-pt[0][0])*(pt[1][2]-pt[1][0]) -
                  (pt[1][1]-pt[1][0])*(pt[0][2]-pt[0][0]);
            rdet = det;
            if(abs(rdet) <= 0) continue;

            deti = 1.0/det;

            a0 = deti*(pt[1][0]*pt[0][2] - pt[0][0]*pt[1][2]);
            a1 = deti*(pt[1][2]-pt[1][0]);
            a2 = deti*(pt[0][0]-pt[0][2]);
            b0 = deti*(pt[0][0]*pt[1][1] - pt[1][0]*pt[0][1]);
            b1 = deti*(pt[1][0]-pt[1][1]);
            b2 = deti*(pt[0][1]-pt[0][0]);

            aa0 = aa*a0 - a1*bb - a2*dd;
            aa1 = a1;
            aa2 = a2;
            bb0 = aa*b0 - b1*bb - b2*dd;
            bb1 = b1;
            bb2 = b2;

            c0 = Utils::ddt(vec[0],xhs);
            c1 = Utils::ddt(vec[1],xhs);
            c2 = Utils::ddt(vec[2],xhs);
            d0 = Utils::ddt(vec[0],yhs);
            d1 = Utils::ddt(vec[1],yhs);
            d2 = Utils::ddt(vec[2],yhs);

            cc0 = aa*c0 + c1*aa0 + c2*bb0;
            cc1 = c1*aa1 + c2*bb1;
            cc2 = c1*aa2 + c2*bb2;
            dd0 = aa*d0 + d1*aa0 + d2*bb0;
            dd1 = d1*aa1 + d2*bb1;
            dd2 = d1*aa2 + d2*bb2;

            px0[k] = cc0+bb;
            px1[k] = cc1;
            px2[k] = cc2;
            py0[k] = dd0+dd;
            py1[k] = dd1;
            py2[k] = dd2;

            xsp[k] = int((abs(cc1)+abs(cc2))/2.+0.5 );
            ysp[k] = int((abs(dd1)+abs(dd2))/2.+0.5 );
            ///////////////////////////////////////////////////////////////////////////////////////////////

            if(xsp[k] == 0) ixzero=ixzero+1;
            if(ysp[k] == 0) iyzero=iyzero+1;

            xspmax = std::max(xspmax,float(xsp[k]));
            yspmax = std::max(yspmax,float(ysp[k]));

            //当存在不好的点的时候，就略过
            int xm = 10000;
            if(abs(px0[k]) < xm && abs(px1[k]) < xm && abs(px1[k]) < xm && abs(py0[k]) < xm && abs(py1[k]) < xm &&  abs(py1[k]) <xm)
            {
                mark=1;
                break;
            }
        }
        if (mark ==1) continue;
        indx[k]=1;
        izero = izero+1;
    }

    itry = itry-npoly;


    ///////////////////////////////////////////////////////////////////////


    //随后根据计算得到的投影矩阵，分别进行对照，看看是否是光照叶片；
    int isun = 0;
    int idark = 0;
    int fdark = 1;
    int ics=0;
    int xi,xj,ix,iy;
    int ix1,ix2,iy1,iy2;
    for(int i=0;i<MXPIC;i++)
        for(int j=0;j<MYPIC;j++)
        {
            //因为这里只有1面能够被阳光照射，因此光照并不用特别区分？
            ics = abs(picv[i*MYPIC+j]);
            if(ics <=0) continue;
            k2 = ics - 1;
            //if(k2 < 0) k2 = -k2;

            ////OK case
            if( k2 >= 0 && k2 <= npoly)
            {
                //这里是对于那些失败了的格网而言，1个光照1个阴影；
                if( indx[k2] ==1 )
                {
                    if(fdark ==1 )
                    {
                        idark = idark+1;
                        irea2[k2] = irea2[k2]+1;
                        fdark = 0;
                        //	pic0[i*MYPIC+j] = 1;
                        //	cout<<1<<endl;
                    }
                    else
                    {
                        isun = isun+1;
                        irea[k2] =  irea[k2] +1;
                        fdark = 1;
                        //	pic0[i*MYPIC + j] = 1;
                        //	cout<<2<<endl;
                    }
                    continue;
                }

                //计算得到光照部分的投影区域；
                xi = i;
                xj = j;
                ix = px0[k2] + px1[k2] * xi + px2[k2] * xj + 0.5;
                iy = py0[k2] + py1[k2] * xi + py2[k2] * xj + 0.5;
                ix = ix ;
                iy = iy ;

                //给出一定的范围；
                ix1 = ix-xsp[k2];
                ix2 = ix+xsp[k2];
                iy1 = iy-ysp[k2];
                iy2 = iy+ysp[k2];

                //判断在该范围内，是否是满足约束条件，如果满足就算，不满足就跳出；
                float  perc = 0.25;
                int ithere=0;
                int icnt=0;
                for (int kx = ix1; kx <= ix2; kx++)
                {
                    for (int ky = iy1; ky <= iy2; ky++)
                    {
                        if (kx < 0) continue;
                        if (ky < 0)  continue;
                        if (kx >= MXPIC)  continue;
                        if (ky >= MYPIC)  continue;
                        icnt = icnt + 1;
                        ics = abs(pics[(ky) + kx*MYPIC]);
                        k3 = ics - 1; //太阳光照的点；
                        if (k3 == k2) ithere = ithere + 1; //如果太阳光照和观测的是同样的面元，就累加；
                    }
                }

                //如果满足足够的点信息，就算作是光照，否者就是阴影，这里是大于0.25就算是光照？
                if(ithere >= perc*icnt)
                {
                    isun=isun+1;
                    irea[k2]=irea[k2]+1;
                    //	   pic0[i*MYPIC+j] = 1;
                }
                else
                {
                    irea2[k2]=irea2[k2]+1;
                    idark=idark+1;
                    //   pic0[i*MYPIC+j] = -1;
                }

            }
        }

    //float xx = 100.0*isun/(isun+idark);
    for(int i=0;i<npoly;i++)
    {
        if((irea[i]+irea2[i])==0)
        {
            vs[i]= 0;
            continue;
        }
        vs[i] = 1.0*(irea[i])/(irea[i]+irea2[i]);
    }



    //	string outfileName1 = wdir+"/Data1/pics.dat";
    //  ofstream outfilet1(outfileName1.c_str(),ios::binary);
    //outfilet1.write(reinterpret_cast<const char *>(pics), sizeof(int)*MXPIC*MYPIC);
    //outfilet1.close();


    //string outfileName2 = wdir+"/Data1/picv.dat";
    //   ofstream outfilet2(outfileName2.c_str(),ios::binary);
    //outfilet2.write(reinterpret_cast<const char *>(picv), sizeof(int)*MXPIC*MYPIC);
    //outfilet2.close();

    //string outfileName3 = wdir+"/Data1/pic0_.dat";
    //   ofstream outfilet3(outfileName3.c_str(),ios::binary);
    //outfilet3.write(reinterpret_cast<const char *>(pic0), sizeof(int)*MXPIC*MYPIC);
    //outfilet3.close();

    //string outfileName3 = wdir+"/Data1/multiobs"+affiliate+".dat";
    //    //  remove(outfileName3.c_str());
    //      ofstream outfile3(outfileName3.c_str(),ios::app);
    //      if (outfile3.is_open())
    //      {
    //	 for(int i=0;i<npoly;i++)
    //	{
    //		outfile3<<irea[i]<<"  "<<irea2[i]<<endl;
    //	 }
    //        //  outfile3<<<<" "<<vaa<<" "<<obs<<endl;
    //          outfile3.close();
    //      }


}

void Virtual::hist(std::shared_ptr<RadiosityEBIO>& mio, float vza_d, float vaa_d, float sza_d, float saa_d,
    std::vector<int>& picv, std::vector<int>& pics, std::vector<int>& picvvs)
{
    auto npoly = mio->m_npoly;
    auto &facets = mio->m_facetio->facets;
    auto a = mio->m_scenescale.a;
    auto b = mio->m_scenescale.b;
    auto c = mio->m_scenescale.c;
    auto d = mio->m_scenescale.d;
    auto xvw = mio->m_xvw;

    // 空间点到观测图像的转换矩阵
    float xhv[3], yhv[3], zhv[3];
    float zen_d = vza_d;
    float azim_d = vaa_d;
    xhv[0] = -sin(azim_d);
    xhv[1] = cos(azim_d);
    xhv[2] = 0;
    yhv[0] = -cos(zen_d)*cos(azim_d);
    yhv[1] = -cos(zen_d)*sin(azim_d);
    yhv[2] = sin(zen_d);
    zhv[0] = sin(zen_d)*cos(azim_d);
    zhv[1] = sin(zen_d)*sin(azim_d);
    zhv[2] = cos(zen_d);

    // 空间点到太阳图像的转换矩阵
    float xhs[3], yhs[3], zhs[3];
    azim_d = saa_d;
    zen_d = sza_d;
    xhs[0] = -sin(azim_d);
    xhs[1] = cos(azim_d);
    xhs[2] = 0;
    yhs[0] = -cos(zen_d)*cos(azim_d);
    yhs[1] = -cos(zen_d)*sin(azim_d);
    yhs[2] = sin(zen_d);
    zhs[0] = sin(zen_d)*cos(azim_d);
    zhs[1] = sin(zen_d)*sin(azim_d);
    zhs[2] = cos(zen_d);

    // 定义一个 3x3 矩阵
    Eigen::Matrix3d matv;
    matv << xhv[0], xhv[1], xhv[2],
        yhv[0], yhv[1], yhv[2],
        zhv[0], zhv[1], zhv[2];
    // 计算逆矩阵
    Eigen::Matrix3d invMatv = matv.inverse();

    Eigen::Matrix3d mats;
    mats << xhs[0], xhs[1], xhs[2],
        yhs[0], yhs[1], yhs[2],
        zhs[0], zhs[1], zhs[2];

    for (auto kp=0; kp < picv.size(); kp++)
    {
        // view facet index, 0 is nothing, value is (facetId - 1)
        int indv = picv[kp];

        if (indv > 0)
        {
            // 在观测图像上的坐标
            int xxv = int(kp / MYPIC);
            int yyv = kp % MYPIC;


            Eigen::Vector3d posV;
            posV << (xxv - mio->m_scenescale.b) / mio->m_scenescale.a,
                    (yyv - mio->m_scenescale.d)/mio->m_scenescale.c,
                    0;

            // 执行矩阵乘法
            Eigen::Vector3d pos = invMatv * posV;
            Eigen::Vector3d posS = mats * pos;

            int xxs, yys;
            xxs = posS[0] * mio->m_scenescale.a + mio->m_scenescale.b;
            yys = posS[1] * mio->m_scenescale.c + mio->m_scenescale.d;

            int inds = xxs * MYPIC + yys;
            if (inds >= 0)
            {
                int picsInd = pics[inds];
                if (picsInd == indv)
                {
                    picvvs[kp] = 1;
                }
                else
                {
                    picvvs[kp] = 0;
                }
            }
        }
    }
}










